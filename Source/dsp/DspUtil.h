#pragma once

// ============================================================================
//  DspUtil.h — shared constants and small numeric helpers.
//
//  Everything in the dsp/ folder is deliberately free of any JUCE dependency
//  (with the single exception of Cabinet.h, which wraps juce::dsp::Convolution).
//  Keeping the signal-processing core pure C++ lets us unit-test it with a bare
//  g++/clang build — see tests/dsp_tests.cpp — which is how the anti-aliasing,
//  tone-stack and power-amp behaviours are validated against the spec (§13).
// ============================================================================

#include <cmath>
#include <algorithm>

namespace tekk
{
namespace math
{
    constexpr double kPi     = 3.14159265358979323846;
    constexpr double kTwoPi  = 2.0 * kPi;

    // Flush denormals with a tiny DC offset trick where ScopedNoDenormals is not
    // in scope (the plugin also enables FTZ/DAZ globally — see §15). Cheap and
    // branch-free; only meaningful on values that have already decayed towards 0.
    template <typename F>
    inline F flushDenorm (F x) noexcept
    {
        static constexpr F tiny = static_cast<F> (1.0e-30);
        x += tiny;
        x -= tiny;
        return x;
    }

    // Numerically stable log(cosh(x)).  cosh() overflows for |x| ≳ 710 in double,
    // and we routinely push tube stages far into saturation, so evaluate it as
    //     log(cosh x) = |x| + log1p(exp(-2|x|)) - log(2)
    // which is exact for small |x| (→0 at x=0) and asymptotically |x|-log2.
    template <typename F>
    inline F logcosh (F x) noexcept
    {
        const F a = std::abs (x);
        return a + std::log1p (std::exp (F (-2) * a)) - static_cast<F> (0.6931471805599453);
    }

    template <typename F>
    inline F clampf (F x, F lo, F hi) noexcept
    {
        return std::min (std::max (x, lo), hi);
    }

    // dB <-> linear.
    template <typename F>
    inline F dbToGain (F db) noexcept { return std::pow (F (10), db * F (0.05)); }

    template <typename F>
    inline F gainToDb (F g)  noexcept { return F (20) * std::log10 (std::max (g, F (1.0e-9))); }

    // One-pole smoothing coefficient for a given time constant (seconds).
    template <typename F>
    inline F onePoleCoeff (F timeConstantSeconds, double sampleRate) noexcept
    {
        if (timeConstantSeconds <= F (0)) return F (1);
        return F (1) - std::exp (F (-1) / (timeConstantSeconds * static_cast<F> (sampleRate)));
    }
}
}
