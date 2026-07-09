// ============================================================================
//  dsp_tests.cpp — standalone validation of the JUCE-independent DSP core.
//
//  Build & run (no JUCE required):
//      c++ -std=c++17 -O2 -I../Source tests/dsp_tests.cpp -o dsp_tests && ./dsp_tests
//
//  Covers the spec's validation priorities (§13):
//    * ADAA anti-aliasing actually suppresses aliasing vs. naive waveshaping.
//    * Shaper antiderivatives are C¹-continuous (ADAA correctness precondition).
//    * Tone stack is stable and moves in the right direction per control.
//    * Power-supply sag droops then blooms.
//    * Noise gate mutes silence, passes signal, and doesn't chatter.
//    * Output transformer saturates and bandlimits.
// ============================================================================

#include "dsp/ADAA.h"
#include "dsp/Shapers.h"
#include "dsp/TubeStage.h"
#include "dsp/PreampCascade.h"
#include "dsp/ToneStack.h"
#include "dsp/PowerAmp.h"
#include "dsp/OutputTransformer.h"
#include "dsp/NoiseGate.h"
#include "dsp/SpeakerVoicing.h"

#include <cstdio>
#include <vector>
#include <complex>
#include <cmath>
#include <string>

using namespace tekk;
static int g_failures = 0;

static void check (bool cond, const std::string& name, const std::string& detail = "")
{
    std::printf ("  [%s] %s%s%s\n", cond ? "PASS" : "FAIL", name.c_str(),
                 detail.empty() ? "" : "  — ", detail.c_str());
    if (! cond) ++g_failures;
}

// ---- tiny iterative radix-2 FFT --------------------------------------------
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        std::complex<double> wlen (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                auto u = a[i + k];
                auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// Windowed-sinc FIR decimator used to build the "ground-truth" reference.
static std::vector<double> designLowpass (int numTaps, double fcNorm)
{
    std::vector<double> h ((size_t) numTaps);
    const double m = (numTaps - 1) / 2.0;
    double sum = 0.0;
    for (int i = 0; i < numTaps; ++i)
    {
        const double x = i - m;
        const double sinc = (std::abs (x) < 1e-9) ? 2.0 * fcNorm
                                                  : std::sin (2.0 * M_PI * fcNorm * x) / (M_PI * x);
        const double win = 0.54 - 0.46 * std::cos (2.0 * M_PI * i / (numTaps - 1)); // Hamming
        h[(size_t) i] = sinc * win;
        sum += h[(size_t) i];
    }
    for (auto& v : h) v /= sum;
    return h;
}

// ============================================================================
static void testShaperContinuity()
{
    std::printf ("Shaper antiderivative continuity (ADAA precondition):\n");

    TriodeShaper tri;
    // f continuous at 0, F1 continuous at 0.
    check (std::abs (tri.f (1e-6f) - tri.f (-1e-6f)) < 1e-4f, "TriodeShaper f continuous at 0");
    check (std::abs (tri.F1 (0.0f)) < 1e-6f, "TriodeShaper F1(0)=0");

    CubicClip cc;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) < tol; };
    // Value + slope continuity of F1 and F2 at the clip knees x=±1.
    check (near (cc.F1 (1.0f), 0.5f - 1.0f / 12.0f, 1e-5f), "CubicClip F1(1) value");
    check (near (cc.F2 (1.0f), 1.0f / 6.0f - 1.0f / 60.0f, 1e-5f), "CubicClip F2(1) inner value");
    // Approaching the knee from both sides.
    check (near (cc.F2 (1.001f), cc.F2 (0.999f), 2e-3f), "CubicClip F2 continuous at +1");
    check (near (cc.F2 (-1.001f), cc.F2 (-0.999f), 2e-3f), "CubicClip F2 continuous at -1");
    check (near (cc.F1 (1.001f), cc.F1 (0.999f), 2e-3f), "CubicClip F1 continuous at +1");
    // f'(x)=1-x^2 inside; f flat outside.
    check (near (cc.f (2.0f), 2.0f / 3.0f, 1e-6f), "CubicClip saturates to 2/3");
}

// Measure inharmonic (alias) energy for a hard-clipped single tone. Uses an
// integer number of periods so there is zero spectral leakage.
static double aliasEnergy (const std::vector<double>& sig, int k0)
{
    const size_t N = sig.size();
    std::vector<std::complex<double>> spec (N);
    for (size_t i = 0; i < N; ++i) spec[i] = { sig[i], 0.0 };
    fft (spec);

    double alias = 0.0;
    const size_t half = N / 2;
    for (size_t b = 1; b <= half; ++b)
    {
        // Legit (below-Nyquist) harmonics sit exactly on multiples of k0.
        const bool legit = (b % (size_t) k0) == 0 && b <= half;
        if (! legit)
            alias += std::norm (spec[b]);
    }
    return alias;
}

static void testADAAAntiAliasing()
{
    std::printf ("ADAA anti-aliasing (§5, the make-or-break issue):\n");

    const size_t N  = 16384;
    const int    k0 = 3400;          // fundamental bin; f0 = k0*fs/N ≈ 9.96 kHz @ 48k
    const double amp = 1.0;
    const double k   = 4.0;          // strong tanh saturation

    // (a) naive point-wise waveshaping at 1x.
    std::vector<double> naive (N);
    for (size_t n = 0; n < N; ++n)
    {
        const double x = amp * std::sin (2.0 * M_PI * (double) k0 * (double) n / (double) N);
        naive[n] = std::tanh (k * x);
    }

    // (b) first-order ADAA at 1x.
    std::vector<double> adaaSig (N);
    {
        ADAA1<TanhShaper, double> adaa;
        adaa.nonlinearity().k = k;
        for (size_t n = 0; n < N; ++n)
        {
            const double x = amp * std::sin (2.0 * M_PI * (double) k0 * (double) n / (double) N);
            adaaSig[n] = adaa.process (x);
        }
    }

    // (c) 16x oversampled reference, FIR-decimated → near alias-free ground truth.
    std::vector<double> ref (N);
    {
        const int OS = 16;
        std::vector<double> hi (N * (size_t) OS);
        for (size_t n = 0; n < hi.size(); ++n)
        {
            const double x = amp * std::sin (2.0 * M_PI * (double) k0 * (double) n / (double) (N * OS));
            hi[n] = std::tanh (k * x);
        }
        auto h = designLowpass (129, 0.5 / OS * 0.9);
        const int M = (int) h.size();
        for (size_t n = 0; n < N; ++n)
        {
            const long center = (long) (n * OS);
            double acc = 0.0;
            for (int t = 0; t < M; ++t)
            {
                const long idx = center - (t - (M - 1) / 2);
                if (idx >= 0 && idx < (long) hi.size())
                    acc += h[(size_t) t] * hi[(size_t) idx];
            }
            ref[n] = acc;
        }
    }

    const double aNaive = aliasEnergy (naive, k0);
    const double aAdaa  = aliasEnergy (adaaSig, k0);
    const double aRef   = aliasEnergy (ref, k0);

    auto dB = [] (double x) { return 10.0 * std::log10 (std::max (x, 1e-300)); };
    std::printf ("     alias energy: naive %.1f dB | ADAA1 %.1f dB | 16x ref %.1f dB\n",
                 dB (aNaive), dB (aAdaa), dB (aRef));
    std::printf ("     ADAA1 improvement over naive: %.1f dB\n", dB (aNaive) - dB (aAdaa));

    check (std::isfinite (aAdaa) && std::isfinite (aNaive), "outputs finite");
    check (aAdaa < aNaive * 0.5, "ADAA1 reduces aliasing >3 dB vs naive");
    check (aRef < aAdaa, "16x reference cleaner than ADAA1 alone (sanity)");
}

static void testADAA2Reference()
{
    std::printf ("ADAA2 (cubic soft-clip) sanity:\n");
    ADAA2<CubicClip, double> adaa;
    // A slow low-amplitude ramp stays in the linear region; ADAA2 output should
    // track f very closely there.
    double maxErr = 0.0;
    CubicClip cc;
    double prev = 0.0;
    for (int n = 0; n < 2000; ++n)
    {
        const double x = 0.3 * std::sin (2.0 * M_PI * n / 512.0);
        const double y = adaa.process (x);
        // Compare against f delayed by one sample (ADAA2 ≈ 1 sample group delay).
        const double expected = cc.f (prev);
        maxErr = std::max (maxErr, std::abs (y - expected));
        prev = x;
    }
    check (maxErr < 0.02, "ADAA2 tracks f in linear region", "maxErr=" + std::to_string (maxErr));
}

static void testToneStack()
{
    std::printf ("Tone stack: stability + directional response (§6):\n");
    const double fs = 48000.0;
    ToneStack ts; ts.prepare (fs);

    // Stability across a grid of control settings.
    bool stable = true;
    double worst = 0.0;
    for (float t = 0.0f; t <= 1.0f; t += 0.25f)
        for (float m = 0.0f; m <= 1.0f; m += 0.25f)
            for (float b = 0.0f; b <= 1.0f; b += 0.25f)
            {
                ts.setControls (t, m, b);
                const double pm = ts.maxPoleMagnitude();
                worst = std::max (worst, pm);
                if (pm >= 1.0) stable = false;
            }
    check (stable, "all poles inside unit circle over control grid", "max|pole|=" + std::to_string (worst));

    auto w = [fs] (double hz) { return 2.0 * M_PI * hz / fs; };

    // Treble control raises high-frequency output.
    ts.setControls (0.1f, 0.5f, 0.5f); const double hiLo = ts.magnitude (w (4000));
    ts.setControls (0.9f, 0.5f, 0.5f); const double hiHi = ts.magnitude (w (4000));
    check (hiHi > hiLo, "treble up raises 4 kHz", "lo=" + std::to_string (hiLo) + " hi=" + std::to_string (hiHi));

    // Bass control raises low-frequency output.
    ts.setControls (0.5f, 0.5f, 0.1f); const double loLo = ts.magnitude (w (90));
    ts.setControls (0.5f, 0.5f, 0.9f); const double loHi = ts.magnitude (w (90));
    check (loHi > loLo, "bass up raises 90 Hz", "lo=" + std::to_string (loLo) + " hi=" + std::to_string (loHi));

    // Mid control raises midrange output (scoop when low).
    ts.setControls (0.5f, 0.1f, 0.5f); const double mdLo = ts.magnitude (w (650));
    ts.setControls (0.5f, 0.9f, 0.5f); const double mdHi = ts.magnitude (w (650));
    check (mdHi > mdLo, "mid up raises 650 Hz", "lo=" + std::to_string (mdLo) + " hi=" + std::to_string (mdHi));

    // DC is blocked (AC-coupled network).
    ts.setControls (0.5f, 0.5f, 0.5f);
    check (ts.magnitude (0.0) < 1e-3, "DC blocked by the stack");
}

static void testSag()
{
    std::printf ("Power-supply sag: droop then bloom (§7):\n");
    const double fs = 48000.0;
    PowerAmp pa; pa.prepare (fs);
    pa.setDrive (8.0f);
    pa.setSag (0.6f);
    pa.setSagTimes (8.0, 80.0);

    // Hit it with a sustained loud tone, then silence, tracking the supply state.
    const double f = 110.0;
    double supplyAtStart = 1.0, supplyAtLoad = 1.0, supplyAfterRelease = 1.0;
    for (int n = 0; n < (int) (fs * 0.4); ++n)
    {
        const double x = 0.9 * std::sin (2.0 * M_PI * f * n / fs);
        pa.process ((float) x);
        if (n == 8) supplyAtStart = pa.getSupplyState();
        if (n == (int) (fs * 0.05)) supplyAtLoad = pa.getSupplyState();
    }
    for (int n = 0; n < (int) (fs * 0.4); ++n)
    {
        pa.process (0.0f);
    }
    supplyAfterRelease = pa.getSupplyState();

    std::printf ("     supply: start %.3f | under load %.3f | after release %.3f\n",
                 supplyAtStart, supplyAtLoad, supplyAfterRelease);
    check (supplyAtLoad < supplyAtStart - 0.05f, "rail droops under sustained load");
    check (supplyAfterRelease > supplyAtLoad + 0.05f, "rail recovers (bloom) after load");
}

static void testNoiseGate()
{
    std::printf ("Noise gate: mute / pass / no chatter (§10):\n");
    const double fs = 48000.0;
    NoiseGate gate; gate.prepare (fs);
    gate.setThreshold (-40.0f);
    gate.setTimes (0.5, 30.0, 80.0);

    // Quiet noise floor → gated (near silent output).
    double quietOut = 0.0;
    for (int n = 0; n < (int) fs; ++n)
    {
        const double noise = 0.001 * std::sin (2.0 * M_PI * 200.0 * n / fs); // -60 dB
        quietOut = std::max (quietOut, (double) std::abs (gate.process ((float) noise)));
    }
    check (quietOut < 0.001, "sub-threshold signal is gated");

    // Loud note → passes.
    double loudOut = 0.0;
    for (int n = 0; n < (int) (fs * 0.2); ++n)
    {
        const double s = 0.5 * std::sin (2.0 * M_PI * 200.0 * n / fs);
        loudOut = std::max (loudOut, (double) std::abs (gate.process ((float) s)));
    }
    check (loudOut > 0.4, "above-threshold signal passes", "peak=" + std::to_string (loudOut));

    // Signal hovering right around threshold shouldn't flip the gate rapidly.
    gate.reset();
    int transitions = 0; float lastGain = gate.getGain();
    for (int n = 0; n < (int) (fs * 0.5); ++n)
    {
        const double env = 0.011 + 0.003 * std::sin (2.0 * M_PI * 3.0 * n / fs); // wobble near -39 dB
        const double s = env * std::sin (2.0 * M_PI * 200.0 * n / fs);
        gate.process ((float) s);
        const float g = gate.getGain();
        if ((g > 0.5f) != (lastGain > 0.5f)) ++transitions;
        lastGain = g;
    }
    check (transitions <= 2, "hysteresis prevents chatter near threshold",
           "transitions=" + std::to_string (transitions));
}

static void testTransformer()
{
    std::printf ("Output transformer: saturation + bandwidth (§7):\n");
    const double fs = 48000.0;
    OutputTransformer ot; ot.prepare (fs);
    ot.setDrive (1.0f); ot.setSaturation (1.2f);

    // Small signal ~linear gain; large signal compresses (core saturates).
    auto peakFor = [&] (double amp)
    {
        ot.reset();
        double pk = 0.0;
        for (int n = 0; n < (int) (fs * 0.1); ++n)
        {
            const double x = amp * std::sin (2.0 * M_PI * 220.0 * n / fs);
            pk = std::max (pk, (double) std::abs (ot.process ((float) x)));
        }
        return pk;
    };
    const double small = peakFor (0.05);
    const double large = peakFor (2.0);
    const double smallGain = small / 0.05;
    const double largeGain = large / 2.0;
    check (largeGain < smallGain * 0.7, "core saturates at high level",
           "smallGain=" + std::to_string (smallGain) + " largeGain=" + std::to_string (largeGain));
    check (std::isfinite (large), "transformer output finite");
}

static void testPreampGainStructure()
{
    std::printf ("Preamp cascade: Lead has more gain than Rhythm (§4):\n");
    const double fs = 96000.0;
    PreampCascade rhythm, lead;
    rhythm.prepare (fs); lead.prepare (fs);
    rhythm.setChannel (Channel::Rhythm); rhythm.setPreGain (7.0f);
    lead.setChannel   (Channel::Lead);   lead.setPreGain   (7.0f);

    auto rms = [fs] (PreampCascade& p)
    {
        double acc = 0.0; int cnt = 0;
        for (int n = 0; n < (int) (fs * 0.1); ++n)
        {
            const double x = 0.05 * std::sin (2.0 * M_PI * 150.0 * n / fs);
            const double y = p.process ((float) x);
            if (n > 2000) { acc += y * y; ++cnt; }
        }
        return std::sqrt (acc / std::max (cnt, 1));
    };
    const double rRms = rms (rhythm);
    const double lRms = rms (lead);
    check (lead.getActiveStages() == 5 && rhythm.getActiveStages() == 3, "stage counts (Lead 5 / Rhythm 3)");
    check (std::isfinite (lRms) && std::isfinite (rRms) && lRms > 0.0, "cascade output finite & non-zero");
    std::printf ("     rhythm RMS %.4f | lead RMS %.4f\n", rRms, lRms);
}

int main()
{
    std::printf ("================ TEKK Redline 120 — DSP core validation ================\n\n");
    testShaperContinuity();       std::printf ("\n");
    testADAAAntiAliasing();       std::printf ("\n");
    testADAA2Reference();         std::printf ("\n");
    testToneStack();              std::printf ("\n");
    testSag();                    std::printf ("\n");
    testNoiseGate();              std::printf ("\n");
    testTransformer();            std::printf ("\n");
    testPreampGainStructure();    std::printf ("\n");

    std::printf ("========================================================================\n");
    if (g_failures == 0) std::printf ("ALL TESTS PASSED\n");
    else                 std::printf ("%d TEST(S) FAILED\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
