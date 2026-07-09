#pragma once

// ============================================================================
//  NoiseGate.h — hysteretic noise gate (§10, §11).
//
//  Essential on a high-gain amp. Two threshold trips (open above, close a few
//  dB below) give hysteresis so a decaying note doesn't chatter the gate on and
//  off around a single threshold. Independent attack / hold / release shape the
//  gain envelope. Can be placed pre-preamp (tight chug) or post-cab (natural) —
//  the placement switch lives in the processor.
// ============================================================================

#include "DspUtil.h"
#include "EnvelopeFollower.h"

namespace tekk
{

class NoiseGate
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        detector.prepare (sr);
        detector.setTimes (1.0, 12.0);   // detector smoothing
        setTimes (0.5, 40.0, 120.0);
        reset();
    }

    void reset() noexcept { detector.reset(); gain = 0.0f; holdCounter = 0; open = false; }

    void setThreshold (float thresholdDb) noexcept
    {
        openThresh  = math::dbToGain (thresholdDb);
        closeThresh = math::dbToGain (thresholdDb - hysteresisDb);
    }

    void setTimes (double attackMs, double holdMs, double releaseMs) noexcept
    {
        aAtk = onePole (attackMs);
        aRel = onePole (releaseMs);
        holdSamples = (int) (0.001 * holdMs * sampleRate);
    }

    // Detector taps the (usually pre-preamp) signal; the resulting gain is
    // applied to `signal`. Feeding the same buffer does both at once.
    inline float process (float detIn, float signal) noexcept
    {
        const float env = detector.process (detIn);

        if (! open && env > openThresh)       { open = true;  holdCounter = holdSamples; }
        else if (open && env < closeThresh)
        {
            if (holdCounter > 0) --holdCounter;
            else open = false;
        }
        else if (open && env >= closeThresh)  { holdCounter = holdSamples; }

        const float target = open ? 1.0f : 0.0f;
        const float a = (target > gain) ? aAtk : aRel;
        gain += a * (target - gain);
        gain = math::flushDenorm (gain);

        return signal * gain;
    }

    inline float process (float x) noexcept { return process (x, x); }

    float getGain() const noexcept { return gain; }

private:
    float onePole (double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0f;
        return static_cast<float> (1.0 - std::exp (-1.0 / (0.001 * ms * sampleRate)));
    }

    double sampleRate = 44100.0;
    static constexpr float hysteresisDb = 6.0f;
    float openThresh = 0.001f, closeThresh = 0.0005f;
    float aAtk = 0.5f, aRel = 0.01f, gain = 0.0f;
    int   holdSamples = 0, holdCounter = 0;
    bool  open = false;
    EnvelopeFollower detector;
};

} // namespace tekk
