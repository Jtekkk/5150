#pragma once

// ============================================================================
//  TransformerJA.h — output transformer core modelled with the Jiles-Atherton
//  hysteresis ODE, solved per sample (§7, §11).
//
//  This is the "correct" upgrade over OutputTransformer.h's play/backlash
//  stand-in: it integrates the standard Jiles-Atherton magnetisation model
//  (the formulation widely used for audio tape/transformer emulation, e.g.
//  J. Chowdhury, "Real-Time Physical Modelling for Analog Tape Machines",
//  DAFx-19) with RK4 across the per-sample field step, then brackets the core
//  with the same LF/HF bandwidth filters (finite primary inductance rolls off
//  the lows; leakage inductance rolls off the highs).
//
//  Drop-in replacement for tekk::OutputTransformer — identical public surface:
//      prepare / reset / setDrive / setSaturation / setHysteresis / process.
//
//  JUCE-independent, pure C++17. Internal state is kept in double precision;
//  only the bandwidth Biquads run in the shared float path.
// ============================================================================

#include "DspUtil.h"
#include "Biquad.h"

#include <cmath>

namespace tekk
{

class JATransformer
{
public:
    // ---- lifecycle ---------------------------------------------------------
    void prepare (double sr) noexcept
    {
        sampleRate = sr;

        // Bandwidth: finite primary inductance → LF high-pass (~22 Hz);
        // leakage inductance + winding capacitance → HF low-pass (~11 kHz)
        // with a touch of resonance.
        lfRolloff.setHighPass (sr, 22.0, 0.707);
        hfRolloff.setLowPass  (sr, 11000.0, 1.1);

        recomputeDerived();
        reset();
    }

    void reset() noexcept
    {
        lfRolloff.reset();
        hfRolloff.reset();
        M      = 0.0;
        Hprev  = 0.0;
    }

    // ---- controls (drop-in signatures) ------------------------------------
    void setDrive (float d) noexcept { drive = static_cast<double> (d); }

    // Saturation maps to the core knee: a larger value shrinks the Langevin
    // scale `a`, so the B-H curve bends over (saturates) sooner.
    void setSaturation (float s) noexcept
    {
        satParam = math::clampf (static_cast<double> (s), 0.05, 4.0);
        recomputeDerived();
    }

    // Hysteresis scales the loop width via the coercivity `k`. 0 → nearly
    // single-valued (thin loop); 1 → wide open loop. Default ~0.5.
    void setHysteresis (float h) noexcept
    {
        hystParam = math::clampf (static_cast<double> (h), 0.0, 1.0);
        recomputeDerived();
    }

    // ---- per-sample process ------------------------------------------------
    inline float process (float xf) noexcept
    {
        const double x = static_cast<double> (xf);

        // Map audio → applied field H. inputScale is chosen so a ±1.0 input
        // pushes He/a well into the saturating region of the Langevin curve.
        const double H  = drive * inputScale * x;
        const double dH = H - Hprev;

        if (dH > 0.0 || dH < 0.0)   // field changed (exact-zero step is skipped)
        {
            const double delta = (dH > 0.0) ? 1.0 : -1.0;
            const double hstep = dH / static_cast<double> (kSubsteps);
            double       Hc    = Hprev;

            // Classic RK4 integration of dM/dH across the per-sample step, in
            // a few substeps for stability at high drive.
            for (int i = 0; i < kSubsteps; ++i)
            {
                const double k1 = dMdH (M,                       Hc,               delta);
                const double k2 = dMdH (M + 0.5 * hstep * k1,    Hc + 0.5 * hstep, delta);
                const double k3 = dMdH (M + 0.5 * hstep * k2,    Hc + 0.5 * hstep, delta);
                const double k4 = dMdH (M +       hstep * k3,    Hc +       hstep, delta);

                M  += (hstep / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
                Hc += hstep;
            }
        }
        // dH == 0 → no field change → dM = 0, M held.

        M = math::flushDenorm (M);
        Hprev = H;

        // Normalised magnetisation (small-signal gain ≈ 1), then bandwidth.
        float y = static_cast<float> (outputScale * M);
        y = lfRolloff.process (y);
        y = hfRolloff.process (y);
        return y;
    }

private:
    // ---- Jiles-Atherton right-hand side: dM/dH at (M,H) --------------------
    inline double dMdH (double Mv, double Hv, double delta) const noexcept
    {
        const double He = Hv + alpha * Mv;      // effective field
        const double xa = He / aEff;

        double L, Lp;                            // Langevin and its derivative
        if (std::fabs (xa) < 1.0e-4)
        {
            L  = xa / 3.0 - (xa * xa * xa) / 45.0;
            Lp = 1.0 / 3.0 - (xa * xa) / 15.0;
        }
        else
        {
            const double th = std::tanh (xa);    // coth(x) = 1/tanh(x)
            const double sh = std::sinh (xa);
            L  = 1.0 / th - 1.0 / xa;
            Lp = 1.0 / (xa * xa) - 1.0 / (sh * sh);
        }

        const double Man      = Ms * L;                  // anhysteretic magnetisation
        const double dMandHe  = (Ms / aEff) * Lp;        // dMan/dHe
        const double q        = Man - Mv;

        // Guard against unphysical states: only let the irreversible term push
        // M toward Man when the field is moving the right way.
        const double deltaM   = (q * delta >= 0.0) ? 1.0 : 0.0;

        double den2 = kEff * delta - alpha * q;          // keep off zero
        if (std::fabs (den2) < 1.0e-9) den2 = (den2 >= 0.0 ? 1.0 : -1.0) * 1.0e-9;

        const double num = (1.0 - cEff) * deltaM * q / den2 + cEff * dMandHe;
        double       den = 1.0 - cEff * alpha * dMandHe; // keep off zero
        if (std::fabs (den) < 1.0e-9) den = (den >= 0.0 ? 1.0 : -1.0) * 1.0e-9;

        return num / den;
    }

    // Recompute the sat/hyst-dependent quantities and the output normalisation.
    void recomputeDerived() noexcept
    {
        aEff = aBase / satParam;                         // sat sharpens the knee

        // Hysteresis opens the B-H loop. The physically clean, monotonic width
        // control in Jiles-Atherton is the reversibility c: c→1 collapses the
        // loop to the single-valued anhysteretic curve, lower c opens it. We
        // scale c with the control (and give the coercivity k a mild parallel
        // sweep, keeping it in the region where the loop still grows with k).
        cEff = 1.0 - hystParam * (1.0 - cMin);           // 1.0 (closed) → cMin
        kEff = kFloor + hystParam * kSpan;               // 0.12 → 0.28

        // Small-signal susceptibility of the JA model at the origin (M=H=0):
        //   dM/dH ≈ c·χan / (1 − c·alpha·χan),  χan = dMan/dHe|0 = Ms/(3·a).
        // Renormalise at the current operating point so the perceived level is
        // held (small-signal gain ≈ 1) across all knob settings.
        const double chiAn = Ms / (3.0 * aEff);
        const double chi0  = (cEff * chiAn) / (1.0 - cEff * alpha * chiAn);
        outputScale = gainTrim / (chi0 * inputScale);
    }

    // ---- fixed model constants (tuned; see transformer_ja_tests.cpp) -------
    static constexpr double Ms         = 1.0;      // saturation magnetisation
    static constexpr double aBase      = 0.16;     // Langevin scale (knee)
    static constexpr double alpha      = 1.1e-3;   // inter-domain coupling
    static constexpr double kFloor     = 0.12;     // coercivity at hyst = 0
    static constexpr double kSpan      = 0.16;     // coercivity sweep with hyst
    static constexpr double cMin       = 0.4;      // reversibility at hyst = 1
    static constexpr double inputScale = 1.0;      // audio → field scaling
    static constexpr double gainTrim   = 1.0;      // small-signal gain trim
    static constexpr int    kSubsteps  = 4;        // RK4 substeps per sample

    // ---- state -------------------------------------------------------------
    double sampleRate = 48000.0;
    double drive      = 1.0;
    double satParam   = 1.0;
    double hystParam  = 0.5;

    double aEff        = aBase;
    double kEff        = kFloor + 0.5 * kSpan;
    double cEff        = 1.0 - 0.5 * (1.0 - cMin);
    double outputScale = 1.0;

    double M     = 0.0;   // magnetisation (double-precision state)
    double Hprev = 0.0;   // previous applied field

    Biquad lfRolloff, hfRolloff;
};

} // namespace tekk
