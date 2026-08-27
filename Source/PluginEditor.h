#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ============================================================================
//  PluginEditor.h — control-panel GUI.
//
//  Deliberately a clean, honest control surface (rotaries + switches + meters)
//  rather than a photoreal amp face. Every control is bound to the APVTS so it
//  survives host automation and preset recall.
// ============================================================================

class RedlineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit RedlineAudioProcessorEditor (RedlineAudioProcessor&);
    ~RedlineAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };
    Knob& addKnob (const juce::String& paramID, const juce::String& text);
    juce::ToggleButton& addToggle (const juce::String& paramID, const juce::String& text);
    juce::ComboBox&     addCombo  (const juce::String& paramID, const juce::String& text,
                                   const juce::StringArray& items);

    RedlineAudioProcessor& processor;

    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> comboLabels;
    juce::OwnedArray<APVTS::ButtonAttachment> toggleAttachments;
    juce::OwnedArray<APVTS::ComboBoxAttachment> comboAttachments;

    juce::TextButton loadIRButton { "Load IR…" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Preset selector (drives the processor's program, not an APVTS param).
    juce::ComboBox   presetBox;
    juce::Label      presetLabel;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    void refreshPresetBox();

    // Simple meters.
    float inMeter = 0.0f, outMeter = 0.0f;

    juce::LookAndFeel_V4 lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedlineAudioProcessorEditor)
};
