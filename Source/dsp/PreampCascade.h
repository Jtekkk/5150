#pragma once

// ============================================================================
//  PreampCascade.h — the cascaded 12AX7 front end (§2, §4).
//
//  Rhythm channel ≈ 3 gain stages; Lead channel ≈ 5 gain stages — the extra
//  two stages are where the extreme gain lives. Later stages are hit harder,
//  clip more, and use tighter coupling caps (accumulated gain would turn a
//  fat low end to mush otherwise). Bright and Crunch reshape the Rhythm channel.
// ============================================================================

#include <cstddef>   // std::size_t (used in the stage-array index casts below)

#include "TubeStage.h"

namespace tekk
{

enum class Channel { Rhythm = 0, Lead = 1 };

class PreampCascade
{
public:
    static constexpr int kMaxStages = 5;

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        brightCap.reset();
        for (auto& s : stages) s.prepare (sr);
        configure();
    }

    void reset() noexcept
    {
        brightCap.reset();
        for (auto& s : stages) s.reset();
    }

    void setChannel (Channel c) noexcept  { channel = c;    configure(); }
    void setPreGain (float knob0to10) noexcept { preGain = knob0to10; configure(); }
    void setBright  (bool on) noexcept    { bright = on;    configure(); }
    void setCrunch  (bool on) noexcept    { crunch = on;    configure(); }

    inline float process (float x) noexcept
    {
        // Rhythm "bright" bleeds highs around the first gain pot.
        if (channel == Channel::Rhythm && bright)
            x = brightCap.process (x);

        for (int i = 0; i < activeStages; ++i)
            x = stages[(size_t) i].process (x);

        return x;
    }

    int  getActiveStages() const noexcept { return activeStages; }

private:
    void configure() noexcept
    {
        activeStages = (channel == Channel::Lead) ? 5 : 3;

        // Master preamp drive: musical 0..10 taper. The knob mostly loads the
        // FIRST couple of stages; the tail stages sit deep in saturation.
        const float g = preGain / 10.0f;                 // 0..1
        const float frontDrive = 1.0f + 14.0f * g * g;   // stage-1 drive swing
        const float crunchBoost = crunch ? 1.6f : 1.0f;

        // Per-stage voicing templates. Drive climbs, coupling caps tighten and
        // cathode lift eases as we move down the chain.
        static constexpr float driveMul   [kMaxStages] = { 1.00f, 0.85f, 0.80f, 0.78f, 0.75f };
        static constexpr float couplingHz [kMaxStages] = {  9.0f, 22.0f, 33.0f, 47.0f, 68.0f };
        static constexpr float cathodeF   [kMaxStages] = { 250.0f,220.0f,180.0f,150.0f,120.0f };
        static constexpr float cathodeDb  [kMaxStages] = {  3.0f,  2.6f,  2.2f,  1.8f,  1.5f };
        static constexpr float biasSh     [kMaxStages] = { 0.05f, 0.10f, 0.16f, 0.22f, 0.30f };

        for (int i = 0; i < kMaxStages; ++i)
        {
            TubeStageParams p;
            const bool isFront = (i == 0);
            p.drive         = (isFront ? frontDrive : (2.2f + 2.0f * g)) * driveMul[i] * crunchBoost;
            p.makeup        = isFront ? 0.42f : 0.40f;
            p.couplingHz    = couplingHz[i];
            p.cathodeFreq   = cathodeF[i];
            p.cathodeGainDb = cathodeDb[i];
            p.gridStopperHz = 24000.0f;
            p.biasShift     = biasSh[i];
            p.biasReleaseMs = 26.0f + 6.0f * i;
            // Slightly stronger asymmetry deeper in the chain → more even-order
            // grit and a firmer gate on the tail stages.
            p.kp            = 1.25f + 0.06f * i;
            p.kn            = 0.74f - 0.02f * i;
            stages[(size_t) i].setParams (p);
        }

        brightCap.setHighShelf (sampleRate, 1500.0, bright ? 6.0 : 0.0, 0.7);
    }

    double sampleRate = 44100.0;
    Channel channel = Channel::Lead;
    float preGain = 5.0f;
    bool  bright = false, crunch = false;
    int   activeStages = 5;

    TubeStage stages[kMaxStages];
    Biquad    brightCap;
};

} // namespace tekk
