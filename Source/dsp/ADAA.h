#pragma once

// ============================================================================
//  ADAA.h — Antiderivative Anti-Aliasing for memoryless nonlinearities (§5).
//
//  Cascaded hard saturators throw harmonics far past Nyquist; left alone they
//  fold back as inharmonic "fizz" and are the reason cheap sims sound digital.
//  ADAA suppresses that aliasing cheaply, without brute-force oversampling, by
//  integrating the nonlinearity over each sample interval instead of point
//  sampling it.
//
//  References:
//    * Parker, Zavalishin, Le Bivic, "Reducing the Aliasing of Nonlinear
//      Waveshaping Using Continuous-Time Convolution", DAFx-16.
//    * Bilbao, Esqueda, Parker, Välimäki, "Antiderivative Antialiasing for
//      Memoryless Nonlinearities", IEEE SPL 2017.
//
//  A Nonlinearity is any type exposing:
//      Float f  (Float x)   — the shaping function itself
//      Float F1 (Float x)   — first  antiderivative  (∫ f)
//      Float F2 (Float x)   — second antiderivative  (∫∫ f)   [only for ADAA2]
//
//  Two known gotchas (§5), both handled here:
//    * ill-conditioning when successive inputs are nearly equal (division by a
//      tiny difference) → fall back to the direct/midpoint evaluation.
//    * a half-sample group delay per ADAA order → reported as latency by the
//      host wrapper.
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

// ---- First-order ADAA -------------------------------------------------------
//   y[n] = ( F1(x[n]) - F1(x[n-1]) ) / ( x[n] - x[n-1] )
// with a midpoint fallback f((x+x1)/2) when the denominator is tiny.
// Introduces 0.5 sample of delay.
template <typename Nonlinearity, typename Float = float>
class ADAA1
{
public:
    void reset() noexcept { x1 = F1x1 = Float (0); }

    inline Float process (Float x) noexcept
    {
        const Float F1x = nl.F1 (x);
        const Float diff = x - x1;
        Float y;
        if (std::abs (diff) < kTol)
            y = nl.f (Float (0.5) * (x + x1));      // ill-conditioned → direct eval
        else
            y = (F1x - F1x1) / diff;
        x1   = x;
        F1x1 = F1x;
        return y;
    }

    Nonlinearity& nonlinearity() noexcept { return nl; }
    static constexpr double latencySamples() noexcept { return 0.5; }

private:
    static constexpr Float kTol = static_cast<Float> (1.0e-5);
    Nonlinearity nl;
    Float x1 = 0, F1x1 = 0;
};

// ---- Second-order ADAA ------------------------------------------------------
//   Uses x[n], x[n-1], x[n-2] and the first two antiderivatives.  Robust
//   formulation after J. Chowdhury (chowdsp): a nested fallback handles both
//   x[n]≈x[n-2] and the inner x̄≈x[n-1] degeneracy.  Introduces 1 sample delay.
template <typename Nonlinearity, typename Float = float>
class ADAA2
{
public:
    void reset() noexcept { x1 = x2 = 0; }

    inline Float process (Float x) noexcept
    {
        Float y;
        if (std::abs (x - x2) < kTol)
        {
            const Float xBar  = Float (0.5) * (x + x2);
            const Float delta = xBar - x1;
            if (std::abs (delta) < kTol)
                y = nl.f (Float (0.5) * (xBar + x1));
            else
                y = (Float (2) / delta) * (nl.F1 (xBar) + (nl.F2 (x1) - nl.F2 (xBar)) / delta);
        }
        else
        {
            y = (Float (2) / (x - x2)) * (calcD (x, x1) - calcD (x1, x2));
        }
        x2 = x1;
        x1 = x;
        return y;
    }

    Nonlinearity& nonlinearity() noexcept { return nl; }
    static constexpr double latencySamples() noexcept { return 1.0; }

private:
    inline Float calcD (Float a, Float b) noexcept
    {
        const Float diff = a - b;
        if (std::abs (diff) < kTol)
            return nl.F1 (Float (0.5) * (a + b));
        return (nl.F2 (a) - nl.F2 (b)) / diff;
    }

    // ADAA2 needs a *looser* ill-conditioning tolerance than ADAA1. At a signal
    // extremum the three taps bunch together; if the outer/`calcD` branches
    // disagree about which are "equal", the large 2/(x-x2) factor amplifies the
    // mismatch into a spike. A generous tolerance makes the whole extremum use
    // the smooth fallback consistently. 1e-3 caps the worst-case reconstruction
    // error at ~1e-3 (verified in tests/dsp_tests.cpp); 1e-5 leaves audible
    // spikes at slope reversals.
    static constexpr Float kTol = static_cast<Float> (1.0e-3);
    Nonlinearity nl;
    Float x1 = 0, x2 = 0;
};

} // namespace tekk
