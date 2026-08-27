// ============================================================================
//  transformer_ja_tests.cpp — standalone validation of the Jiles-Atherton
//  output-transformer core (Source/dsp/TransformerJA.h).
//
//  Build & run (no JUCE required):
//     c++ -std=c++17 -O2 -Wall -Wextra -I../Source
//         transformer_ja_tests.cpp -o jatest && ./jatest
//
//  Covers:
//    1. Finiteness / stability under a loud low-note.
//    2. Small-signal linearity + unity-ish gain.
//    3. Saturation / compression at high drive.
//    4. Hysteresis loop area (open loop) that scales with the control.
//    5. Bandwidth roll-off (LF and HF).
// ============================================================================

#include "dsp/TransformerJA.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

using namespace tekk;

static int g_failures = 0;

static void check (bool cond, const std::string& name, const std::string& detail = "")
{
    std::printf ("  [%s] %s%s%s\n", cond ? "PASS" : "FAIL", name.c_str(),
                 detail.empty() ? "" : "  — ", detail.c_str());
    if (! cond) ++g_failures;
}

static const double kSR = 48000.0;

// Peak |output| of a steady sine of the given amplitude/frequency, after
// discarding a settling interval so the filters + core reach steady state.
static double peakGain (JATransformer& t, double amp, double freq,
                        double seconds = 0.5)
{
    t.reset();
    const int n     = static_cast<int> (seconds * kSR);
    const int settle = static_cast<int> (0.20 * kSR);
    const double w  = 2.0 * M_PI * freq / kSR;
    double peak = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = static_cast<float> (amp * std::sin (w * i));
        const float y = t.process (x);
        if (i >= settle) peak = std::max (peak, std::fabs (static_cast<double> (y)));
    }
    return peak / amp;
}

// Largest finite-difference spread between the rising-field and falling-field
// branches of the B-H loop, probed via the (input, output) trajectory of one
// steady cycle. Driven near ~600 Hz, where the LF/HF filter phase very nearly
// cancels, so the measured gap reflects genuine hysteresis, not filter lag.
static double loopGap (JATransformer& t, double amp, double freq = 600.0)
{
    t.reset();
    const double w        = 2.0 * M_PI * freq / kSR;
    const int    period   = static_cast<int> (std::round (kSR / freq));
    const int    warmCyc  = 40;                 // let minor loop settle
    // Warm up.
    for (int i = 0; i < warmCyc * period; ++i)
        t.process (static_cast<float> (amp * std::sin (w * i)));

    // Capture one full cycle of (x, y).
    std::vector<double> xs, ys;
    xs.reserve (period + 2);
    ys.reserve (period + 2);
    for (int i = 0; i <= period; ++i)
    {
        const double ph = w * (warmCyc * period + i);
        const double x  = amp * std::sin (ph);
        const double y  = static_cast<double> (t.process (static_cast<float> (x)));
        xs.push_back (x);
        ys.push_back (y);
    }

    // For a set of probe field levels, linearly interpolate the output on the
    // rising branch (dx>0) and the falling branch (dx<0), and take the spread.
    double maxGap = 0.0;
    const int probes = 9;
    for (int p = 1; p < probes; ++p)
    {
        const double hx = amp * (-0.8 + 1.6 * p / probes);   // interior levels

        double yUp = 0.0, yDn = 0.0;
        bool   gotUp = false, gotDn = false;
        for (size_t i = 1; i < xs.size(); ++i)
        {
            const double x0 = xs[i - 1], x1 = xs[i];
            if (x1 == x0) continue;
            const bool crosses = (x0 <= hx && hx <= x1) || (x1 <= hx && hx <= x0);
            if (! crosses) continue;
            const double frac = (hx - x0) / (x1 - x0);
            const double y    = ys[i - 1] + frac * (ys[i] - ys[i - 1]);
            if (x1 > x0) { yUp = y; gotUp = true; }
            else         { yDn = y; gotDn = true; }
        }
        if (gotUp && gotDn)
            maxGap = std::max (maxGap, std::fabs (yUp - yDn));
    }
    return maxGap;
}

int main()
{
    std::printf ("Jiles-Atherton output transformer tests\n");

    // ---- 1. Finiteness / stability ----------------------------------------
    {
        JATransformer t;
        t.prepare (kSR);
        const int n = static_cast<int> (2.0 * kSR);
        const double w = 2.0 * M_PI * 110.0 / kSR;
        bool allFinite = true;
        double peak = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float x = static_cast<float> (1.0 * std::sin (w * i));
            const float y = t.process (x);
            if (! std::isfinite (y)) allFinite = false;
            peak = std::max (peak, std::fabs (static_cast<double> (y)));
        }
        check (allFinite, "1a finite output (2s 110Hz @ 1.0)");
        check (peak < 4.0, "1b bounded |y|<4",
               "peak=" + std::to_string (peak));
    }

    // ---- 2. Small-signal ~linear + unity-ish gain -------------------------
    {
        JATransformer t;
        t.prepare (kSR);
        const double g02 = peakGain (t, 0.02, 1000.0);
        const double g01 = peakGain (t, 0.01, 1000.0);
        check (g02 > 0.5 && g02 < 1.5, "2a small-signal peak gain in [0.5,1.5]",
               "gain=" + std::to_string (g02));

        // Feeding 0.01 vs 0.02 should scale the output ~2x (i.e. equal gains).
        const double ratio = g02 / g01;                 // ~1.0 if linear
        check (std::fabs (ratio - 1.0) < 0.10, "2b linear (0.01 vs 0.02 within 10%)",
               "g(.02)/g(.01)=" + std::to_string (ratio));
    }

    // ---- 3. Saturation / compression --------------------------------------
    {
        JATransformer t;
        t.prepare (kSR);
        const double gSmall = peakGain (t, 0.02, 1000.0);
        const double gBig   = peakGain (t, 2.0,  1000.0);
        check (gBig <= 0.7 * gSmall, "3 peak gain @2.0 <= 0.7x small-signal",
               "gBig=" + std::to_string (gBig) + " gSmall=" + std::to_string (gSmall));
    }

    // ---- 4. Hysteresis loop area (open loop, control-dependent) -----------
    {
        JATransformer t;
        t.prepare (kSR);

        // Default hysteresis (~0.5): loop must be clearly open.
        t.setHysteresis (0.5f);
        const double gapDefault = loopGap (t, 0.6);
        check (gapDefault > 0.02, "4a loop open at default hysteresis",
               "maxGap=" + std::to_string (gapDefault));

        // Loop width must shrink from h=1 to h=0.
        t.setHysteresis (1.0f);
        const double gapHi = loopGap (t, 0.6);
        t.setHysteresis (0.0f);
        const double gapLo = loopGap (t, 0.6);
        check (gapHi > gapLo, "4b loop shrinks as hysteresis 1.0 -> 0.0",
               "gap(1.0)=" + std::to_string (gapHi) + " gap(0.0)=" + std::to_string (gapLo));
        // Meaningful separation, not filter-phase noise.
        check (gapLo < gapDefault, "4c h=0 loop narrower than default",
               "gap(0.0)=" + std::to_string (gapLo) + " gap(0.5)=" + std::to_string (gapDefault));
    }

    // ---- 5. Bandwidth (LF and HF roll-off) --------------------------------
    {
        JATransformer t;
        t.prepare (kSR);
        const double amp = 0.03;                          // stay ~linear
        const double gRef = peakGain (t, amp, 1000.0);
        const double gLo  = peakGain (t, amp, 40.0);
        const double gHi  = peakGain (t, amp, 16000.0);
        check (gLo < gRef, "5a 40Hz attenuated vs 1kHz (LF rolloff)",
               "g40=" + std::to_string (gLo) + " g1k=" + std::to_string (gRef));
        check (gHi < gRef, "5b 16kHz attenuated vs 1kHz (HF rolloff)",
               "g16k=" + std::to_string (gHi) + " g1k=" + std::to_string (gRef));
    }

    std::printf ("\n%s (%d failure%s)\n",
                 g_failures == 0 ? "ALL PASS" : "FAILURES",
                 g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
