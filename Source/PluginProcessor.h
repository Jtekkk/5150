#pragma once

// ============================================================================
//  PluginProcessor.h — TEKK Redline 120.
//
//  Original name / branding (the legal note at the top of the spec): this ships
//  as "Redline 120", voiced after a classic American 120 W 6L6 high-gain head.
//  No trademarked names, logos, or third-party captured impulse responses are
//  distributed with it.
//
//  Signal chain (§3):
//     input gain → DC block → [gate:pre] → [TS boost]
//        →‖ oversampled: preamp cascade → tone stack → phase inverter
//                        → power amp (sag) → output transformer → NFB ‖
//        → cabinet (IR / built-in voicing) → [gate:post] → output gain
// ============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "ParameterIDs.h"
#include "Presets.h"
#include "dsp/DCBlocker.h"
#include "dsp/ScreamerBoost.h"
#include "dsp/PreampCascade.h"
#include "dsp/ToneStack.h"
#include "dsp/PhaseInverter.h"
#include "dsp/PowerAmp.h"
#include "dsp/TransformerJA.h"
#include "dsp/NegativeFeedback.h"
#include "dsp/NoiseGate.h"
#include "dsp/Cabinet.h"

class RedlineAudioProcessor : public juce::AudioProcessor
{
public:
    RedlineAudioProcessor();
    ~RedlineAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Redline 120"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return (int) tekk::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Called by the editor's "Load IR" button.
    bool loadImpulseResponse (const juce::File& file);

    // For the editor's meters.
    float getInputLevel()  const noexcept { return inputLevel.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }

private:
    void rebuildOversampling (int order, int type);
    void pullParameters();
    void applyPreset (int index);

    double hostSampleRate = 44100.0;
    double innerSampleRate = 176400.0;   // host rate × oversampling factor
    int    currentOsOrder = 2;           // log2(factor); default 4×
    int    currentOsType = 0;            // 0 = IIR (live), 1 = FIR (linear phase)
    int    currentBlockSize = 512;
    int    currentProgram = 0;

    // --- DSP blocks ----------------------------------------------------------
    juce::dsp::Gain<float>  inputGain, outputGain;
    tekk::DCBlocker         dcBlock;
    tekk::ScreamerBoost     boost;
    tekk::PreampCascade     preamp;
    tekk::ToneStack         toneStack;
    tekk::PhaseInverter     phaseInverter;
    tekk::PowerAmp          powerAmp;
    tekk::JATransformer     outputTransformer;
    tekk::NegativeFeedback  nfb;
    tekk::Cabinet           cabinet;
    tekk::NoiseGate         gate;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Smoothed continuous controls (≈15 ms) to avoid zipper noise (§9).
    juce::SmoothedValue<float> smBass, smMid, smTreble, smPresence, smResonance, smPreGain;
    juce::SmoothedValue<float> smMix;   // dry/wet blend

    juce::AudioBuffer<float> monoBuffer, dryBuffer;

    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedlineAudioProcessor)
};
