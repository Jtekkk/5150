#pragma once

// ============================================================================
//  Cabinet.h — cabinet section (§8).  The ONLY dsp/ header that depends on JUCE
//  (it wraps juce::dsp::Convolution for partitioned IR convolution).
//
//  Two modes:
//    * User IR loaded  → partitioned convolution of a WAV impulse response
//      (normalised, optionally truncated, sample-rate-converted on load by JUCE).
//    * No IR           → the built-in analytic SpeakerVoicing (an original 4x12
//      "V30-style" filter, NOT a capture — see the legal note in the spec).
//
//  Bypassable for players running external IRs or a real cab.
// ============================================================================

#include <juce_dsp/juce_dsp.h>
#include "SpeakerVoicing.h"

namespace tekk
{

class Cabinet
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        convolution.prepare (spec);
        voicing.prepare (spec.sampleRate);
        convReady = false;
    }

    void reset()
    {
        convolution.reset();
        voicing.reset();
    }

    void setBypassed (bool b) noexcept { bypassed = b; }
    void setUseIR    (bool u) noexcept { useIR = u; }

    // Load an impulse response from a file. JUCE normalises, trims and resamples
    // it to the host rate. Falls back to the built-in voicing on failure.
    bool loadIRFromFile (const juce::File& file, int maxLengthSamples = 2048)
    {
        if (! file.existsAsFile())
            return false;

        convolution.loadImpulseResponse (file,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes,
            (size_t) maxLengthSamples,
            juce::dsp::Convolution::Normalise::yes);
        convReady = true;
        useIR = true;
        return true;
    }

    // Load an IR from raw memory (e.g. a factory IR embedded in BinaryData).
    void loadIRFromMemory (const void* data, size_t sizeBytes, int maxLengthSamples = 2048)
    {
        convolution.loadImpulseResponse (data, sizeBytes,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes,
            (size_t) maxLengthSamples,
            juce::dsp::Convolution::Normalise::yes);
        convReady = true;
    }

    // Mono in-place block processing (the amp is mono up to the cab).
    void processBlock (float* samples, int numSamples)
    {
        if (bypassed)
            return;

        if (useIR && convReady)
        {
            juce::dsp::AudioBlock<float> block (&samples, 1, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            convolution.process (ctx);
        }
        else
        {
            for (int n = 0; n < numSamples; ++n)
                samples[n] = voicing.process (samples[n]);
        }
    }

    bool isBypassed() const noexcept { return bypassed; }

    // Convolution PDC latency (0 when running the built-in voicing).
    int getLatency() const noexcept
    {
        return (useIR && convReady) ? (int) convolution.getLatency() : 0;
    }

private:
    double sampleRate = 44100.0;
    bool   bypassed = false, useIR = false, convReady = false;

    juce::dsp::Convolution convolution;
    SpeakerVoicing         voicing;
};

} // namespace tekk
