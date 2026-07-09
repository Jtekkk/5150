#include "PluginEditor.h"
#include "ParameterIDs.h"

namespace P = tekk::params;

RedlineAudioProcessorEditor::RedlineAudioProcessorEditor (RedlineAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lnf.setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff17181c));
    lnf.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffd8412f));
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3d44));
    lnf.setColour (juce::Slider::thumbColourId,               juce::Colour (0xffe8e6df));
    lnf.setColour (juce::Label::textColourId,                 juce::Colour (0xffcfd2d8));
    setLookAndFeel (&lnf);

    // Main tone/gain knobs (top row).
    addKnob (P::inputGain, "Input");
    addKnob (P::preGain,   "Pre Gain");
    addKnob (P::bass,      "Bass");
    addKnob (P::mid,       "Mid");
    addKnob (P::treble,    "Treble");
    addKnob (P::resonance, "Resonance");
    addKnob (P::presence,  "Presence");
    addKnob (P::postGain,  "Post Gain");
    addKnob (P::sag,       "Sag");
    addKnob (P::output,    "Output");

    // Boost + gate knobs (secondary row).
    addKnob (P::boostDrive,  "Boost Drv");
    addKnob (P::boostLevel,  "Boost Lvl");
    addKnob (P::gateThresh,  "Gate Thr");
    addKnob (P::gateRelease, "Gate Rel");

    addCombo  (P::channel,      "Channel",  { "Rhythm", "Lead" });
    addToggle (P::bright,       "Bright");
    addToggle (P::crunch,       "Crunch");
    addToggle (P::boostOn,      "Boost");
    addToggle (P::gateOn,       "Gate");
    addCombo  (P::gatePosition, "Gate Pos", { "Pre", "Post" });
    addToggle (P::cabOn,        "Cab");
    addToggle (P::useIR,        "Use IR");
    addCombo  (P::osQuality,    "OS Quality", { "2x", "4x", "8x", "16x" });

    loadIRButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Select a cabinet impulse response",
                                                           juce::File{}, "*.wav;*.aiff");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                    processor.loadImpulseResponse (f);
            });
    };
    addAndMakeVisible (loadIRButton);

    setSize (940, 520);
    startTimerHz (24);
}

RedlineAudioProcessorEditor::~RedlineAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

RedlineAudioProcessorEditor::Knob& RedlineAudioProcessorEditor::addKnob (const juce::String& paramID,
                                                                        const juce::String& text)
{
    auto* k = new Knob();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 16);
    addAndMakeVisible (k->slider);

    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
    addAndMakeVisible (k->label);

    k->attach = std::make_unique<APVTS::SliderAttachment> (processor.apvts, paramID, k->slider);
    knobs.add (k);
    return *k;
}

juce::ToggleButton& RedlineAudioProcessorEditor::addToggle (const juce::String& paramID, const juce::String& text)
{
    auto* b = new juce::ToggleButton (text);
    addAndMakeVisible (b);
    toggles.add (b);
    toggleAttachments.add (new APVTS::ButtonAttachment (processor.apvts, paramID, *b));
    return *b;
}

juce::ComboBox& RedlineAudioProcessorEditor::addCombo (const juce::String& paramID, const juce::String& text,
                                                       const juce::StringArray& items)
{
    auto* lbl = new juce::Label ({}, text);
    lbl->setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
    lbl->setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (lbl);
    comboLabels.add (lbl);

    auto* c = new juce::ComboBox();
    c->addItemList (items, 1);
    addAndMakeVisible (c);
    combos.add (c);
    comboAttachments.add (new APVTS::ComboBoxAttachment (processor.apvts, paramID, *c));
    return *c;
}

void RedlineAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17181c));

    auto header = getLocalBounds().removeFromTop (46);
    g.setColour (juce::Colour (0xff20222a));
    g.fillRect (header);
    g.setColour (juce::Colour (0xffd8412f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (22.0f).withStyle ("Bold")));
    g.drawText ("REDLINE 120", header.reduced (16, 0), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8a8f99));
    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    g.drawText ("120W 6L6 high-gain — TEKK Audio Labs",
                header.reduced (16, 0), juce::Justification::centredRight);

    // Input / output meters (bottom strip).
    auto strip = getLocalBounds().removeFromBottom (26).reduced (16, 6);
    auto drawMeter = [&g] (juce::Rectangle<int> r, float level, const juce::String& name)
    {
        g.setColour (juce::Colour (0xff2a2d35));
        g.fillRect (r);
        const float norm = juce::jlimit (0.0f, 1.0f, juce::Decibels::gainToDecibels (level) / 60.0f + 1.0f);
        g.setColour (norm > 0.85f ? juce::Colour (0xffd8412f) : juce::Colour (0xff4fae5a));
        g.fillRect (r.removeFromLeft ((int) (r.getWidth() * norm)));
        g.setColour (juce::Colour (0xffcfd2d8));
        g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
        g.drawText (name, r, juce::Justification::centredLeft);
    };
    drawMeter (strip.removeFromLeft (strip.getWidth() / 2 - 8), inMeter,  " IN");
    strip.removeFromLeft (16);
    drawMeter (strip, outMeter, " OUT");
}

void RedlineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (46);      // header
    area.removeFromBottom (26);   // meters
    area = area.reduced (14, 8);

    const int knobW = 86, knobH = 96;

    // Row 1: the ten main knobs.
    auto row1 = area.removeFromTop (knobH + 6);
    for (int i = 0; i < 10 && i < knobs.size(); ++i)
    {
        auto cell = row1.removeFromLeft (knobW);
        knobs[i]->label.setBounds (cell.removeFromTop (16));
        knobs[i]->slider.setBounds (cell);
    }

    area.removeFromTop (6);
    auto row2 = area.removeFromTop (knobH + 6);

    // Row 2 left: boost + gate knobs.
    for (int i = 10; i < 14 && i < knobs.size(); ++i)
    {
        auto cell = row2.removeFromLeft (knobW);
        knobs[i]->label.setBounds (cell.removeFromTop (16));
        knobs[i]->slider.setBounds (cell);
    }

    // Row 2 right: switches / combos / IR button laid out in a small grid.
    auto panel = row2.reduced (10, 4);
    const int lineH = 26;
    auto placeCombo = [&] (int idx)
    {
        auto line = panel.removeFromTop (lineH);
        comboLabels[idx]->setBounds (line.removeFromLeft (86));
        combos[idx]->setBounds (line.removeFromLeft (120));
    };
    auto placeToggles = [&] (std::initializer_list<int> idxs)
    {
        auto line = panel.removeFromTop (lineH);
        for (int i : idxs) toggles[i]->setBounds (line.removeFromLeft (100));
    };

    placeCombo (0);                    // Channel
    placeToggles ({ 0, 1, 2 });        // Bright / Crunch / Boost
    placeToggles ({ 3, 4, 5 });        // Gate / Cab / Use IR
    placeCombo (1);                    // Gate Pos
    placeCombo (2);                    // OS Quality
    auto irLine = panel.removeFromTop (lineH);
    loadIRButton.setBounds (irLine.removeFromLeft (120));
}

void RedlineAudioProcessorEditor::timerCallback()
{
    // Ballistic decay towards the processor's RMS reading.
    inMeter  = juce::jmax (processor.getInputLevel(),  inMeter  * 0.8f);
    outMeter = juce::jmax (processor.getOutputLevel(), outMeter * 0.8f);
    repaint();
}
