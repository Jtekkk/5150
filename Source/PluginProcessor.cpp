#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tekk;

// ============================================================================
//  Parameter layout (§9)
// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout tekk::params::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbGain = [] (const char* id, const char* name)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, params::stateVersion }, name,
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f);
    };
    auto knob10 = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, params::stateVersion }, name,
                                                       NormalisableRange<float> (0.0f, 10.0f, 0.01f), def);
    };

    p.push_back (dbGain (params::inputGain, "Input Gain"));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { params::channel, params::stateVersion },
                    "Channel", StringArray { "Rhythm", "Lead" }, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::bright, params::stateVersion }, "Bright", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::crunch, params::stateVersion }, "Crunch", false));

    p.push_back (knob10 (params::preGain,   "Pre Gain",  5.0f));
    p.push_back (knob10 (params::bass,      "Bass",      5.0f));
    p.push_back (knob10 (params::mid,       "Mid",       5.0f));
    p.push_back (knob10 (params::treble,    "Treble",    6.0f));
    p.push_back (knob10 (params::resonance, "Resonance", 5.0f));
    p.push_back (knob10 (params::presence,  "Presence",  5.0f));
    p.push_back (knob10 (params::postGain,  "Post Gain", 5.0f));
    p.push_back (knob10 (params::sag,       "Sag",       4.0f));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::boostOn, params::stateVersion }, "Boost", false));
    p.push_back (knob10 (params::boostDrive, "Boost Drive", 3.0f));
    p.push_back (knob10 (params::boostLevel, "Boost Level", 7.0f));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::gateOn, params::stateVersion }, "Gate", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { params::gateThresh, params::stateVersion },
                    "Gate Threshold", NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -60.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { params::gateRelease, params::stateVersion },
                    "Gate Release", NormalisableRange<float> (10.0f, 500.0f, 1.0f), 120.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { params::gatePosition, params::stateVersion },
                    "Gate Position", StringArray { "Pre", "Post" }, 0));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::cabOn, params::stateVersion }, "Cab", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::useIR, params::stateVersion }, "Use IR", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { params::osQuality, params::stateVersion },
                    "OS Quality", StringArray { "2x", "4x", "8x", "16x" }, 1));

    p.push_back (dbGain (params::output, "Output"));

    return { p.begin(), p.end() };
}

// ============================================================================
RedlineAudioProcessor::RedlineAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", params::createLayout())
{
}

bool RedlineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || in == juce::AudioChannelSet::mono() || in.isDisabled() == false;
}

void RedlineAudioProcessor::rebuildOversampling (int order)
{
    currentOsOrder = order;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, (size_t) order,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,  // low-latency IIR (live use)
        true, true);
    oversampling->initProcessing ((size_t) juce::jmax (1, currentBlockSize));
    innerSampleRate = hostSampleRate * (double) (1 << order);
}

void RedlineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate  = sampleRate;
    currentBlockSize = samplesPerBlock;

    const int order = params::osChoiceToOrder ((int) apvts.getRawParameterValue (params::osQuality)->load());
    rebuildOversampling (order);

    // Blocks inside the oversampled region run at innerSampleRate.
    boost.prepare (innerSampleRate);
    preamp.prepare (innerSampleRate);
    toneStack.prepare (innerSampleRate);
    phaseInverter.prepare (innerSampleRate);
    powerAmp.prepare (innerSampleRate);
    outputTransformer.prepare (innerSampleRate);
    nfb.prepare (innerSampleRate);

    // Base-rate blocks.
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    dcBlock.prepare (sampleRate, 8.0);
    cabinet.prepare (spec);
    gate.prepare (sampleRate);
    inputGain.prepare (spec);
    outputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.01);
    outputGain.setRampDurationSeconds (0.01);

    auto initSmooth = [sampleRate] (juce::SmoothedValue<float>& s, float v)
    { s.reset (sampleRate, 0.02); s.setCurrentAndTargetValue (v); };
    initSmooth (smBass,      5.0f); initSmooth (smMid,       5.0f); initSmooth (smTreble,   6.0f);
    initSmooth (smPresence,  5.0f); initSmooth (smResonance, 5.0f); initSmooth (smPreGain,  5.0f);

    monoBuffer.setSize (1, samplesPerBlock, false, false, true);

    setLatencySamples ((int) std::round (oversampling->getLatencyInSamples())
                       + (int) cabinet.getLatency());
}

void RedlineAudioProcessor::pullParameters()
{
    // --- Channel / voicing switches -----------------------------------------
    preamp.setChannel ((int) apvts.getRawParameterValue (params::channel)->load() == 1
                       ? Channel::Lead : Channel::Rhythm);
    preamp.setBright (apvts.getRawParameterValue (params::bright)->load() > 0.5f);
    preamp.setCrunch (apvts.getRawParameterValue (params::crunch)->load() > 0.5f);

    // --- Boost ---------------------------------------------------------------
    boost.setEnabled (apvts.getRawParameterValue (params::boostOn)->load() > 0.5f);
    boost.setDrive (apvts.getRawParameterValue (params::boostDrive)->load());
    boost.setLevel (apvts.getRawParameterValue (params::boostLevel)->load());

    // --- Power amp -----------------------------------------------------------
    powerAmp.setDrive (apvts.getRawParameterValue (params::postGain)->load());
    powerAmp.setSag (apvts.getRawParameterValue (params::sag)->load() / 10.0f);

    // --- Gate ----------------------------------------------------------------
    gate.setThreshold (apvts.getRawParameterValue (params::gateThresh)->load());
    gate.setTimes (0.5, 40.0, (double) apvts.getRawParameterValue (params::gateRelease)->load());

    // --- Cab -----------------------------------------------------------------
    cabinet.setBypassed (apvts.getRawParameterValue (params::cabOn)->load() < 0.5f);
    cabinet.setUseIR (apvts.getRawParameterValue (params::useIR)->load() > 0.5f);

    // --- Gains ---------------------------------------------------------------
    inputGain.setGainDecibels (apvts.getRawParameterValue (params::inputGain)->load());
    outputGain.setGainDecibels (apvts.getRawParameterValue (params::output)->load());

    // Smoothed continuous controls: retarget here, sampled per block below.
    smBass.setTargetValue      (apvts.getRawParameterValue (params::bass)->load());
    smMid.setTargetValue       (apvts.getRawParameterValue (params::mid)->load());
    smTreble.setTargetValue    (apvts.getRawParameterValue (params::treble)->load());
    smPresence.setTargetValue  (apvts.getRawParameterValue (params::presence)->load());
    smResonance.setTargetValue (apvts.getRawParameterValue (params::resonance)->load());
    smPreGain.setTargetValue   (apvts.getRawParameterValue (params::preGain)->load());
}

void RedlineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;   // FTZ/DAZ — reactive/feedback states (§15)

    const int numSamples = buffer.getNumSamples();
    const int numOut = getTotalNumOutputChannels();

    // Hot-swap oversampling quality if the user changed it (rare; reallocs).
    const int wantedOrder = params::osChoiceToOrder ((int) apvts.getRawParameterValue (params::osQuality)->load());
    if (wantedOrder != currentOsOrder)
    {
        rebuildOversampling (wantedOrder);
        boost.prepare (innerSampleRate);       preamp.prepare (innerSampleRate);
        toneStack.prepare (innerSampleRate);   phaseInverter.prepare (innerSampleRate);
        powerAmp.prepare (innerSampleRate);     outputTransformer.prepare (innerSampleRate);
        nfb.prepare (innerSampleRate);
        setLatencySamples ((int) std::round (oversampling->getLatencyInSamples())
                           + (int) cabinet.getLatency());
    }

    pullParameters();

    // --- Downmix to the mono amp core ---------------------------------------
    monoBuffer.setSize (1, numSamples, false, false, true);
    float* mono = monoBuffer.getWritePointer (0);
    if (buffer.getNumChannels() >= 2)
        for (int n = 0; n < numSamples; ++n)
            mono[n] = 0.5f * (buffer.getSample (0, n) + buffer.getSample (1, n));
    else
        juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0), numSamples);

    inputLevel.store (monoBuffer.getRMSLevel (0, 0, numSamples));

    // Input gain + DC block (base rate).
    { juce::dsp::AudioBlock<float> b (monoBuffer);
      juce::dsp::ProcessContextReplacing<float> ctx (b);
      inputGain.process (ctx); }
    for (int n = 0; n < numSamples; ++n) mono[n] = dcBlock.process (mono[n]);

    const bool gateEnabled = apvts.getRawParameterValue (params::gateOn)->load() > 0.5f;
    const bool gatePost    = (int) apvts.getRawParameterValue (params::gatePosition)->load() == 1;

    // Gate in front (tight chug): detector = the clean pre-preamp signal (§10).
    if (gateEnabled && ! gatePost)
        for (int n = 0; n < numSamples; ++n) mono[n] = gate.process (mono[n]);

    // Per-block control-rate values for the tone stack / preamp / NFB.
    const float bassV = smBass.skip (numSamples);
    const float midV  = smMid.skip (numSamples);
    const float trebV = smTreble.skip (numSamples);
    const float presV = smPresence.skip (numSamples);
    const float resoV = smResonance.skip (numSamples);
    const float preV  = smPreGain.skip (numSamples);

    preamp.setPreGain (preV);
    toneStack.setControls (trebV / 10.0f, midV / 10.0f, bassV / 10.0f);
    nfb.setPresence (presV);
    nfb.setResonance (resoV);

    // --- Oversampled amp core -----------------------------------------------
    juce::dsp::AudioBlock<float> baseBlock (monoBuffer);
    auto osBlock = oversampling->processSamplesUp (baseBlock);
    float* os = osBlock.getChannelPointer (0);
    const int osN = (int) osBlock.getNumSamples();
    for (int i = 0; i < osN; ++i)
    {
        float s = os[i];
        s = boost.process (s);
        s = preamp.process (s);
        s = toneStack.process (s);
        s = phaseInverter.process (s);
        s = powerAmp.process (s);
        s = outputTransformer.process (s);
        s = nfb.process (s);
        os[i] = s;
    }
    oversampling->processSamplesDown (baseBlock);

    // --- Cabinet (base rate) -------------------------------------------------
    cabinet.processBlock (mono, numSamples);

    // Smart post-cab gate (more natural release) if selected.
    if (gateEnabled && gatePost)
        for (int n = 0; n < numSamples; ++n) mono[n] = gate.process (mono[n]);

    // Output gain.
    { juce::dsp::AudioBlock<float> b (monoBuffer);
      juce::dsp::ProcessContextReplacing<float> ctx (b);
      outputGain.process (ctx); }

    outputLevel.store (monoBuffer.getRMSLevel (0, 0, numSamples));

    // Fan the mono amp out to every output channel.
    for (int ch = 0; ch < numOut; ++ch)
        juce::FloatVectorOperations::copy (buffer.getWritePointer (ch), mono, numSamples);
}

bool RedlineAudioProcessor::loadImpulseResponse (const juce::File& file)
{
    const bool ok = cabinet.loadIRFromFile (file);
    if (ok)
        if (auto* pv = apvts.getParameter (params::useIR))
            pv->setValueNotifyingHost (1.0f);
    return ok;
}

void RedlineAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RedlineAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RedlineAudioProcessor::createEditor()
{
    return new RedlineAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RedlineAudioProcessor();
}
