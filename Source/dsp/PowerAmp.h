#pragma once

// ============================================================================
//  PowerAmp.h — push-pull class-AB 6L6 power stage with supply sag (§7).
//
//  Elements modelled here:
//    * Push-pull class-AB nonlinearity: roughly symmetric clipping (both tubes
//      combined) plus a small crossover region near zero where both tubes idle
//      close to cutoff. Modelled as a smooth crossover "deadzone" ahead of a
//      symmetric soft clip. The clip uses ADAA2 (second-order) — the crossover
//      kink is exactly the sort of low-level detail that benefits from the extra
//      aliasing suppression.
//    * Power-supply sag: under heavy transients the B+ rail droops, so the
//      transient attack is compressed and then "blooms" back as the reservoir
//      recharges. This is the single biggest feel element (§7). Implemented as
//      an envelope that reduces available drive/headroom with an RC recovery
//      time matched to the supply reservoir.
//
//  The output transformer (core saturation + bandwidth) and the presence/
//  resonance NFB shaping are separate blocks (OutputTransformer / Negative
//  Feedback) so the chain stays composable.
// ============================================================================

#include "ADAA.h"
#include "Shapers.h"
#include "EnvelopeFollower.h"

namespace tekk
{

class PowerAmp
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        sagEnv.prepare (sr);
        sagEnv.setTimes (sagAttackMs, sagReleaseMs);
        reset();
    }

    void reset() noexcept { sagEnv.reset(); clip.reset(); }

    void setDrive (float knob0to10) noexcept
    {
        drive = 0.4f + 2.4f * (knob0to10 / 10.0f);
    }

    void setSag (float amount0to1) noexcept        // 0 = stiff supply, 1 = spongy
    {
        sagDepth = math::clampf (amount0to1, 0.0f, 0.9f);
    }

    void setSagTimes (double atkMs, double relMs) noexcept
    {
        sagAttackMs = atkMs; sagReleaseMs = relMs;
        sagEnv.setTimes (atkMs, relMs);
    }

    void setCrossover (float depth) noexcept { xoverDepth = math::clampf (depth, 0.0f, 0.4f); }

    inline float process (float x) noexcept
    {
        const float driven = drive * x;

        // --- Supply sag ------------------------------------------------------
        // Envelope of the driven signal droops the rail; fast-ish attack (rail
        // sags under load), slow release (reservoir recharge → bloom).
        const float det    = sagEnv.process (driven);
        const float supply = 1.0f - sagDepth * std::tanh (1.6f * det);
        const float g      = drive * supply;

        float in = g * x;

        // --- Crossover deadzone ---------------------------------------------
        // Smooth reduction of gain near zero: both tubes idling near cutoff.
        // Gaussian notch keeps the harmonic content bounded (and it lives inside
        // the oversampled region anyway).
        const float notch = xoverDepth * std::exp (-(in * in) / (xoverW * xoverW));
        in *= (1.0f - notch);

        // --- Symmetric push-pull saturation ---------------------------------
        float y = clip.process (in);
        return makeup * y;
    }

    float getSupplyState() const noexcept { return 1.0f - sagDepth * std::tanh (1.6f * sagEnv.getEnvelope()); }

private:
    double sampleRate = 44100.0;
    float  drive = 1.6f, makeup = 1.0f;
    float  sagDepth = 0.35f;
    double sagAttackMs = 8.0, sagReleaseMs = 70.0;
    float  xoverDepth = 0.06f;
    static constexpr float xoverW = 0.12f;

    EnvelopeFollower   sagEnv;
    ADAA2<CubicClip>   clip;
};

} // namespace tekk
