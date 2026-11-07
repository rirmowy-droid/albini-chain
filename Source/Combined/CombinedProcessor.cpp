#include "CombinedProcessor.h"
#include "CombinedEditor.h"
#include "CombinedPresets.h"

namespace
{
    struct PedalDef { const char* id; const char* name; float stonoga::Params::* mem; float lo, hi; };

    // Te same parametry co w Stonoga FX (bez ochrony uszu, która jest na końcu łańcucha).
    const PedalDef kPedal[CombinedProcessor::kNumPedalParams] = {
        { "drive",    "Przester",             &stonoga::Params::drive,    0.f, 1.f },
        { "starve",   "Głód baterii",         &stonoga::Params::starve,   0.f, 1.f },
        { "gate",     "Rozpad (bramka)",      &stonoga::Params::gate,     0.f, 1.f },
        { "octave",   "Oktawa",               &stonoga::Params::octave,   0.f, 1.f },
        { "rust",     "Rdza",                 &stonoga::Params::rust,     0.f, 1.f },
        { "crackle",  "Trzaski",              &stonoga::Params::crackle,  0.f, 1.f },
        { "tone",     "Barwa",                &stonoga::Params::tone,     0.f, 1.f },
        { "mask",     "Maska",                &stonoga::Params::mask,     0.f, 1.f },
        { "vowel",    "Samogłoska",           &stonoga::Params::vowel,    0.f, 1.f },
        { "feedback", "Sprzężenie",           &stonoga::Params::feedback, 0.f, 1.f },
        { "fbtime",   "Czas sprzężenia",      &stonoga::Params::fbTime,   0.f, 1.f },
        { "howl",     "Wycie",                &stonoga::Params::howl,     0.f, 1.f },
        { "revfb",    "Pętla pogłosu",        &stonoga::Params::revfb,    0.f, 1.f },
        { "mix",      "Mix kostki",           &stonoga::Params::mix,      0.f, 1.f },
        { "reverb",   "Pogłos 1 (śpiew)",     &stonoga::Params::reverb,   0.f, 1.f },
        { "size",     "Rozmiar 1",            &stonoga::Params::size,     0.f, 1.f },
        { "sing",     "Śpiew",                &stonoga::Params::sing,     0.f, 1.f },
        { "choir",    "Chór",                 &stonoga::Params::choir,    0.f, 1.f },
        { "soft",     "Miękkość",             &stonoga::Params::soft,     0.f, 1.f },
        { "body",     "Pełnia",               &stonoga::Params::body,     0.f, 1.f },
        { "swell",    "Mgła",                 &stonoga::Params::swell,    0.f, 1.f },
        { "chorus",   "Chorus",               &stonoga::Params::chorus,   0.f, 1.f },
        { "chrate",   "Tempo chorusa",        &stonoga::Params::chRate,   0.f, 1.f },
        { "rev2",     "Pogłos 2 (jasny)",     &stonoga::Params::rev2,     0.f, 1.f },
        { "size2",    "Rozmiar 2",            &stonoga::Params::size2,    0.f, 1.f },
        { "rev3",     "Pogłos 3 (otchłań)",   &stonoga::Params::rev3,     0.f, 1.f },
        { "size3",    "Rozmiar 3",            &stonoga::Params::size3,    0.f, 1.f },
        { "output",   "Do wzmacniacza (dB)",  &stonoga::Params::outDb,  -40.f, 6.f },
    };

    struct ChainDef { const char* id; const char* shortName; };
    const ChainDef kChain[CombinedProcessor::kNumChainParams] = {
        { "ampInput",   "Wejście"   },
        { "ampDrive",   "Drive"     },
        { "cabBlend",   "Cab A/B"   },
        { "roomMix",    "Pokój"     },
        { "roomSize",   "Rozmiar"   },
        { "console",    "Konsoleta" },
        { "tape",       "Taśma"     },
        { "outputGain", "Wyjście"   },
        { "protect",    "Ochrona uszu" },
    };

    // [0..2] na przesterze, [3..5] na całości
    const ChainDef kBus[CombinedProcessor::kNumBusParams] = {
        { "distComp",  "Kompresja" },
        { "distWidth", "Panorama"  },
        { "distSmear", "Pogłos"    },
        { "busComp",   "Kompresja" },
        { "width",     "Panorama"  },
        { "smear",     "Pogłos"    },
    };

    juce::String pl (const char* s) { return juce::String::fromUTF8 (s); }
}

const char* CombinedProcessor::pedalParamId (int i)        { return kPedal[i].id; }
const char* CombinedProcessor::chainParamId (int i)        { return kChain[i].id; }
const char* CombinedProcessor::chainParamShortName (int i) { return kChain[i].shortName; }
const char* CombinedProcessor::busParamId (int i)          { return kBus[i].id; }
const char* CombinedProcessor::busParamShortName (int i)   { return kBus[i].shortName; }

juce::AudioProcessorValueTreeState::ParameterLayout CombinedProcessor::createLayout()
{
    using P   = juce::AudioParameterFloat;
    using ID  = juce::ParameterID;
    using Att = juce::AudioParameterFloatAttributes;
    using R   = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;

    const stonoga::Params d = combined::kPresets[0].pedal;
    for (const auto& def : kPedal)
        v.push_back (std::make_unique<P> (ID { def.id, 1 }, pl (def.name),
                                          R (def.lo, def.hi, def.hi > 1.f ? 0.1f : 0.f), d.*(def.mem)));

    const auto& c   = combined::kPresets[0].chain;
    const auto db   = Att().withLabel ("dB");
    const auto pct  = Att().withStringFromValueFunction ([] (float x, int) { return juce::String (juce::roundToInt (x * 100.0f)) + " %"; });
    const auto blnd = Att().withStringFromValueFunction ([] (float x, int)
    {
        const int b = juce::roundToInt (x * 100.0f);
        return "A " + juce::String (100 - b) + " / B " + juce::String (b);
    });

    v.push_back (std::make_unique<juce::AudioParameterBool> (ID { "pedalOn", 1 }, "Kostka", c.pedalOn));
    v.push_back (std::make_unique<P> (ID { "ampInput", 1 },   pl ("Wzmacniacz: wejście"), R (-12.f, 24.f, 0.1f), c.ampInput, db));
    v.push_back (std::make_unique<P> (ID { "ampDrive", 1 },   pl ("Wzmacniacz: drive"),   R (0.f, 1.f), c.ampDrive, pct));
    v.push_back (std::make_unique<P> (ID { "cabBlend", 1 },   pl ("Kolumna A/B"),         R (0.f, 1.f), c.cabBlend, blnd));
    v.push_back (std::make_unique<P> (ID { "roomMix", 1 },    pl ("Pokój"),               R (0.f, 0.5f), c.roomMix, pct));
    v.push_back (std::make_unique<P> (ID { "roomSize", 1 },   pl ("Rozmiar pokoju"),      R (0.f, 1.f), c.roomSize, pct));
    v.push_back (std::make_unique<P> (ID { "console", 1 },    pl ("Konsoleta"),           R (0.f, 1.f), c.console, pct));
    v.push_back (std::make_unique<P> (ID { "tape", 1 },       pl ("Taśma"),               R (0.f, 1.f), c.tape, pct));
    v.push_back (std::make_unique<P> (ID { "distComp", 1 },   pl ("Przester: kompresja"), R (0.f, 1.f), c.distComp, pct));
    v.push_back (std::make_unique<P> (ID { "distWidth", 1 },  pl ("Przester: panorama"),  R (0.f, 1.f), c.distWidth, pct));
    v.push_back (std::make_unique<P> (ID { "distSmear", 1 },  pl ("Przester: pogłos"),    R (0.f, 1.f), c.distSmear, pct));
    v.push_back (std::make_unique<P> (ID { "busComp", 1 },    pl ("Całość: kompresja"),   R (0.f, 1.f), c.busComp, pct));
    v.push_back (std::make_unique<P> (ID { "width", 1 },      pl ("Całość: panorama"),    R (0.f, 1.f), c.width, pct));
    v.push_back (std::make_unique<P> (ID { "smear", 1 },      pl ("Całość: pogłos"),      R (0.f, 1.f), c.smear, pct));
    v.push_back (std::make_unique<P> (ID { "outputGain", 1 }, pl ("Wyjście"),             R (-24.f, 12.f, 0.1f), c.outputGain, db));
    v.push_back (std::make_unique<P> (ID { "protect", 1 },    pl ("Ochrona uszu"),        R (0.f, 1.f), c.protect, pct));

    return { v.begin(), v.end() };
}

CombinedProcessor::CombinedProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STONOGA_ALBINI", createLayout())
{
    for (int i = 0; i < kNumPedalParams; ++i)
        pedalParams[(size_t) i] = apvts.getRawParameterValue (kPedal[i].id);

    pPedalOn    = apvts.getRawParameterValue ("pedalOn");
    pAmpInput   = apvts.getRawParameterValue ("ampInput");
    pAmpDrive   = apvts.getRawParameterValue ("ampDrive");
    pCabBlend   = apvts.getRawParameterValue ("cabBlend");
    pRoomMix    = apvts.getRawParameterValue ("roomMix");
    pRoomSize   = apvts.getRawParameterValue ("roomSize");
    pConsole    = apvts.getRawParameterValue ("console");
    pTape       = apvts.getRawParameterValue ("tape");
    pOutputGain = apvts.getRawParameterValue ("outputGain");
    pProtect    = apvts.getRawParameterValue ("protect");
    pSmear      = apvts.getRawParameterValue ("smear");
    pWidth      = apvts.getRawParameterValue ("width");
    pChoir      = apvts.getRawParameterValue ("choir");
    pDistComp   = apvts.getRawParameterValue ("distComp");
    pDistWidth  = apvts.getRawParameterValue ("distWidth");
    pDistSmear  = apvts.getRawParameterValue ("distSmear");
    pBusComp    = apvts.getRawParameterValue ("busComp");

    pedal.builtInProtection = false;
}

bool CombinedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void CombinedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    const int nch = juce::jmax (1, getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) nch };

    pedalOS = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) nch, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    pedalOS->initProcessing ((size_t) maxBlockSize);
    pedal.prepare (sampleRate * 4.0, nch);
    osDry.setSize (nch, maxBlockSize * 4, false, true, true);
    pedalMix.reset (sampleRate * 4.0, 0.02);
    pedalMix.setCurrentAndTargetValue (pPedalOn->load() > 0.5f ? 1.0f : 0.0f);

    amp.prepare (spec);
    cab.prepare (spec);
    room.prepare (spec);
    console.prepare (spec);
    tape.prepare (spec);
    choir.prepare (sampleRate);
    distComp.prepare (sampleRate, Compressor::Mode::Sustain);
    busComp.prepare (sampleRate, Compressor::Mode::Glue);
    distSpace.prepare (sampleRate, 2.0f);   // krótszy ogon: przester ma zostać czytelny
    space.prepare (sampleRate, 3.2f);
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.05);
    guard.prepare (sampleRate, nch);

    setLatencySamples (juce::roundToInt (pedalOS->getLatencyInSamples()
                                         + amp.getLatencyInSamples()
                                         + console.getLatencyInSamples()
                                         + (float) tape.getLatencyInSamples()));
}

void CombinedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else
        for (int c = numIn; c < numOut; ++c)
            buffer.clear (c, 0, n);

    if (pedalOS == nullptr || n == 0)
        return;

    amp.setGain    (pAmpInput->load());
    amp.setDrive   (pAmpDrive->load());
    cab.setBlend   (pCabBlend->load());
    room.setMix    (pRoomMix->load());
    room.setSize   (pRoomSize->load());
    console.setAmount (pConsole->load());
    tape.setAmount    (pTape->load());
    outputGain.setGainDecibels (pOutputGain->load());
    choir.setAmount (pChoir->load());
    space.setSmear (pSmear->load());
    space.setWidth (pWidth->load());
    distComp.setAmount (pDistComp->load());
    busComp.setAmount (pBusComp->load());
    distSpace.setSmear (pDistSmear->load());
    distSpace.setWidth (pDistWidth->load());
    pedalMix.setTargetValue (pPedalOn->load() > 0.5f ? 1.0f : 0.0f);

    juce::dsp::AudioBlock<float> full (buffer);
    full = full.getSubsetChannelBlock (0, (size_t) juce::jmax (1, numOut));

    const auto total = full.getNumSamples();
    for (size_t pos = 0; pos < total; pos += (size_t) maxBlockSize)
    {
        auto chunk = full.getSubBlock (pos, juce::jmin ((size_t) maxBlockSize, total - pos));
        processChunk (chunk);
    }
}

void CombinedProcessor::processChunk (juce::dsp::AudioBlock<float>& block)
{
    // --- kostka Stonoga (4x). Silnik pracuje zawsze, więc po włączeniu
    //     footswitcha ogony pogłosów i sprzężenia są "żywe", jak w trybie trails.
    stonoga::Params p;
    for (int i = 0; i < kNumPedalParams; ++i)
        p.*(kPedal[i].mem) = pedalParams[(size_t) i]->load();
    p.choir = 0.0f; // stary chór Stonogi (pętla z przesuwaniem wysokości) zastąpiony przez ChoirStage

    auto up = pedalOS->processSamplesUp (block);
    const auto nUp  = up.getNumSamples();
    const auto nCh  = juce::jmin (up.getNumChannels(), (size_t) osDry.getNumChannels());

    for (size_t c = 0; c < nCh; ++c)
        juce::FloatVectorOperations::copy (osDry.getWritePointer ((int) c), up.getChannelPointer (c), (int) nUp);

    std::array<float*, 8> ptrs {};
    const int engCh = (int) juce::jmin (nCh, ptrs.size());
    for (int c = 0; c < engCh; ++c)
        ptrs[(size_t) c] = up.getChannelPointer ((size_t) c);
    pedal.process (ptrs.data(), engCh, (int) nUp, p);

    if (pedalMix.isSmoothing() || pedalMix.getTargetValue() < 1.0f)
    {
        for (size_t i = 0; i < nUp; ++i)
        {
            const float m = pedalMix.getNextValue();
            for (size_t c = 0; c < nCh; ++c)
            {
                auto* w = up.getChannelPointer (c);
                const float dry = osDry.getSample ((int) c, (int) i);
                w[i] = dry + m * (w[i] - dry);
            }
        }
    }

    pedalOS->processSamplesDown (block);

    // --- łańcuch nagraniowy ----------------------------------------------
    amp.process (block);
    cab.process (block);

    // na przesterze
    distComp.process (block);
    distSpace.process (block);

    choir.process (block);
    room.process (block);
    console.process (block);
    tape.process (block);

    // na całości
    busComp.process (block);
    space.process (block);

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    outputGain.process (ctx);

    guard.process (block, pProtect->load());
}

// ---------------------------------------------------------------------------
// Presety
// ---------------------------------------------------------------------------
int CombinedProcessor::getNumPrograms() { return combined::kNumPresets; }

const juce::String CombinedProcessor::getProgramName (int index)
{
    if (index < 0 || index >= combined::kNumPresets)
        return {};
    return pl (combined::kPresets[index].name);
}

void CombinedProcessor::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void CombinedProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= combined::kNumPresets)
        return;
    currentProgram = index;

    const auto& preset = combined::kPresets[index];
    const auto& sp = preset.pedal;
    for (int i = 0; i < kNumPedalParams; ++i)
        setParam (kPedal[i].id, sp.*(kPedal[i].mem));

    const auto& c = preset.chain;
    setParam ("pedalOn",    c.pedalOn ? 1.0f : 0.0f);
    setParam ("ampInput",   c.ampInput);
    setParam ("ampDrive",   c.ampDrive);
    setParam ("cabBlend",   c.cabBlend);
    setParam ("roomMix",    c.roomMix);
    setParam ("roomSize",   c.roomSize);
    setParam ("console",    c.console);
    setParam ("tape",       c.tape);
    setParam ("distComp",   c.distComp);
    setParam ("distWidth",  c.distWidth);
    setParam ("distSmear",  c.distSmear);
    setParam ("busComp",    c.busComp);
    setParam ("width",      c.width);
    setParam ("smear",      c.smear);
    setParam ("outputGain", c.outputGain);
    setParam ("protect",    c.protect);
}

// ---------------------------------------------------------------------------
// IR kolumny
// ---------------------------------------------------------------------------
juce::Identifier CombinedProcessor::irProperty (int slot)
{
    static const juce::Identifier a ("irPathA"), b ("irPathB");
    return slot == 0 ? a : b;
}

bool CombinedProcessor::loadCabIR (int slot, const juce::File& file)
{
    if (! cab.loadIR (slot, file))
        return false;
    apvts.state.setProperty (irProperty (slot), file.getFullPathName(), nullptr);
    return true;
}

void CombinedProcessor::clearCabIR (int slot)
{
    cab.clearIR (slot);
    apvts.state.setProperty (irProperty (slot), juce::String(), nullptr);
}

juce::String CombinedProcessor::getCabIRDisplayName (int slot) const
{
    const auto path = apvts.state.getProperty (irProperty (slot)).toString();
    if (path.isEmpty())
        return pl ("(brak – wbudowana kolumna)");
    const juce::File f (path);
    return cab.hasIR (slot) ? f.getFileName() : pl ("brak pliku: ") + f.getFileName();
}

void CombinedProcessor::restoreCabIRsFromState()
{
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto path = apvts.state.getProperty (irProperty (slot)).toString();
        if (path.isNotEmpty() && juce::File (path).existsAsFile())
            cab.loadIR (slot, juce::File (path));
        else
            cab.clearIR (slot);
    }
}

// ---------------------------------------------------------------------------
juce::AudioProcessorEditor* CombinedProcessor::createEditor() { return new CombinedEditor (*this); }

void CombinedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CombinedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            restoreCabIRsFromState();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CombinedProcessor(); }
