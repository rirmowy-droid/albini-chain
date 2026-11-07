#include "PluginProcessor.h"
#include "PluginEditor.h"

AlbiniChainAudioProcessor::AlbiniChainAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    pInputGain  = apvts.getRawParameterValue ("inputGain");
    pDrive      = apvts.getRawParameterValue ("drive");
    pCabBlend   = apvts.getRawParameterValue ("cabBlend");
    pRoomMix    = apvts.getRawParameterValue ("roomMix");
    pRoomSize   = apvts.getRawParameterValue ("roomSize");
    pConsole    = apvts.getRawParameterValue ("console");
    pTape       = apvts.getRawParameterValue ("tape");
    pOutputGain = apvts.getRawParameterValue ("outputGain");
}

AlbiniChainAudioProcessor::~AlbiniChainAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout AlbiniChainAudioProcessor::createParameterLayout()
{
    using P   = juce::AudioParameterFloat;
    using ID  = juce::ParameterID;
    using Att = juce::AudioParameterFloatAttributes;

    const auto db = Att().withLabel ("dB");
    const auto pct = Att().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    const auto blendText = Att().withStringFromValueFunction ([] (float v, int)
    {
        const int b = juce::roundToInt (v * 100.0f);
        return juce::String ("A ") + juce::String (100 - b) + " / B " + juce::String (b);
    });

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<P> (ID { "inputGain", 1 },  "Input Gain",  juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f, db));
    params.push_back (std::make_unique<P> (ID { "drive", 1 },      "Drive",       juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    params.push_back (std::make_unique<P> (ID { "cabBlend", 1 },   "Cab A/B",     juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, blendText));
    params.push_back (std::make_unique<P> (ID { "roomMix", 1 },    "Room Mix",    juce::NormalisableRange<float> (0.0f, 0.5f), 0.12f, pct));
    params.push_back (std::make_unique<P> (ID { "roomSize", 1 },   "Room Size",   juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    params.push_back (std::make_unique<P> (ID { "console", 1 },    "Console",     juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    params.push_back (std::make_unique<P> (ID { "tape", 1 },       "Tape",        juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    params.push_back (std::make_unique<P> (ID { "outputGain", 1 }, "Output Gain", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db));

    return { params.begin(), params.end() };
}

bool AlbiniChainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void AlbiniChainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    const juce::dsp::ProcessSpec spec { sampleRate,
                                        (juce::uint32) maxBlockSize,
                                        (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };

    amp.prepare (spec);
    cab.prepare (spec);
    room.prepare (spec);
    console.prepare (spec);
    tape.prepare (spec);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.05);

    setLatencySamples (juce::roundToInt (amp.getLatencyInSamples()
                                         + console.getLatencyInSamples()
                                         + (float) tape.getLatencyInSamples()));
}

void AlbiniChainAudioProcessor::releaseResources() {}

void AlbiniChainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();

    // Mono in -> stereo out: duplicate the guitar to both sides
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    else
        for (auto ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());

    amp.setGain   (pInputGain->load());
    amp.setDrive  (pDrive->load());
    cab.setBlend  (pCabBlend->load());
    room.setMix   (pRoomMix->load());
    room.setSize  (pRoomSize->load());
    console.setAmount (pConsole->load());
    tape.setAmount    (pTape->load());
    outputGain.setGainDecibels (pOutputGain->load());

    juce::dsp::AudioBlock<float> full (buffer);
    full = full.getSubsetChannelBlock (0, (size_t) juce::jmax (1, numOut));

    // Some hosts send blocks larger than announced: process in safe chunks.
    const auto total = full.getNumSamples();
    for (size_t pos = 0; pos < total; pos += (size_t) maxBlockSize)
    {
        auto chunk = full.getSubBlock (pos, juce::jmin ((size_t) maxBlockSize, total - pos));
        processChunk (chunk);
    }
}

void AlbiniChainAudioProcessor::processChunk (juce::dsp::AudioBlock<float>& block)
{
    amp.process (block);
    cab.process (block);
    room.process (block);
    console.process (block);
    tape.process (block);

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    outputGain.process (ctx);
}

// ---------------------------------------------------------------------------
// Cabinet IRs
// ---------------------------------------------------------------------------
juce::Identifier AlbiniChainAudioProcessor::irProperty (int slot)
{
    static const juce::Identifier a ("irPathA"), b ("irPathB");
    return slot == 0 ? a : b;
}

bool AlbiniChainAudioProcessor::loadCabIR (int slot, const juce::File& file)
{
    if (! cab.loadIR (slot, file))
        return false;

    apvts.state.setProperty (irProperty (slot), file.getFullPathName(), nullptr);
    return true;
}

void AlbiniChainAudioProcessor::clearCabIR (int slot)
{
    cab.clearIR (slot);
    apvts.state.setProperty (irProperty (slot), juce::String(), nullptr);
}

juce::String AlbiniChainAudioProcessor::getCabIRDisplayName (int slot) const
{
    const auto path = apvts.state.getProperty (irProperty (slot)).toString();
    if (path.isEmpty())
        return "(none)";

    const juce::File f (path);
    return cab.hasIR (slot) ? f.getFileName() : "missing: " + f.getFileName();
}

void AlbiniChainAudioProcessor::restoreCabIRsFromState()
{
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto path = apvts.state.getProperty (irProperty (slot)).toString();
        if (path.isNotEmpty() && juce::File (path).existsAsFile())
            cab.loadIR (slot, juce::File (path));
        else
            cab.clearIR (slot); // path kept in state, UI shows "missing"
    }
}

// ---------------------------------------------------------------------------
juce::AudioProcessorEditor* AlbiniChainAudioProcessor::createEditor()
{
    return new AlbiniChainAudioProcessorEditor (*this);
}

void AlbiniChainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AlbiniChainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        restoreCabIRsFromState();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AlbiniChainAudioProcessor();
}
