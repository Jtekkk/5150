#pragma once

// ============================================================================
//  DCBlocker.h — classic one-pole/one-zero DC blocker.
//      y[n] = x[n] - x[n-1] + R y[n-1]
//  Used at the very front of the chain (§3, HPF ~5-10 Hz) and after any stage
//  that introduces a bias offset (the tube coupling model deliberately shifts
//  DC and relies on downstream AC coupling to remove it).
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

class DCBlocker
{
public:
    void prepare (double sr, double cutoffHz = 8.0) noexcept
    {
        R = static_cast<float> (1.0 - (math::kTwoPi * cutoffHz / sr));
        R = math::clampf (R, 0.0f, 0.99999f);
        reset();
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = math::flushDenorm (y);
        return y1;
    }

private:
    float R = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

} // namespace tekk
