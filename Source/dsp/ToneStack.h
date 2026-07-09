#pragma once

// ============================================================================
//  ToneStack.h — passive Bass/Mid/Treble tone stack (§6).
//
//  This is the interactive passive network that sits between the preamp and the
//  power amp. The three controls are NOT independent EQ bands: the network
//  loads the driving stage and the pots interact. We model it as the analog
//  prototype transfer function H(s) discretised with the bilinear transform,
//  recomputing coefficients at control rate.
//
//  Transfer function and discretisation after David Te-Mao Yeh & J.O. Smith,
//  "Discretization of the '59 Fender Bassman Tone Stack" (DAFx-06); the exact
//  coefficient algebra matches the Guitarix / Faust `tonestacks.lib`
//  formulation. Component values below are the Peavey ("classic American
//  120 W 6L6 head") voicing: a Marshall-style TMB with a 20 k mid pot and a
//  68 k slope resistor.
//
//      H(s) = (b1 s + b2 s² + b3 s³) / (a0 + a1 s + a2 s² + a3 s³)
//
//  Network (input from preamp on the left, wiper of treble pot = output):
//
//        C1
//   IN >--||-------+-----------+
//        |         |          | | R1  Treble
//       | |R4     | |R2 Bass  | |<---- OUT
//       | |       | |          |
//        | C2      |           |
//        +--||-----+----+      |
//        |              | |    |
//        |     C3       | |<--- R2 Bass wiper
//        +-----||------>| | R3 Middle
//                        |
//                       GND
// ============================================================================

#include "DspUtil.h"

namespace tekk
{

struct ToneStackComponents
{
    // Peavey / classic-American-120W voicing (values in ohms / farads).
    double R1 = 250e3;   // treble pot
    double R2 = 250e3;   // bass pot
    double R3 = 20e3;    // mid pot
    double R4 = 68e3;    // slope resistor
    double C1 = 270e-12; // treble cap
    double C2 = 22e-9;   // bass cap
    double C3 = 22e-9;   // mid cap
};

class ToneStack
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        reset();
        update();
    }

    void reset() noexcept
    {
        x1 = x2 = x3 = 0.0;
        y1 = y2 = y3 = 0.0;
    }

    void setComponents (const ToneStackComponents& c) noexcept { comp = c; update(); }

    // t, m, bassRaw in [0,1]. The bass control follows a log ("audio") taper —
    // a linear pot fraction would put nearly all of the useful travel at one end.
    void setControls (float t, float m, float bassRaw) noexcept
    {
        const float nt = math::clampf (t, 0.0f, 1.0f);
        const float nm = math::clampf (m, 0.0f, 1.0f);
        const float nl = math::clampf (bassRaw, 0.0f, 1.0f);
        // Skip the (cubic) coefficient recompute when nothing moved.
        constexpr float eps = 1.0e-7f;
        if (std::abs (nt - tSet) < eps && std::abs (nm - mSet) < eps && std::abs (nl - bSet) < eps)
            return;
        tSet = nt; mSet = nm; bSet = nl;
        update();
    }

    inline float process (float in) noexcept
    {
        const double x = static_cast<double> (in);
        double y = nb0 * x + nb1 * x1 + nb2 * x2 + nb3 * x3
                          - na1 * y1 - na2 * y2 - na3 * y3;
        y = math::flushDenorm (y);
        x3 = x2; x2 = x1; x1 = x;
        y3 = y2; y2 = y1; y1 = y;
        return static_cast<float> (y);
    }

    // |H(e^{jw})| for tests / analyser (w in 0..pi).
    double magnitude (double w) const noexcept
    {
        const double cw  = std::cos (w),  sw  = std::sin (w);
        const double c2w = std::cos (2*w), s2w = std::sin (2*w);
        const double c3w = std::cos (3*w), s3w = std::sin (3*w);
        const double nRe = nb0 + nb1 * cw + nb2 * c2w + nb3 * c3w;
        const double nIm = -(nb1 * sw + nb2 * s2w + nb3 * s3w);
        const double dRe = 1.0 + na1 * cw + na2 * c2w + na3 * c3w;
        const double dIm = -(na1 * sw + na2 * s2w + na3 * s3w);
        return std::sqrt (nRe*nRe + nIm*nIm) / std::max (std::sqrt (dRe*dRe + dIm*dIm), 1e-15);
    }

    // Largest denominator root magnitude — a stability check for the tests.
    // (All three poles must sit strictly inside the unit circle.)
    double maxPoleMagnitude() const noexcept;

private:
    void update() noexcept
    {
        const double t = tSet;
        const double m = mSet;
        // Bass log taper (matches the Guitarix mapping exactly).
        const double l = std::exp ((bSet - 1.0) * 3.4);

        const double C1 = comp.C1, C2 = comp.C2, C3 = comp.C3;
        const double R1 = comp.R1, R2 = comp.R2, R3 = comp.R3, R4 = comp.R4;

        const double b1 = t*C1*R1 + m*C3*R3 + l*(C1*R2 + C2*R2) + (C1*R3 + C2*R3);

        const double b2 = t*(C1*C2*R1*R4 + C1*C3*R1*R4)
                        - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
                        + m*(C1*C3*R1*R3 + C1*C3*R3*R3 + C2*C3*R3*R3)
                        + l*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4)
                        + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3)
                        + (C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R3*R4);

        const double b3 = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4)
                        - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                        + m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                        + t*C1*C2*C3*R1*R3*R4 - t*m*C1*C2*C3*R1*R3*R4
                        + t*l*C1*C2*C3*R1*R2*R4;

        const double a0 = 1.0;

        const double a1 = (C1*R1 + C1*R3 + C2*R3 + C2*R4 + C3*R4)
                        + m*C3*R3 + l*(C1*R2 + C2*R2);

        const double a2 = m*(C1*C3*R1*R3 - C2*C3*R3*R4 + C1*C3*R3*R3 + C2*C3*R3*R3)
                        + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3)
                        - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
                        + l*(C1*C2*R2*R4 + C1*C2*R1*R2 + C1*C3*R2*R4 + C2*C3*R2*R4)
                        + (C1*C2*R1*R4 + C1*C3*R1*R4 + C1*C2*R3*R4 + C1*C2*R1*R3 + C1*C3*R3*R4 + C2*C3*R3*R4);

        const double a3 = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4)
                        - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                        + m*(C1*C2*C3*R3*R3*R4 + C1*C2*C3*R1*R3*R3 - C1*C2*C3*R1*R3*R4)
                        + l*C1*C2*C3*R1*R2*R4
                        + C1*C2*C3*R1*R3*R4;

        // Bilinear transform, s -> c (z-1)/(z+1) with c = 2 fs (no prewarp,
        // per Yeh). Yields a 3rd-order IIR; store coefficients normalised by A0.
        const double c  = 2.0 * sampleRate;
        const double c2 = c * c;
        const double c3 = c2 * c;

        const double B0 = -b1*c - b2*c2 - b3*c3;
        const double B1 = -b1*c + b2*c2 + 3*b3*c3;
        const double B2 =  b1*c + b2*c2 - 3*b3*c3;
        const double B3 =  b1*c - b2*c2 + b3*c3;
        const double A0 = -a0 - a1*c - a2*c2 - a3*c3;
        const double A1 = -3*a0 - a1*c + a2*c2 + 3*a3*c3;
        const double A2 = -3*a0 + a1*c + a2*c2 - 3*a3*c3;
        const double A3 = -a0 + a1*c - a2*c2 + a3*c3;

        const double inv = 1.0 / A0;
        nb0 = B0*inv; nb1 = B1*inv; nb2 = B2*inv; nb3 = B3*inv;
        na1 = A1*inv; na2 = A2*inv; na3 = A3*inv;
    }

    double sampleRate = 44100.0;
    ToneStackComponents comp;
    float tSet = 0.5f, mSet = 0.5f, bSet = 0.5f;

    // Normalised difference-equation coefficients.
    double nb0 = 1, nb1 = 0, nb2 = 0, nb3 = 0, na1 = 0, na2 = 0, na3 = 0;
    // State.
    double x1 = 0, x2 = 0, x3 = 0, y1 = 0, y2 = 0, y3 = 0;
};

// Cubic root-magnitude via companion-matrix eigenvalue-free approach: we only
// need the max |root| for a stability assertion, so use a few Newton/deflation
// steps on the monic denominator z³ + na1 z² + na2 z + na3.
inline double ToneStack::maxPoleMagnitude() const noexcept
{
    // Denominator polynomial p(z) = z^3 + na1 z^2 + na2 z + na3.
    const double a = na1, b = na2, cc = na3;
    // Depressed cubic via trig/Cardano to get all three (possibly complex) roots.
    const double p = b - a * a / 3.0;
    const double q = 2.0 * a * a * a / 27.0 - a * b / 3.0 + cc;
    const double shift = -a / 3.0;
    const double disc = q * q / 4.0 + p * p * p / 27.0;

    auto mag = [] (double re, double im) { return std::sqrt (re * re + im * im); };
    double best = 0.0;

    if (disc >= 0.0)
    {
        const double s = std::sqrt (disc);
        const double u = std::cbrt (-q / 2.0 + s);
        const double v = std::cbrt (-q / 2.0 - s);
        const double r1 = u + v + shift;                       // real root
        const double reC = -0.5 * (u + v) + shift;             // complex pair real part
        const double imC = std::sqrt (3.0) / 2.0 * (u - v);    // ± imaginary part
        best = std::max (std::abs (r1), mag (reC, imC));
    }
    else
    {
        const double r = std::sqrt (-p * p * p / 27.0);
        const double phi = std::acos (math::clampf (-q / 2.0 / r, -1.0, 1.0));
        const double m2 = 2.0 * std::sqrt (-p / 3.0);
        for (int kk = 0; kk < 3; ++kk)
        {
            const double root = m2 * std::cos ((phi + 2.0 * math::kPi * kk) / 3.0) + shift;
            best = std::max (best, std::abs (root));
        }
    }
    return best;
}

} // namespace tekk
