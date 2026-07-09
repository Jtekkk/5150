#pragma once

// ============================================================================
//  Biquad.h — RBJ cookbook biquad (TDF-II) with the shelving / peak designs we
//  need for cathode-bypass shaping, presence/resonance NFB approximation and
//  the built-in cabinet voicing.
//
//  Reference: Robert Bristow-Johnson, "Cookbook formulae for audio EQ biquad
//  filter coefficients".
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.0; }

    inline float process (float in) noexcept
    {
        const double x = static_cast<double> (in);
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float> (y);
    }

    void setLowShelf  (double sr, double freq, double gainDb, double S = 0.707) { design (Shelf::Low,  sr, freq, gainDb, S); }
    void setHighShelf (double sr, double freq, double gainDb, double S = 0.707) { design (Shelf::High, sr, freq, gainDb, S); }

    void setPeak (double sr, double freq, double gainDb, double Q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = math::kTwoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);

        const double b0n = 1.0 + alpha * A;
        const double b1n = -2.0 * cw;
        const double b2n = 1.0 - alpha * A;
        const double a0n = 1.0 + alpha / A;
        const double a1n = -2.0 * cw;
        const double a2n = 1.0 - alpha / A;
        normalise (b0n, b1n, b2n, a0n, a1n, a2n);
    }

    void setLowPass (double sr, double freq, double Q = 0.707)
    {
        const double w0 = math::kTwoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double b1n = 1.0 - cw;
        const double b0n = b1n * 0.5;
        const double b2n = b0n;
        const double a0n = 1.0 + alpha;
        const double a1n = -2.0 * cw;
        const double a2n = 1.0 - alpha;
        normalise (b0n, b1n, b2n, a0n, a1n, a2n);
    }

    void setHighPass (double sr, double freq, double Q = 0.707)
    {
        const double w0 = math::kTwoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double b1n = -(1.0 + cw);
        const double b0n = (1.0 + cw) * 0.5;
        const double b2n = b0n;
        const double a0n = 1.0 + alpha;
        const double a1n = -2.0 * cw;
        const double a2n = 1.0 - alpha;
        normalise (b0n, b1n, b2n, a0n, a1n, a2n);
    }

    // Evaluate |H(e^{jw})| at a normalised radian frequency w (0..pi). Handy for
    // unit tests and for the analyser display.
    double magnitude (double w) const noexcept
    {
        const double cw = std::cos (w), c2w = std::cos (2.0 * w);
        const double sw = std::sin (w), s2w = std::sin (2.0 * w);
        const double numRe = b0 + b1 * cw + b2 * c2w;
        const double numIm = -(b1 * sw + b2 * s2w);
        const double denRe = 1.0 + a1 * cw + a2 * c2w;
        const double denIm = -(a1 * sw + a2 * s2w);
        const double num = std::sqrt (numRe * numRe + numIm * numIm);
        const double den = std::sqrt (denRe * denRe + denIm * denIm);
        return num / std::max (den, 1.0e-12);
    }

private:
    enum class Shelf { Low, High };

    void design (Shelf type, double sr, double freq, double gainDb, double S)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = math::kTwoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double tsa = 2.0 * std::sqrt (A) * alpha;

        double b0n, b1n, b2n, a0n, a1n, a2n;
        if (type == Shelf::Low)
        {
            b0n =      A * ((A + 1.0) - (A - 1.0) * cw + tsa);
            b1n =  2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2n =      A * ((A + 1.0) - (A - 1.0) * cw - tsa);
            a0n =          (A + 1.0) + (A - 1.0) * cw + tsa;
            a1n = -2.0 *   ((A - 1.0) + (A + 1.0) * cw);
            a2n =          (A + 1.0) + (A - 1.0) * cw - tsa;
        }
        else
        {
            b0n =      A * ((A + 1.0) + (A - 1.0) * cw + tsa);
            b1n = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2n =      A * ((A + 1.0) + (A - 1.0) * cw - tsa);
            a0n =          (A + 1.0) - (A - 1.0) * cw + tsa;
            a1n =  2.0 *   ((A - 1.0) - (A + 1.0) * cw);
            a2n =          (A + 1.0) - (A - 1.0) * cw - tsa;
        }
        normalise (b0n, b1n, b2n, a0n, a1n, a2n);
    }

    void normalise (double b0n, double b1n, double b2n, double a0n, double a1n, double a2n)
    {
        const double inv = 1.0 / a0n;
        b0 = b0n * inv; b1 = b1n * inv; b2 = b2n * inv;
        a1 = a1n * inv; a2 = a2n * inv;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

} // namespace tekk
