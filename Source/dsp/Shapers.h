#pragma once

// ============================================================================
//  Shapers.h — the static nonlinearities fed to the ADAA wrapper.
//
//  Each shaper exposes f / F1 (and F2 where a closed form exists) so it can be
//  dropped straight into ADAA1<> / ADAA2<>.
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

// ---- Asymmetric triode shaper ----------------------------------------------
//  A 12AX7 gain stage clips asymmetrically: the positive-going grid-conduction
//  knee bites earlier/harder than the negative cutoff knee, and that asymmetry
//  is what generates the even-harmonic "character" (§4).  We model it as a
//  soft saturator with a different hardness either side of zero:
//
//      f(x) = tanh(kp·x)/kp   for x ≥ 0     (grid conduction, higher k = earlier clip)
//             tanh(kn·x)/kn   for x < 0     (cutoff, softer)
//
//  This is C¹ at the origin (value and slope match), and — crucially for ADAA —
//  its first antiderivative
//
//      F1(x) = logcosh(kp·x)/kp²  |  logcosh(kn·x)/kn²
//
//  is continuous everywhere (both pieces → 0 at x=0), so the first-order ADAA
//  difference quotient is well defined even across the sign change.
//
//  No elementary F2 exists (∫ log cosh is a dilogarithm), so triode stages use
//  ADAA1 + oversampling — exactly the balance the spec recommends (§5).
struct TriodeShaper
{
    float kp = 1.30f;   // positive (grid-conduction) hardness
    float kn = 0.72f;   // negative (cutoff) hardness

    inline float f (float x) const noexcept
    {
        return x >= 0.0f ? std::tanh (kp * x) / kp
                         : std::tanh (kn * x) / kn;
    }

    inline float F1 (float x) const noexcept
    {
        return x >= 0.0f ? math::logcosh (kp * x) / (kp * kp)
                         : math::logcosh (kn * x) / (kn * kn);
    }
};

// ---- Symmetric tanh with full antiderivative chain -------------------------
//  Used by the phase inverter and as a general-purpose soft clip.  tanh has a
//  clean F1 = logcosh; F2 has no elementary form, so only ADAA1 is exact here.
struct TanhShaper
{
    float k = 1.0f;
    inline float f  (float x) const noexcept { return std::tanh (k * x); }
    inline float F1 (float x) const noexcept { return math::logcosh (k * x) / k; }
};

// ---- Cubic soft-clip with closed-form F1 AND F2 ----------------------------
//  f(x) = x - x³/3   (|x| ≤ 1),  ±2/3 beyond.  This is the one shaper we have
//  a full second antiderivative for, so it is what the power-amp push-pull
//  stage uses to exercise ADAA2 (the crossover region benefits from the extra
//  aliasing suppression).
//
//      region |x|≤1:  F1 = x²/2 - x⁴/12,          F2 = x³/6  - x⁵/60
//      region  x>1 :  f = 2/3,   F1, F2 continued so both antiderivatives stay C¹.
struct CubicClip
{
    inline float f (float x) const noexcept
    {
        if (x >  1.0f) return  2.0f / 3.0f;
        if (x < -1.0f) return -2.0f / 3.0f;
        return x - x * x * x * (1.0f / 3.0f);
    }

    inline float F1 (float x) const noexcept
    {
        if (x > 1.0f)  return  (2.0f / 3.0f) * x - 0.25f;          // continuity: F1(1)=1/2-1/12=5/12; 2/3-1/4=5/12 ✓
        if (x < -1.0f) return -(2.0f / 3.0f) * x - 0.25f;
        return 0.5f * x * x - (1.0f / 12.0f) * x * x * x * x;
    }

    inline float F2 (float x) const noexcept
    {
        // Constants chosen for C¹ continuity at x=±1 (value + slope match the
        // inner branch): inner F2(±1)=±0.15, slope 5/12.
        if (x > 1.0f)
            return  (1.0f / 3.0f) * x * x - 0.25f * x + (1.0f / 15.0f);
        if (x < -1.0f)
            return -(1.0f / 3.0f) * x * x - 0.25f * x - (1.0f / 15.0f);
        return (1.0f / 6.0f) * x * x * x - (1.0f / 60.0f) * x * x * x * x * x;
    }
};

} // namespace tekk
