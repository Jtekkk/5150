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
    lnf.setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xff23252c));
    lnf.setColour (juce::ComboBox::textColourId,              juce::Colour (0xffcfd2d8));
    lnf.setColour (juce::TextButton::buttonColourId,          juce::Colour (0xff2a2d35));
    setLookAndFeel (&lnf);

    // ---- Preset bar --------------------------------------------------------
    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
    addAndMakeVisible (presetLabel);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) processor.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presetBox);

    prevPreset.onClick = [this]
    {
        const int n = processor.getNumPrograms();
        processor.setCurrentProgram ((processor.getCurrentProgram() - 1 + n) % n);
        refreshPresetBox();
    };
    nextPreset.onClick = [this]
    {
        const int n = processor.getNumPrograms();
        processor.setCurrentProgram ((processor.getCurrentProgram() + 1) % n);
        refreshPresetBox();
    };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);

    // ---- Knobs — row 1 (amp) ----------------------------------------------
    addKnob (P::inputGain, "Input");
    addKnob (P::preGain,   "Pre Gain");
    addKnob (P::bass,      "Bass");
    addKnob (P::mid,       "Mid");
    addKnob (P::treble,    "Treble");
    addKnob (P::presence,  "Presence");
    addKnob (P::resonance, "Resonance");
    addKnob (P::depth,     "Depth");
    addKnob (P::postGain,  "Post Gain");
    addKnob (P::output,    "Output");
    // ---- Knobs — row 2 (feel / boost / gate / cab) ------------------------
    addKnob (P::tightness,   "Tightness");
    addKnob (P::sag,         "Sag");
    addKnob (P::mix,         "Mix");
    addKnob (P::boostDrive,  "Boost Drv");
    addKnob (P::boostLevel,  "Boost Lvl");
    addKnob (P::gateThresh,  "Gate Thr");
    addKnob (P::gateRelease, "Gate Rel");
    addKnob (P::cabMix,      "Cab Mix");

    // ---- Switches / combos -------------------------------------------------
    addCombo  (P::channel,      "Channel",   { "Rhythm", "Lead" });
    addCombo  (P::gatePosition, "Gate Pos",  { "Pre", "Post" });
    addCombo  (P::cabModel,     "Cab",       { "V30 4x12", "Greenback 4x12", "Modern 2x12", "Vintage 1x12" });
    addCombo  (P::osQuality,    "OS Qual",   { "2x", "4x", "8x", "16x" });
    addCombo  (P::osType,       "OS Type",   { "IIR (Live)", "FIR (Linear)" });
    addToggle (P::bright,  "Bright");
    addToggle (P::crunch,  "Crunch");
    addToggle (P::boostOn, "Boost");
    addToggle (P::gateOn,  "Gate");
    addToggle (P::cabOn,   "Cab");
    addToggle (P::useIR,   "Use IR");

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

    setSize (1040, 640);
    startTimerHz (24);
}

RedlineAudioProcessorEditor::~RedlineAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void RedlineAudioProcessorEditor::refreshPresetBox()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

RedlineAudioProcessorEditor::Knob& RedlineAudioProcessorEditor::addKnob (const juce::String& paramID,
                                                                        const juce::String& text)
{
    auto* k = new Knob();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
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
    lbl->setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Bold")));
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
    g.drawText ("120W 6L6 high-gain  ·  v0.2  ·  TEKK Audio Labs",
                header.reduced (16, 0), juce::Justification::centredRight);

    // Section separators behind the two knob rows.
    auto body = getLocalBounds();
    body.removeFromTop (46 + 36);
    body.removeFromBottom (26);
    body = body.reduced (10, 6);
    g.setColour (juce::Colour (0xff1d1f25));
    g.fillRoundedRectangle (body.removeFromTop (118).toFloat(), 6.0f);
    body.removeFromTop (6);
    g.fillRoundedRectangle (body.removeFromTop (118).toFloat(), 6.0f);

    // Meters (bottom strip).
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
    area.removeFromTop (46);   // header

    // ---- Preset bar --------------------------------------------------------
    auto bar = area.removeFromTop (36).reduced (12, 4);
    presetLabel.setBounds (bar.removeFromLeft (52));
    prevPreset.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (4);
    presetBox.setBounds (bar.removeFromLeft (220));
    bar.removeFromLeft (4);
    nextPreset.setBounds (bar.removeFromLeft (28));
    loadIRButton.setBounds (bar.removeFromRight (110));

    area.removeFromBottom (26);        // meters
    area = area.reduced (10, 6);

    const int knobH = 112;

    auto layoutKnobRow = [] (juce::Rectangle<int> row, juce::OwnedArray<Knob>& ks, int first, int count)
    {
        if (count <= 0) return;
        const int w = row.getWidth() / count;
        for (int i = 0; i < count; ++i)
        {
            auto cell = row.removeFromLeft (w).reduced (2, 4);
            ks[first + i]->label.setBounds (cell.removeFromTop (16));
            ks[first + i]->slider.setBounds (cell);
        }
    };

    layoutKnobRow (area.removeFromTop (knobH + 6), knobs, 0, 10);   // amp row
    area.removeFromTop (6);
    layoutKnobRow (area.removeFromTop (knobH + 6), knobs, 10, 8);   // feel/boost/gate/cab row

    // ---- Switch / combo panel ---------------------------------------------
    area.removeFromTop (8);
    auto panel = area.reduced (4, 0);

    // Combos across the top of the panel.
    auto comboRow = panel.removeFromTop (44);
    const int cw = comboRow.getWidth() / (int) combos.size();
    for (int i = 0; i < combos.size(); ++i)
    {
        auto cell = comboRow.removeFromLeft (cw).reduced (6, 2);
        comboLabels[i]->setBounds (cell.removeFromTop (16));
        combos[i]->setBounds (cell.removeFromTop (24));
    }

    // Toggles in a row below.
    panel.removeFromTop (6);
    auto toggleRow = panel.removeFromTop (28);
    if (toggles.size() > 0)
    {
        const int tw = toggleRow.getWidth() / toggles.size();
        for (int i = 0; i < toggles.size(); ++i)
            toggles[i]->setBounds (toggleRow.removeFromLeft (tw).reduced (4, 0));
    }
}

void RedlineAudioProcessorEditor::timerCallback()
{
    inMeter  = juce::jmax (processor.getInputLevel(),  inMeter  * 0.8f);
    outMeter = juce::jmax (processor.getOutputLevel(), outMeter * 0.8f);
    if (presetBox.getSelectedId() != processor.getCurrentProgram() + 1)
        refreshPresetBox();
    repaint();
}
