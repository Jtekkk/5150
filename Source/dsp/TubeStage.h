#pragma once

// ============================================================================
//  TubeStage.h — one cascaded 12AX7 preamp gain stage (§4).
//
//  Signal path within a stage:
//     grid-stopper LPF → drive → (fixed bias + envelope bias-shift)
//        → ADAA1 asymmetric triode saturation
//        → coupling HPF (removes DC / shapes low end)
//        → cathode-bypass low shelf (low-mid emphasis)
//        → makeup
//
//  The two behaviours that make a real high-gain preamp feel alive:
//    * Asymmetric clipping (TriodeShaper) → even harmonics, per-stage voicing.
//    * Coupling-cap bias shift → "blocking distortion". Under sustained heavy
//      signal the coupling cap charges and drags the operating point toward
//      cutoff, so palm mutes get tight/gated and the amp "farts out" when
//      pushed. We approximate the cap voltage with a slow envelope and use it
//      to offset the bias (§4, §15 — tune against a palm-mute reference).
//
//  Each stage owns its own parameters so stages can be voiced independently
//  against the reference.
// ============================================================================

#include "ADAA.h"
#include "Shapers.h"
#include "OnePole.h"
#include "Biquad.h"
#include "EnvelopeFollower.h"

namespace tekk
{

struct TubeStageParams
{
    float drive          = 2.0f;    // linear gain into the shaper
    float makeup         = 0.5f;    // post gain to keep unity-ish staging
    float gridStopperHz  = 22000.0f;// input low-pass (Miller / grid-stopper)
    float couplingHz     = 12.0f;   // interstage high-pass corner
    float cathodeFreq    = 250.0f;  // cathode-bypass low-shelf corner
    float cathodeGainDb  = 3.0f;    // low-mid emphasis from the bypass cap
    float fixedBias      = 0.0f;    // static operating-point offset
    float biasShift      = 0.0f;    // depth of envelope-driven bias drift
    float biasReleaseMs  = 28.0f;   // coupling-cap "discharge" time constant
    float kp             = 1.30f;   // positive-side (grid conduction) hardness
    float kn             = 0.72f;   // negative-side (cutoff) hardness
};

class TubeStage
{
public:
    void setParams (const TubeStageParams& p) noexcept
    {
        params = p;
        gridLP.setCutoff (p.gridStopperHz);
        couplingHP.setCutoff (p.couplingHz);
        cathode.setLowShelf (sampleRate, p.cathodeFreq, p.cathodeGainDb, 0.8);
        biasEnv.setTimes (2.0, p.biasReleaseMs);
        shaper().kp = p.kp;
        shaper().kn = p.kn;
    }

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        gridLP.prepare (sr);
        couplingHP.prepare (sr);
        biasEnv.prepare (sr);
        setParams (params);
        reset();
    }

    void reset() noexcept
    {
        gridLP.reset();
        couplingHP.reset();
        cathode.reset();
        biasEnv.reset();
        adaa.reset();
    }

    inline float process (float x) noexcept
    {
        x = gridLP.process (x);
        const float driven = params.drive * x;

        // Coupling-cap bias drift: envelope of the driven signal pulls the
        // operating point toward cutoff → blocking distortion / gated feel.
        const float env  = biasEnv.process (driven);
        const float bias = params.fixedBias - params.biasShift * env;

        float y = adaa.process (driven + bias);
        y = couplingHP.process (y);       // strips the DC bias, tightens lows
        y = cathode.process (y);          // cathode-bypass low-mid lift
        return params.makeup * y;
    }

    const TubeStageParams& getParams() const noexcept { return params; }

private:
    TriodeShaper& shaper() noexcept { return adaa.nonlinearity(); }

    double sampleRate = 44100.0;
    TubeStageParams params;

    OnePoleLP  gridLP;
    OnePoleHP  couplingHP;
    Biquad     cathode;
    EnvelopeFollower biasEnv;
    ADAA1<TriodeShaper> adaa;
};

} // namespace tekk
