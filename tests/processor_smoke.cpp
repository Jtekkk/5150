// ============================================================================
//  processor_smoke.cpp — end-to-end smoke test of the assembled plugin.
//
//  Instantiates RedlineAudioProcessor and runs realistic audio through the full
//  chain (oversampling + every DSP block) at several settings, asserting the
//  output stays finite, bounded and non-silent, latency is reported, and state
//  save/load round-trips. Catches integration bugs the pure-DSP unit tests
//  can't (NaNs, denormals, wiring mistakes in processBlock).
//
//  Built as a JUCE console app by CMake (target `redline_smoke`).
// ============================================================================

#include "PluginProcessor.h"
#include <cstdio>

static int g_fail = 0;
static void check (bool c, const char* name)
{
    std::printf ("  [%s] %s\n", c ? "PASS" : "FAIL", name);
    if (! c) ++g_fail;
}

// A palm-mute-ish test signal: fundamental + harmonics with a decaying envelope.
static void fillGuitar (juce::AudioBuffer<float>& buf, double fs, double f0, float amp)
{
    for (int n = 0; n < buf.getNumSamples(); ++n)
    {
        const double t = n / fs;
        const double env = std::exp (-3.0 * t) * (0.6 + 0.4 * std::sin (2.0 * M_PI * 5.0 * t));
        double s = std::sin (2.0 * M_PI * f0 * t)
                 + 0.5 * std::sin (2.0 * M_PI * 2 * f0 * t)
                 + 0.3 * std::sin (2.0 * M_PI * 3 * f0 * t);
        const float v = (float) (amp * env * s / 1.8);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            buf.setSample (ch, n, v);
    }
}

static bool runChain (RedlineAudioProcessor& p, double fs, int block, float& peakOut, bool& anySignal)
{
    juce::MidiBuffer midi;
    peakOut = 0.0f; anySignal = false;
    bool finite = true;
    for (int b = 0; b < 40; ++b)
    {
        juce::AudioBuffer<float> buf (2, block);
        fillGuitar (buf, fs, 82.41, 0.5f);   // low E
        p.processBlock (buf, midi);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int n = 0; n < block; ++n)
            {
                const float v = buf.getSample (ch, n);
                if (! std::isfinite (v)) finite = false;
                peakOut = std::max (peakOut, std::abs (v));
                if (std::abs (v) > 1.0e-4f) anySignal = true;
            }
    }
    return finite;
}

static void setParam (RedlineAudioProcessor& p, const juce::String& id, float value01)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (value01);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // message manager for APVTS
    std::printf ("============ Redline 120 — processor smoke test ============\n\n");

    const double fs = 48000.0;
    const int    block = 512;

    RedlineAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, fs, block);
    proc.prepareToPlay (fs, block);

    std::printf ("Default settings (4x OS, Lead):\n");
    float peak; bool sig;
    check (runChain (proc, fs, block, peak, sig), "output finite");
    check (sig, "output non-silent");
    check (peak < 4.0f, "output bounded (no runaway)");
    check (proc.getLatencySamples() >= 0, "latency reported");
    std::printf ("     peak=%.3f  latency=%d samples\n\n", peak, proc.getLatencySamples());

    std::printf ("Max gain, boost on, tight gate, Lead:\n");
    setParam (proc, tekk::params::preGain, 1.0f);
    setParam (proc, tekk::params::postGain, 1.0f);
    setParam (proc, tekk::params::boostOn, 1.0f);
    setParam (proc, tekk::params::channel, 1.0f);
    setParam (proc, tekk::params::gateOn, 1.0f);
    check (runChain (proc, fs, block, peak, sig), "output finite at max gain");
    check (peak < 4.0f, "bounded at max gain");
    std::printf ("     peak=%.3f\n\n", peak);

    std::printf ("Sweep oversampling qualities:\n");
    for (int q = 0; q < 4; ++q)
    {
        setParam (proc, tekk::params::osQuality, q / 3.0f);
        const bool finite = runChain (proc, fs, block, peak, sig);
        char name[64]; std::snprintf (name, sizeof name, "OS quality %dx finite & bounded", 1 << (q + 1));
        check (finite && peak < 4.0f, name);
    }
    std::printf ("     latency at current OS = %d samples\n\n", proc.getLatencySamples());

    std::printf ("Rhythm channel + built-in cab, gate post:\n");
    setParam (proc, tekk::params::channel, 0.0f);
    setParam (proc, tekk::params::gatePosition, 1.0f);
    check (runChain (proc, fs, block, peak, sig), "rhythm chain finite");
    check (sig, "rhythm chain non-silent");

    std::printf ("\nState save / load round-trip:\n");
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    const float before = proc.apvts.getRawParameterValue (tekk::params::treble)->load();
    setParam (proc, tekk::params::treble, 0.123f);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    const float after = proc.apvts.getRawParameterValue (tekk::params::treble)->load();
    check (std::abs (after - before) < 1.0e-3f, "treble restored from saved state");

    std::printf ("\nFactory presets:\n");
    check (proc.getNumPrograms() >= 6, "at least 6 factory presets present");
    // Load every preset and confirm the chain still produces finite, bounded audio.
    bool allPresetsFinite = true;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        if (! runChain (proc, fs, block, peak, sig) || peak >= 4.0f)
            { allPresetsFinite = false; std::printf ("     preset %d bad (peak=%.3f)\n", i, peak); }
    }
    check (allPresetsFinite, "all presets produce finite, bounded audio");
    // Switching to "Djent Chug" (index 2) should raise Tightness well above Init.
    proc.setCurrentProgram (0);
    const float tightInit = proc.apvts.getRawParameterValue (tekk::params::tightness)->load();
    proc.setCurrentProgram (2);
    const float tightDjent = proc.apvts.getRawParameterValue (tekk::params::tightness)->load();
    check (tightDjent > tightInit + 2.0f, "preset changes parameters (Djent tightness > Init)");

    std::printf ("\n============================================================\n");
    std::printf (g_fail == 0 ? "ALL SMOKE TESTS PASSED\n" : "%d SMOKE TEST(S) FAILED\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
