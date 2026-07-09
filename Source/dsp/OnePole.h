#pragma once

// ============================================================================
//  OnePole.h — first-order low-pass / high-pass building blocks.
//
//  Used all over the preamp: grid-stopper roll-offs, coupling high-passes,
//  envelope smoothing.  Coefficients are recomputed from a cutoff in Hz.
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

// Simple leaky-integrator low-pass:  y += g (x - y).
class OnePoleLP
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; update(); }
    void setCutoff (double hz) noexcept { cutoffHz = hz; update(); }
    void reset () noexcept { y = 0.0f; }

    inline float process (float x) noexcept
    {
        y += g * (x - y);
        return y = math::flushDenorm (y);
    }

    float getState() const noexcept { return y; }

private:
    void update() noexcept
    {
        const double fc = std::min (std::max (cutoffHz, 1.0), sampleRate * 0.49);
        g = static_cast<float> (1.0 - std::exp (-math::kTwoPi * fc / sampleRate));
    }

    double sampleRate = 44100.0;
    double cutoffHz   = 10000.0;
    float  g = 0.1f;
    float  y = 0.0f;
};

// First-order high-pass built as (x - lowpass(x)); doubles as a gentle coupling
// cap / DC path.  Sharing the same corner definition as OnePoleLP keeps the
// interstage voicing predictable.
class OnePoleHP
{
public:
    void prepare (double sr) noexcept { lp.prepare (sr); }
    void setCutoff (double hz) noexcept { lp.setCutoff (hz); }
    void reset() noexcept { lp.reset(); }

    inline float process (float x) noexcept
    {
        return x - lp.process (x);
    }

private:
    OnePoleLP lp;
};

} // namespace tekk
