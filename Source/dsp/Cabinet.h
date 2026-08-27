#pragma once

// ============================================================================
//  Cabinet.h — cabinet section (§8), v0.2. The only dsp/ header that depends on
//  JUCE (it wraps juce::dsp::Convolution).
//
//  Modes:
//    * Built-in analytic voicing (selectable model) — original synthesised cabs,
//      NOT captures (legal note: ship no third-party IRs).
//    * User IR — partitioned convolution of a loaded WAV.
//    * Blend — crossfade the built-in voicing against the loaded IR (a
//      mic-position-morph-style control) via Cab Mix.
//  Bypassable for players running external IRs / a real cab.
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
        irBuffer.setSize (1, (int) spec.maximumBlockSize, false, false, true);
        convReady = false;
    }

    void reset()
    {
        convolution.reset();
        voicing.reset();
    }

    void setBypassed (bool b) noexcept { bypassed = b; }
    void setUseIR    (bool u) noexcept { useIR = u; }
    void setModel    (CabModel m) noexcept { voicing.setModel (m); }
    void setCabMix   (float mix0to1) noexcept { cabMix = juce::jlimit (0.0f, 1.0f, mix0to1); }

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

    void loadIRFromMemory (const void* data, size_t sizeBytes, int maxLengthSamples = 2048)
    {
        convolution.loadImpulseResponse (data, sizeBytes,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes,
            (size_t) maxLengthSamples,
            juce::dsp::Convolution::Normalise::yes);
        convReady = true;
    }

    void processBlock (float* samples, int numSamples)
    {
        if (bypassed)
            return;

        const bool haveIR = useIR && convReady;

        if (! haveIR || cabMix <= 0.001f)
        {
            for (int n = 0; n < numSamples; ++n)
                samples[n] = voicing.process (samples[n]);
            return;
        }

        if (cabMix >= 0.999f)
        {
            juce::dsp::AudioBlock<float> block (&samples, 1, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            convolution.process (ctx);
            return;
        }

        // Blend the built-in voicing (dry path) with the convolved IR (wet path).
        if (irBuffer.getNumSamples() < numSamples)
            irBuffer.setSize (1, numSamples, false, false, true);
        float* ir = irBuffer.getWritePointer (0);
        juce::FloatVectorOperations::copy (ir, samples, numSamples);

        juce::dsp::AudioBlock<float> block (irBuffer.getArrayOfWritePointers(), 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convolution.process (ctx);

        const float a = 1.0f - cabMix, b = cabMix;
        for (int n = 0; n < numSamples; ++n)
            samples[n] = a * voicing.process (samples[n]) + b * ir[n];
    }

    bool isBypassed() const noexcept { return bypassed; }

    int getLatency() const noexcept
    {
        return (useIR && convReady) ? (int) convolution.getLatency() : 0;
    }

private:
    double sampleRate = 44100.0;
    bool   bypassed = false, useIR = false, convReady = false;
    float  cabMix = 1.0f;

    juce::dsp::Convolution  convolution;
    SpeakerVoicing          voicing;
    juce::AudioBuffer<float> irBuffer;
};

} // namespace tekk
