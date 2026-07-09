#pragma once

// ============================================================================
//  OutputTransformer.h — output transformer: core saturation + bandwidth (§7).
//
//  Two effects to capture:
//    * Core saturation / hysteresis. A real OT core follows a B-H loop: the
//      magnetisation lags the drive (hysteresis) and flattens as the core
//      saturates, adding low-order harmonics that bloom on loud low notes.
//    * Bandwidth limiting. Finite primary inductance rolls off the lows;
//      leakage inductance + winding capacitance roll off (and slightly resonate)
//      the highs.
//
//  A full Jiles-Atherton ODE solve (Newton per sample) is the "correct" model
//  and the intended upgrade (§7, §11). For v1 this is a lighter but genuinely
//  hysteretic stand-in: an anhysteretic saturating curve plus a first-order
//  "play"/backlash operator that opens a rate-independent B-H loop, bracketed
//  by the LF/HF bandwidth filters. It is honest about being an approximation.
// ============================================================================

#include "DspUtil.h"
#include "Biquad.h"

namespace tekk
{

class OutputTransformer
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        // Finite primary inductance → LF roll-off; leakage/self-capacitance →
        // HF roll-off with a mild resonance.
        lfRolloff.setHighPass (sr, 22.0, 0.707);
        hfRolloff.setLowPass  (sr, 11000.0, 1.1);
        reset();
    }

    void reset() noexcept
    {
        lfRolloff.reset();
        hfRolloff.reset();
        mag = 0.0f;
    }

    void setDrive     (float d) noexcept { drive = d; }      // how hard the core is pushed
    void setSaturation (float s) noexcept { sat = math::clampf (s, 0.05f, 4.0f); }
    void setHysteresis (float h) noexcept { play = math::clampf (h, 0.0f, 0.5f); }

    inline float process (float x) noexcept
    {
        x = lfRolloff.process (x);

        const float in = drive * x;

        // Anhysteretic (loss-free) magnetisation curve: saturating tanh.
        const float anhyst = std::tanh (sat * in) / sat;

        // Play/backlash operator: the magnetisation can only move toward the
        // target once it has taken up the "slack" band of width `play`. This is
        // what opens a hysteresis loop (output depends on history/direction).
        if (anhyst > mag + play)      mag = anhyst - play;
        else if (anhyst < mag - play) mag = anhyst + play;
        mag = math::flushDenorm (mag);

        float y = hfRolloff.process (mag);
        return y;
    }

private:
    double sampleRate = 44100.0;
    // `play` is a subtle backlash band (opens the B-H loop). Kept small so small
    // signals pass ~linearly — a large deadband would behave like crossover
    // distortion and wrongly attenuate quiet passages.
    float drive = 1.0f, sat = 1.0f, play = 0.006f, mag = 0.0f;
    Biquad lfRolloff, hfRolloff;
};

} // namespace tekk
