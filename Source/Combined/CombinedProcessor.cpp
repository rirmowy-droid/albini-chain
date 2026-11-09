#include "CombinedProcessor.h"
#include "CombinedEditor.h"
#include "Presets2.h"
#include "BundledIRs.h"
#if ALBINI_HAS_BUNDLED_IR
 #include "BinaryData.h"
#endif

namespace
{
    juce::String pl (const char* s) { return juce::String::fromUTF8 (s); }

    juce::StringArray names (const char* const* list, int n)
    {
        juce::StringArray a;
        for (int i = 0; i < n; ++i) a.add (pl (list[i]));
        return a;
    }

    constexpr double listenSeconds    = 6.0;   // ile sekund grania potrzeba do pomiaru
    constexpr double listenTimeout    = 14.0;  // potem i tak kończymy
    constexpr double waitTracksTimeout = 20.0;
}

juce::AudioProcessorValueTreeState::ParameterLayout CombinedProcessor::createLayout()
{
    using ID  = juce::ParameterID;
    using Att = juce::AudioParameterFloatAttributes;
    using R   = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    const auto& d = sxa::kPresets[0];

    for (const auto& def : sxa::kPedal)
        v.push_back (std::make_unique<juce::AudioParameterFloat> (ID { def.id, 1 }, pl (def.name),
                     R (def.lo, def.hi, def.hi > 1.f ? 0.1f : 0.f), d.pedal.*(def.mem)));

    const auto pct  = Att().withStringFromValueFunction ([] (float x, int) { return juce::String (juce::roundToInt (x * 100.f)) + " %"; });
    const auto db   = Att().withStringFromValueFunction ([] (float x, int) { return juce::String (x, 1) + " dB"; });
    const auto hz   = Att().withStringFromValueFunction ([] (float x, int) { return x < 25.f ? juce::String::fromUTF8 ("wył.") : juce::String (juce::roundToInt (x)) + " Hz"; });
    const auto panA = Att().withStringFromValueFunction ([] (float x, int)
    {
        const int p = juce::roundToInt (x * 100.f);
        return p == 0 ? juce::String ("C") : (p < 0 ? "L " + juce::String (-p) : "R " + juce::String (p));
    });
    const auto blnd = Att().withStringFromValueFunction ([] (float x, int)
    {
        const int b = juce::roundToInt (x * 100.f);
        return "A " + juce::String (100 - b) + " / B " + juce::String (b);
    });
    const auto rate = Att().withStringFromValueFunction ([] (float x, int) { return juce::String (0.08f + x * x * 2.9f, 2) + " Hz"; });
    const auto msA  = Att().withStringFromValueFunction ([] (float x, int) { return x < 10.f ? juce::String (x, 1) + " ms" : juce::String (juce::roundToInt (x)) + " ms"; });
    const auto ratA = Att().withStringFromValueFunction ([] (float x, int) { return juce::String (x, x < 10.f ? 1 : 0) + ":1"; });

    for (const auto& def : sxa::kParams)
    {
        const float dv = d.s.*(def.mem);
        const ID id { def.id, 1 };
        switch (def.kind)
        {
            case sxa::Kind::Choice:
            {
                juce::StringArray ch;
                if (juce::String (def.id) == "source") ch = names (sxa::kSourceNames, sxa::NumSources);
                else if (juce::String (def.id) == "mic") ch = names (sxa::kMicNames, sxa::NumMics);
                else if (juce::String (def.id) == "compMode" || juce::String (def.id) == "busMode") ch = names (sxa::kCompModeNames, 2);
                else if (juce::String (def.id) == "compChar" || juce::String (def.id) == "busCompChar") ch = names (sxa::kCompCharNames, sxa::kNumCompChars);
                else if (juce::String (def.id) == "consoleModel") ch = names (sxa::kConsoleNames, sxa::kNumConsoles);
                else if (juce::String (def.id) == "ampModel") ch = names (sxa::kAmpNames, sxa::kNumAmps);
                else if (juce::String (def.id) == "drvModel") ch = names (sxa::kDriveNames, sxa::kNumDrives);
                else if (juce::String (def.id) == "modModel") ch = names (sxa::kModNames, sxa::kNumMods);
                else if (juce::String (def.id) == "dlyModel") ch = names (sxa::kDelayNames, sxa::kNumDelays);
                else if (juce::String (def.id) == "cabMic") ch = names (sxa::kCabMicNames, sxa::kNumCabMics);
                else if (juce::String (def.id) == "tapeModel") ch = names (sxa::kTapeNames, sxa::kNumTapes);
                else if (juce::String (def.id) == "tapeSpeed") ch = names (sxa::kTapeSpeedNames, 2);
                else if (juce::String (def.id) == "cabA" || juce::String (def.id) == "cabB")
                {
                    ch.add (pl ("Plik / wbudowana krzywa"));
                    for (const auto& ir : sxa::kBundledIRs) ch.add (pl (ir.name));
                }
                else ch = names (sxa::kStyleNames, sxa::kNumStyles);
                v.push_back (std::make_unique<juce::AudioParameterChoice> (id, pl (def.name), ch, (int) dv));
                break;
            }
            case sxa::Kind::Toggle:
                v.push_back (std::make_unique<juce::AudioParameterBool> (id, pl (def.name), dv > 0.5f));
                break;
            default:
            {
                const auto& att = def.kind == sxa::Kind::Decibel ? db
                                : def.kind == sxa::Kind::Hertz   ? hz
                                : def.kind == sxa::Kind::Pan     ? panA
                                : def.kind == sxa::Kind::Blend   ? blnd
                                : def.kind == sxa::Kind::Rate    ? rate
                                : def.kind == sxa::Kind::Millis  ? msA
                                : def.kind == sxa::Kind::Ratio   ? ratA : pct;
                R range (def.lo, def.hi, def.kind == sxa::Kind::Decibel ? 0.1f : 0.f);
                if (def.kind == sxa::Kind::Hertz) range.setSkewForCentre (90.f);
                if (def.kind == sxa::Kind::Ratio) range.setSkewForCentre (4.f);
                if (def.kind == sxa::Kind::Millis && def.lo > 0.f) range.setSkewForCentre (std::sqrt (def.lo * def.hi));
                v.push_back (std::make_unique<juce::AudioParameterFloat> (id, pl (def.name), range, dv, att));
                break;
            }
        }
    }
    return { v.begin(), v.end() };
}

CombinedProcessor::CombinedProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STONOGA_ALBINI_2", createLayout())
{
    for (int i = 0; i < sxa::kNumParams; ++i) params[(size_t) i] = apvts.getRawParameterValue (sxa::kParams[i].id);
    for (int i = 0; i < sxa::kNumPedal; ++i)  pedalParams[(size_t) i] = apvts.getRawParameterValue (sxa::kPedal[i].id);

    registrySlot = sxa::MixRegistry::get().add();
    seenGeneration = sxa::MixRegistry::get().currentGeneration();
    for (int f = 0; f < sxa::MixRegistry::NumFields; ++f) seenRemote[(size_t) f] = sxa::MixRegistry::get().remoteSeq (juce::jmax (0, registrySlot), f);
    seenUndo = sxa::MixRegistry::get().currentUndoGeneration();
    startTimerHz (10);
}

CombinedProcessor::~CombinedProcessor()
{
    stopTimer();
    auto& reg = sxa::MixRegistry::get();
    if (amState == AM::Listening) reg.pendingTracks.fetch_sub (1);
    if (amState == AM::Listening || amState == AM::WaitingForPeers) reg.pendingEq.fetch_sub (1);
    reg.clearSpectrum (registrySlot);
    reg.publishEnvelopes (registrySlot, 0.f, 0.f);
    reg.remove (registrySlot);
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
    preparedRate = sampleRate;
    engine.prepare (sampleRate, maxBlockSize, juce::jmax (1, getTotalNumOutputChannels()), paramValue ("hq") > 0.5f);
    setLatencySamples (juce::roundToInt (engine.getLatencyInSamples()));
}

void CombinedProcessor::readSettings (sxa::Settings& s, stonoga::Params& p) const
{
    for (int i = 0; i < sxa::kNumParams; ++i) s.*(sxa::kParams[i].mem) = params[(size_t) i]->load();
    p = sxa::cleanPedal();
    for (int i = 0; i < sxa::kNumPedal; ++i)  p.*(sxa::kPedal[i].mem) = pedalParams[(size_t) i]->load();
}

void CombinedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);
    if (n == 0) return;

    sxa::Settings s; stonoga::Params p;
    readSettings (s, p);

    // ustępowanie: obwiednie stopy i głosu od innych kopii wtyczki
    auto& reg = sxa::MixRegistry::get();
    float kick = 0.f, vocal = 0.f;
    reg.readEnvelopes (registrySlot, kick, vocal);
    engine.setSidechain (kick, vocal);
    // solo innej ścieżki wycisza tę (sumy nigdy)
    s.soloMuted = (s.source != sxa::Mix && s.mixSolo < 0.5f && reg.anySolo()) ? 1.f : 0.f;

    juce::dsp::AudioBlock<float> full (buffer);
    full = full.getSubsetChannelBlock (0, (size_t) juce::jmax (1, numOut));
    for (size_t pos = 0; pos < full.getNumSamples(); pos += (size_t) maxBlockSize)
    {
        auto chunk = full.getSubBlock (pos, juce::jmin ((size_t) maxBlockSize, full.getNumSamples() - pos));
        engine.process (chunk, s, p);
    }
    reg.publishEnvelopes (registrySlot, engine.getPublishedKick(), engine.getPublishedVocal());
}

// ---------------------------------------------------------------------------
// Parametry, presety
// ---------------------------------------------------------------------------
void CombinedProcessor::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

float CombinedProcessor::paramValue (const char* id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.f;
}

int CombinedProcessor::getNumPrograms() { return sxa::kNumPresets; }

const juce::String CombinedProcessor::getProgramName (int index)
{
    if (index < 0 || index >= sxa::kNumPresets) return {};
    return pl (sxa::kPresets[index].name);
}

void CombinedProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= sxa::kNumPresets) return;
    currentProgram = index;
    userPresetName.clear();
    const auto& pr = sxa::kPresets[index];
    for (const auto& def : sxa::kPedal) setParam (def.id, pr.pedal.*(def.mem));
    const float keepStyle = paramValue ("style");
    for (const auto& def : sxa::kParams) setParam (def.id, pr.s.*(def.mem));
    setParam ("style", keepStyle);   // styl auto-miksu to ustawienie sesji, nie presetu
}

// ---------------------------------------------------------------------------
// Auto-miks
// ---------------------------------------------------------------------------
int CombinedProcessor::eqParamIndex (int band)
{
    static const auto table = []
    {
        std::array<int, sxa::kNumEqBands> t {};
        for (int b = 0; b < sxa::kNumEqBands; ++b)
            for (int i = 0; i < sxa::kNumParams; ++i)
                if (sxa::kParams[i].mem == sxa::kEqMembers[b]) t[(size_t) b] = i;
        return t;
    }();
    return table[(size_t) band];
}

sxa::MixContext CombinedProcessor::mixContext() const
{
    auto& reg = sxa::MixRegistry::get();
    sxa::MixContext c;
    int idx = 0;
    reg.count (registrySlot, sxa::Guitar, c.guitars, idx);
    c.guitarIndex = idx;
    c.basses = reg.countSource (sxa::Bass);
    c.vocals = reg.countSource (sxa::Vocals);
    c.drums  = reg.countSource (sxa::Drums);
    return c;
}

void CombinedProcessor::requestAutoMix()
{
    sxa::MixRegistry::get().request ((int) std::round (paramValue ("style")));
}

void CombinedProcessor::timerCallback()
{
    auto& reg = sxa::MixRegistry::get();
    const int source = getSource();
    reg.setSource (registrySlot, source);

    // mikser na sumie: przejmij zmiany zrobione z innej kopii, potem podaj swoje ustawienia i poziom
    if (registrySlot >= 0)
    {
        for (int f = 0; f < sxa::MixRegistry::NumFields; ++f)
        {
            const int seq = reg.remoteSeq (registrySlot, f);
            if (seq != seenRemote[(size_t) f])
            {
                seenRemote[(size_t) f] = seq;
                setParam (sxa::MixRegistry::fieldParam (f), reg.remoteValue (registrySlot, f));
            }
            reg.publishChannel (registrySlot, f, paramValue (sxa::MixRegistry::fieldParam (f)));
        }
        meterHold = juce::jmax (engine.takeOutputPeakDb(), meterHold - 2.5f);
        reg.publishMeter (registrySlot, meterHold);
    }

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const int gen = reg.currentGeneration();

    // tryb wysokiej jakości (8x): inna latencja, więc przygotuj tor od nowa
    const bool hqWanted = paramValue ("hq") > 0.5f;
    if (preparedRate > 0.0 && hqWanted != engine.isHighQuality())
    {
        suspendProcessing (true);
        engine.prepare (preparedRate, maxBlockSize, juce::jmax (1, getTotalNumOutputChannels()), hqWanted);
        loadedCab = { -1, -1 };
        suspendProcessing (false);
        setLatencySamples (juce::roundToInt (engine.getLatencyInSamples()));
    }
    engine.cabinet().setAlign (paramValue ("cabAlign") > 0.5f);

    // wbudowane IR kolumn: wczytaj, gdy wybór się zmienił (wątek wiadomości)
    for (int slot = 0; slot < 2; ++slot)
    {
        const int idx = (int) std::round (paramValue (slot == 0 ? "cabA" : "cabB"));
        if (idx == loadedCab[(size_t) slot]) continue;
        loadedCab[(size_t) slot] = idx;
        if (idx > 0) loadBundledIR (slot, idx - 1);
        else
        {
            const auto path = apvts.state.getProperty (irProperty (slot)).toString();
            if (path.isNotEmpty() && juce::File (path).existsAsFile()) engine.cabinet().loadIR (slot, juce::File (path));
            else engine.cabinet().clearIR (slot);
        }
    }

    // cofnij auto-miks (we wszystkich kopiach naraz)
    if (reg.currentUndoGeneration() != seenUndo)
    {
        seenUndo = reg.currentUndoGeneration();
        if (hasSnapshot)
        {
            if (amState == AM::Listening) reg.pendingTracks.fetch_sub (1);
            if (amState == AM::Listening || amState == AM::WaitingForPeers) reg.pendingEq.fetch_sub (1);
            engine.stopMeasuring();
            amState = AM::Idle;
            apvts.replaceState (snapshot.createCopy());
            restoreCabIRsFromState();
            restoreAlbumFromState();
            loadedCab = { -1, -1 };
            hasSnapshot = false;
            autoStatus = pl ("Auto-miks cofnięty – ustawienia jak przed nim");
        }
    }

    if (gen != seenGeneration)
    {
        seenGeneration = gen;
        if (amState == AM::Idle)   // zapamiętaj stan sprzed auto-miksu (do cofnięcia)
        {
            snapshot = apvts.copyState();
            hasSnapshot = true;
        }
        amStyle = juce::jlimit (0, sxa::kNumStyles - 1, reg.currentStyle());
        setParam ("style", (float) amStyle);
        amStart = now;
        if (source == sxa::Mix)
        {
            amState = AM::WaitingForTracks;
            autoStatus = pl ("Auto-miks: czekam na ścieżki…");
        }
        else
        {
            if (amState != AM::Listening) reg.pendingTracks.fetch_add (1);
            if (amState != AM::Listening && amState != AM::WaitingForPeers) reg.pendingEq.fetch_add (1);
            reg.clearSpectrum (registrySlot);
            amState = AM::Listening;
            // najpierw styl (kompresja, sala, taśma zmieniają głośność), potem pomiar
            sxa::Settings s; stonoga::Params p;
            readSettings (s, p);
            sxa::applyTemplate (s, amStyle, mixContext());
            for (const auto& def : sxa::kParams)
                if (juce::String (def.id) != "source" && juce::String (def.id) != "style"
                    && juce::String (def.id) != "autoGain" && juce::String (def.id) != "outputGain")
                    setParam (def.id, s.*(def.mem));
            engine.startMeasuring();
            autoStatus = pl ("Auto-miks: słucham… zagraj fragment utworu");
        }
    }

    const double elapsed = now - amStart;
    const auto ctx = mixContext();

    // ciągłe dopasowanie barwy sumy: co ~2 s dociągnij korektor do krzywej płyty
    if (source == sxa::Mix && amState == AM::Idle && paramValue ("albumTrack") > 0.5f && ++trackTicks >= 20)
    {
        trackTicks = 0;
        const int style = (int) std::round (paramValue ("style"));
        if (sxa::kAlbumCurves[style].has && engine.getTrackedSeconds() > 4.f)
        {
            float bands[AlbumEq::numBands], diff[AlbumEq::numBands], gains[AlbumEq::numBands];
            engine.getTrackedAlbumBandsDb (bands);
            sxa::computeAlbumTarget (style, bands, diff);
            for (int b = 0; b < AlbumEq::numBands; ++b)
                albumTarget[(size_t) b] = juce::jlimit (-8.f, AlbumEq::bandHz (b) > 9000.f ? 2.f : 8.f, albumTarget[(size_t) b] + 0.25f * diff[b]);
            AlbumEq::fit ((float) getSampleRate(), albumTarget.data(), gains);
            setAlbumGains (gains);
            if (autoStatus.isEmpty() || autoStatus.startsWith (pl ("Auto-miks: gotowe")) || autoStatus.startsWith (pl ("Ciągłe")))
                autoStatus = pl ("Ciągłe dopasowanie barwy: ") + pl (sxa::kAlbumCurves[style].name);
        }
    }

    switch (amState)
    {
        case AM::Idle: break;

        case AM::Listening:
        {
            if (elapsed < 0.6) { engine.startMeasuring(); break; }   // niech nowe ustawienia się ułożą
            const double heard = engine.getMeasuredSeconds();
            autoStatus = pl ("Auto-miks: słucham… ") + juce::String (juce::jmin (heard, listenSeconds), 1) + " / " + juce::String (listenSeconds, 0) + " s";
            if (heard >= listenSeconds || elapsed >= listenTimeout)
            {
                engine.stopMeasuring();
                juce::String gainText;
                float gain = paramValue ("autoGain");
                if (heard > 1.0)
                {
                    gain = juce::jlimit (-24.f, 24.f, sxa::targetDb (amStyle, source, ctx) - engine.getMeasuredDb() - paramValue ("outputGain"));
                    setParam ("autoGain", gain);
                    gainText = ", " + juce::String (gain, 1) + " dB";

                    // widmo po ustawieniu głośności -> dla korekty barwy wszystkich ścieżek
                    float bands[sxa::kNumEqBands];
                    engine.getMeasuredBandsDb (bands);
                    for (auto& b : bands) b += gain + paramValue ("outputGain");
                    std::copy (bands, bands + sxa::kNumEqBands, myBands.begin());
                    reg.publishSpectrum (registrySlot, bands);
                }
                else gainText = pl (", cisza – głośność bez zmian");

                reg.pendingTracks.fetch_sub (1);
                amState = AM::WaitingForPeers;
                amPeersStart = now;
                lastGainText = gainText;
                autoStatus = pl ("Auto-miks: czekam na inne ścieżki (korekta barwy)…");
            }
            break;
        }

        case AM::WaitingForPeers:
            if (reg.pendingTracks.load() <= 0 || now - amPeersStart > waitTracksTimeout)
            {
                juce::String eqText = pl (", bez korekty barwy");
                if (engine.getMeasuredSeconds() > 1.0)
                {
                    float out[sxa::kNumEqBands];
                    sxa::computeAutoEq (source, myBands.data(), reg.otherSpectra (registrySlot), out);
                    int changed = 0;
                    for (int b = 0; b < sxa::kNumEqBands; ++b)
                    {
                        setParam (sxa::kParams[eqParamIndex (b)].id, out[b]);
                        if (std::abs (out[b]) > 0.05f) ++changed;
                    }
                    eqText = ", korekta barwy: " + juce::String (changed) + pl (" pasm");
                }
                reg.pendingEq.fetch_sub (1);
                amState = AM::Idle;
                autoStatus = pl ("Auto-miks: gotowe (") + pl (sxa::kSourceNames[source]) + lastGainText + eqText + ")";
            }
            break;

        case AM::WaitingForTracks:
            if ((elapsed > 1.0 && reg.pendingEq.load() <= 0) || elapsed > waitTracksTimeout)
            {
                sxa::Settings s; stonoga::Params p;
                readSettings (s, p);
                sxa::applyTemplate (s, amStyle, ctx);
                for (const char* id : { "busComp", "width", "smear", "console", "tape", "tilt", "roomMix", "lowCut",
                                        "consoleModel", "busCompChar", "eq0", "eq1", "eq2", "eq3", "eq4", "eq5", "eq6", "eq7",
                                        "deHarsh", "busMode", "busScHpf", "busMix" })
                    for (const auto& def : sxa::kParams)
                        if (juce::String (def.id) == id) setParam (id, s.*(def.mem));
                setParam ("autoGain", 0.f);
                setAlbumGains (nullptr);                 // mierzymy bez starej barwy płyty
                engine.startMeasuring();
                amState = AM::ListeningMix;
                amStart = now;
                autoStatus = pl ("Auto-miks: ścieżki gotowe, słucham całości…");
            }
            break;

        case AM::ListeningMix:
        case AM::ListeningMix2:
        {
            const bool second = amState == AM::ListeningMix2;
            const double heard = engine.getMeasuredSeconds();
            if (elapsed < 0.6) { engine.startMeasuring(); break; }   // niech zmiany się ułożą
            autoStatus = (second ? pl ("Auto-miks: sprawdzam wynik na sumie… ") : pl ("Auto-miks: słucham całości… "))
                       + juce::String (juce::jmin (heard, listenSeconds), 1) + " s";
            if (heard < listenSeconds && elapsed < listenTimeout)
                break;

            engine.stopMeasuring();
            const auto& curve = sxa::kAlbumCurves[amStyle];
            juce::String matchText;
            if (heard > 1.0)
            {
                if (! second)
                {
                    const float gain = juce::jlimit (-24.f, 24.f, sxa::targetDb (amStyle, sxa::Mix, ctx) - engine.getMeasuredDb() - paramValue ("outputGain"));
                    setParam ("autoGain", gain);
                }
                else
                {
                    // za limiterem: dociągnij głośność (limiter zjada część podgłośnienia)
                    const float miss = sxa::targetDb (amStyle, sxa::Mix, ctx) - engine.getMeasuredOutputDb();
                    setParam ("autoGain", juce::jlimit (-24.f, 24.f, paramValue ("autoGain") + juce::jlimit (-6.f, 6.f, miss)));
                }

                // barwa: 26 pasm co 1/3 oktawy; w drugim przebiegu dociągamy resztę różnicy
                float bands[AlbumEq::numBands], diff[AlbumEq::numBands], gains[AlbumEq::numBands];
                engine.getMeasuredAlbumBandsDb (bands);
                if (sxa::computeAlbumTarget (amStyle, bands, diff))
                {
                    for (int b = 0; b < AlbumEq::numBands; ++b)
                        albumTarget[(size_t) b] = juce::jlimit (-8.f, AlbumEq::bandHz (b) > 9000.f ? 2.f : 8.f, (second ? albumTarget[(size_t) b] : 0.f) + diff[b]);
                    AlbumEq::fit ((float) getSampleRate(), albumTarget.data(), gains);
                    setAlbumGains (gains);
                    setParam ("width", sxa::computeAlbumWidth (amStyle, engine.getMeasuredSideToMidDb(), paramValue ("width")));
                    matchText = pl (", barwa, szerokość i głośność jak ") + pl (curve.name);
                }
            }

            if (! second && curve.has && heard > 1.0)
            {
                amState = AM::ListeningMix2;
                amStart = now;
                engine.startMeasuring();
                break;
            }
            const int tracks = ctx.guitars + ctx.basses + ctx.vocals + reg.countSource (sxa::Drums);
            amState = AM::Idle;
            autoStatus = pl ("Auto-miks: gotowe – ") + juce::String (tracks) + pl (" ścieżek, styl: ") + pl (sxa::kStyleNames[amStyle]) + matchText;
            break;
        }
    }
}

void CombinedProcessor::updateTrackProperties (const TrackProperties& props)
{
    trackName = props.name;
    sxa::MixRegistry::get().setName (registrySlot, props.name);
}

// ---------------------------------------------------------------------------
// Własne presety
// ---------------------------------------------------------------------------
juce::File CombinedProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Stonoga x Albini").getChildFile ("Presety");
}

juce::Array<juce::File> CombinedProcessor::getUserPresetFiles()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.sxapreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

bool CombinedProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    juce::XmlElement xml ("StonogaAlbiniPreset");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name", name.trim());
    auto addParam = [&] (const char* id)
    {
        auto* e = xml.createNewChildElement ("Param");
        e->setAttribute ("id", id);
        e->setAttribute ("value", (double) paramValue (id));
    };
    for (const auto& def : sxa::kPedal)  addParam (def.id);
    for (const auto& def : sxa::kParams) if (juce::String (def.id) != "style") addParam (def.id);
    for (int slot = 0; slot < 2; ++slot)
        xml.setAttribute (irProperty (slot), apvts.state.getProperty (irProperty (slot)).toString());

    const auto file = folder.getChildFile (clean + ".sxapreset");
    if (! xml.writeTo (file)) return false;
    userPresetName = file.getFileNameWithoutExtension();
    return true;
}

bool CombinedProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("StonogaAlbiniPreset")) return false;
    for (auto* e : xml->getChildWithTagNameIterator ("Param"))
    {
        const auto id = e->getStringAttribute ("id");
        if (id == "style") continue;
        if (apvts.getParameter (id) != nullptr)
            setParam (id.toRawUTF8(), (float) e->getDoubleAttribute ("value"));
    }
    for (int slot = 0; slot < 2; ++slot)
        apvts.state.setProperty (irProperty (slot), xml->getStringAttribute (irProperty (slot).toString()), nullptr);
    loadedCab = { -1, -1 };   // timer wczyta wybrane kolumny (wbudowane albo pliki)
    userPresetName = file.getFileNameWithoutExtension();
    return true;
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
    if (! engine.cabinet().loadIR (slot, file)) return false;
    apvts.state.setProperty (irProperty (slot), file.getFullPathName(), nullptr);
    loadedCab[(size_t) slot] = 0;                       // własny plik zamiast wbudowanej kolumny
    setParam (slot == 0 ? "cabA" : "cabB", 0.f);
    return true;
}

bool CombinedProcessor::loadBundledIR (int slot, int index)
{
   #if ALBINI_HAS_BUNDLED_IR
    if (index < 0 || index >= sxa::kNumBundledIRs) return false;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]) == sxa::kBundledIRs[index].file)
        {
            int size = 0;
            const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            return engine.cabinet().loadIRFromMemory (slot, data, (size_t) size, 0.1);
        }
   #else
    juce::ignoreUnused (slot, index);
   #endif
    return false;
}

void CombinedProcessor::clearCabIR (int slot)
{
    engine.cabinet().clearIR (slot);
    apvts.state.setProperty (irProperty (slot), juce::String(), nullptr);
    loadedCab[(size_t) slot] = 0;
    setParam (slot == 0 ? "cabA" : "cabB", 0.f);
}

juce::String CombinedProcessor::getCabIRDisplayName (int slot) const
{
    const int idx = (int) std::round (paramValue (slot == 0 ? "cabA" : "cabB"));
    if (idx > 0 && idx <= sxa::kNumBundledIRs) return pl (sxa::kBundledIRs[idx - 1].name);
    const auto path = apvts.state.getProperty (irProperty (slot)).toString();
    if (path.isEmpty()) return pl ("(brak – wbudowana kolumna)");
    const juce::File f (path);
    return const_cast<ChainEngine&> (engine).cabinet().hasIR (slot) ? f.getFileName() : pl ("brak pliku: ") + f.getFileName();
}

void CombinedProcessor::setAlbumGains (const float* gains)
{
    float g[AlbumEq::numBands] {};
    if (gains != nullptr) std::copy (gains, gains + AlbumEq::numBands, g);
    engine.albumEq().setGains (g);
    juce::StringArray parts;
    for (float v : g) parts.add (juce::String (v, 2));
    apvts.state.setProperty ("albumEq", parts.joinIntoString (","), nullptr);
    if (gains == nullptr) albumTarget.fill (0.f);
    juce::StringArray tp;
    for (float v : albumTarget) tp.add (juce::String (v, 2));
    apvts.state.setProperty ("albumTarget", tp.joinIntoString (","), nullptr);
}

void CombinedProcessor::restoreAlbumFromState()
{
    juce::StringArray parts;
    parts.addTokens (apvts.state.getProperty ("albumEq").toString(), ",", "");
    float g[AlbumEq::numBands] {};
    if (parts.size() == AlbumEq::numBands)
        for (int b = 0; b < AlbumEq::numBands; ++b) g[b] = parts[b].getFloatValue();
    engine.albumEq().setGains (g);
    juce::StringArray tp;
    tp.addTokens (apvts.state.getProperty ("albumTarget").toString(), ",", "");
    for (int b = 0; b < AlbumEq::numBands; ++b) albumTarget[(size_t) b] = tp.size() == AlbumEq::numBands ? tp[b].getFloatValue() : 0.f;
}

void CombinedProcessor::restoreCabIRsFromState()
{
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto path = apvts.state.getProperty (irProperty (slot)).toString();
        if (path.isNotEmpty() && juce::File (path).existsAsFile()) engine.cabinet().loadIR (slot, juce::File (path));
        else engine.cabinet().clearIR (slot);
    }
}

// ---------------------------------------------------------------------------
juce::AudioProcessorEditor* CombinedProcessor::createEditor() { return new CombinedEditor (*this); }

void CombinedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void CombinedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            restoreCabIRsFromState();
            restoreAlbumFromState();
            loadedCab = { -1, -1 };   // timer dociągnie wbudowane kolumny
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CombinedProcessor(); }
