#pragma once

// ============================================================================
//  EnvelopeFollower.h — peak/RMS-ish envelope follower with independent attack
//  and release time constants.  Drives:
//    * the coupling-cap bias shift inside TubeStage (blocking distortion),
//    * the power-supply sag model,
//    * the noise gate detector.
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

class EnvelopeFollower
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        setTimes (attackMs, releaseMs);
        reset();
    }

    void setTimes (double atkMs, double relMs) noexcept
    {
        attackMs  = atkMs;
        releaseMs = relMs;
        aAtk = coeff (atkMs);
        aRel = coeff (relMs);
    }

    void reset() noexcept { env = 0.0f; }

    // Rectified-peak detector: attack fast when signal exceeds env, release slow.
    inline float process (float x) noexcept
    {
        const float r = std::abs (x);
        const float a = (r > env) ? aAtk : aRel;
        env += a * (r - env);
        return env = math::flushDenorm (env);
    }

    float getEnvelope() const noexcept { return env; }

private:
    float coeff (double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0f;
        return static_cast<float> (1.0 - std::exp (-1.0 / (0.001 * ms * sampleRate)));
    }

    double sampleRate = 44100.0;
    double attackMs = 1.0, releaseMs = 50.0;
    float aAtk = 0.5f, aRel = 0.01f, env = 0.0f;
};

} // namespace tekk
