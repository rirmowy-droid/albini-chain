#include <cstring>
#include "ChainEngine.h"

void ChainEngine::prepare (double sampleRate, int maxBlock, int numChannels, bool highQuality)
{
    sr = sampleRate;
    hqPrepared = highQuality;
    const int osLog2 = highQuality ? 3 : 2;
    osFactor = 1 << osLog2;
    maxBlockSize = juce::jmax (1, maxBlock);
    const int nch = juce::jmax (1, numChannels);
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) nch };

    pedalOS = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) nch, (size_t) osLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    pedalOS->initProcessing ((size_t) maxBlockSize);
    pedal.builtInProtection = false;
    pedal.prepare (sampleRate * osFactor, nch);
    osDry.setSize (nch, maxBlockSize * osFactor, false, true, true);
    diBuf.setSize (nch, maxBlockSize, false, true, true);
    pedalMix.reset (sampleRate * osFactor, 0.02);
    pedalMix.setCurrentAndTargetValue (1.f);
    for (auto& f : diLp) { f.lowpass ((float) sampleRate, 250.f, 0.7f); f.reset(); }

    amp.prepare (spec, osLog2);
    cab.prepare (spec);
    tone.prepare (sampleRate);
    srcComp.prepare (sampleRate, Compressor::Mode::Sustain);
    busComp.prepare (sampleRate, Compressor::Mode::Glue);
    chorus.prepare (sampleRate);
    srcSpace.prepare (sampleRate, SpaceSmear::Mode::DoubleTrack);
    busSpace.prepare (sampleRate, SpaceSmear::Mode::Widen);
    room.prepare (spec);
    console.prepare (spec);
    tape.prepare (spec);
    guard.prepare (sampleRate, nch);
    guard.ownLimiter = false;
    limiter.prepare (sampleRate, nch);
    transient.prepare (sampleRate);
    deEsser.prepare (sampleRate);
    doubleTake.prepare (sampleRate);
    midSide.prepare (sampleRate);
    voice.prepare (sampleRate);
    duck.prepare (sampleRate);
    slap.prepare (sampleRate);
    album.prepare (sampleRate);
    deHarsh.prepare (sampleRate);
    for (int b = 0; b < AlbumEq::numBands; ++b) { trackBp[b].bandpass ((float) sampleRate, AlbumEq::bandHz (b), AlbumEq::q); trackMs[(size_t) b] = 0.f; }
    trackSeconds = 0.f;
    for (int b = 0; b < AlbumEq::numBands; ++b) albumBp[b].bandpass ((float) sampleRate, AlbumEq::bandHz (b), AlbumEq::q);
    for (int b = 0; b < sxa::kNumEqBands; ++b) bandBp[b].bandpass ((float) sampleRate, sxa::kEqHz[b], 1.4f);

    faderGain.reset (sampleRate, 0.05);
    panL.reset (sampleRate, 0.05);
    panR.reset (sampleRate, 0.05);
    faderGain.setCurrentAndTargetValue (1.f);
    panL.setCurrentAndTargetValue (1.f);
    panR.setCurrentAndTargetValue (1.f);

    for (int c = 0; c < 2; ++c)
    {
        kHp[c].highpass ((float) sampleRate, 60.f, 0.7f);
        kShelf[c].highshelf ((float) sampleRate, 1500.f, 4.f);
        kHpOut[c].highpass ((float) sampleRate, 60.f, 0.7f);
        kShelfOut[c].highshelf ((float) sampleRate, 1500.f, 4.f);
    }
}

float ChainEngine::getLatencyInSamples() const
{
    return (pedalOS ? pedalOS->getLatencyInSamples() : 0.f)
         + amp.getLatencyInSamples()
         + console.getLatencyInSamples()
         + (float) tape.getLatencyInSamples()
         + (float) limiter.getLatencyInSamples();
}

void ChainEngine::getMeasuredBandsDb (float* out8) const
{
    for (int b = 0; b < sxa::kNumEqBands; ++b)
        out8[b] = 10.f * std::log10 (bandMs[(size_t) b].load() + 1e-12f);
}

void ChainEngine::getTrackedAlbumBandsDb (float* out) const
{
    for (int b = 0; b < AlbumEq::numBands; ++b) out[b] = 10.f * std::log10 (trackMs[(size_t) b].load() + 1e-15f);
}

void ChainEngine::getMeasuredAlbumBandsDb (float* out26) const
{
    for (int b = 0; b < AlbumEq::numBands; ++b) out26[b] = 10.f * std::log10 (albumMs[(size_t) b].load() + 1e-15f);
}

float ChainEngine::getMeasuredDb() const
{
    return 10.f * std::log10 (measuredMs.load() + 1e-12f);
}

void ChainEngine::process (juce::dsp::AudioBlock<float>& block, const sxa::Settings& s, const stonoga::Params& pedalIn)
{
    const int source = juce::jlimit (0, (int) sxa::NumSources - 1, (int) s.source);
    const bool pedalSource = sxa::hasPedal (source);
    const auto n   = block.getNumSamples();
    const auto nCh = juce::jmin (block.getNumChannels(), (size_t) osDry.getNumChannels());

    // nadajnik dla innych kopii: stopa z bębnów, obwiednia głosu z wokalu
    pubKick  = source == sxa::Drums  ? duck.analyseKick (block)  : 0.f;
    pubVocal = source == sxa::Vocals ? duck.analyseVocal (block) : 0.f;

    // prawdziwy dubel: prawy kanał = drugie podejście, zanim zagra przez kostkę i piec
    const bool trueDbl = s.trueDouble > 0.5f && source == sxa::Guitar && nCh >= 2;
    if (trueDbl) doubleTake.process (block, s.srcLoose);

    // gitara/bas nagrane w mono (L = P): kostka, piec i kolumna liczone raz, potem kopia na P
    const bool monoIn = pedalSource && ! trueDbl && nCh >= 2
                     && std::memcmp (block.getChannelPointer (0), block.getChannelPointer (1), n * sizeof (float)) == 0;
    auto work = monoIn ? block.getSubsetChannelBlock (0, 1) : block;
    const size_t wCh = monoIn ? 1 : nCh;

    // DI basu: czysty sygnał sprzed kostki i pieca (dół pod wzmacniaczem)
    const bool bass = source == sxa::Bass;
    if (bass)
        for (size_t c = 0; c < wCh; ++c)
            juce::FloatVectorOperations::copy (diBuf.getWritePointer ((int) c), work.getChannelPointer (c), (int) n);

    // --- kostka (zawsze przez oversampling: stałe opóźnienie) ----------------
    {
        auto up = pedalOS->processSamplesUp (work);
        const auto nUp = up.getNumSamples();
        // oszczędzanie: wyłączona kostka po ~3 s (gdy ogony wybrzmiały) przestaje liczyć
        pedalMix.setTargetValue (s.pedalOn > 0.5f ? 1.f : 0.f);
        const bool pedalSilent = s.pedalOn < 0.5f && ! pedalMix.isSmoothing() && pedalMix.getCurrentValue() <= 0.f;
        pedalIdleSeconds = pedalSilent ? pedalIdleSeconds + (double) n / sr : 0.0;
        if (pedalSource && pedalIdleSeconds < 3.0)
        {
            stonoga::Params p = pedalIn;
            for (size_t c = 0; c < wCh; ++c)
                juce::FloatVectorOperations::copy (osDry.getWritePointer ((int) c), up.getChannelPointer (c), (int) nUp);

            std::array<float*, 8> ptrs {};
            const int engCh = (int) juce::jmin (wCh, ptrs.size());
            for (int c = 0; c < engCh; ++c) ptrs[(size_t) c] = up.getChannelPointer ((size_t) c);
            pedal.process (ptrs.data(), engCh, (int) nUp, p);

            if (pedalMix.isSmoothing() || pedalMix.getTargetValue() < 1.f)
                for (size_t i = 0; i < nUp; ++i)
                {
                    const float m = pedalMix.getNextValue();
                    for (size_t c = 0; c < wCh; ++c)
                    {
                        auto* w = up.getChannelPointer (c);
                        const float dry = osDry.getSample ((int) c, (int) i);
                        w[i] = dry + m * (w[i] - dry);
                    }
                }
        }
        pedalOS->processSamplesDown (work);
    }

    // --- wzmacniacz i kolumna -------------------------------------------------
    amp.setPassThrough (! pedalSource);
    amp.setGain (s.ampInput);
    amp.setDrive (s.ampDrive);
    amp.setModel ((int) std::round (s.ampModel));
    amp.setPresence (s.ampPresence);
    amp.setResonance (s.ampResonance);
    amp.setSag (s.ampSag);
    amp.process (work);

    if (pedalSource)
    {
        cab.setBassVoicing (bass);
        cab.setBlend (s.cabBlend);
        cab.setMic ((int) std::round (s.cabMic), s.micPos, s.micDist);
        cab.process (work);
    }

    // DI / piec: 0,5 = jak dotąd (0,8 piec + 0,9 czystego dołu z linii)
    const float ampG = 0.8f * std::min (1.f, 2.f * (1.f - s.baDi));
    const float diG  = 0.9f * std::min (1.f, 2.f * s.baDi);
    if (bass)   // DI: czysty dół pod wzmacniaczem, jak przy nagraniu basu z linii + pieca
        for (size_t c = 0; c < wCh && c < diLp.size(); ++c)
        {
            auto* d = work.getChannelPointer (c);
            const auto* di = diBuf.getReadPointer ((int) c);
            for (size_t i = 0; i < n; ++i)
                d[i] = ampG * d[i] + diG * diLp[c].process (di[i]);
        }

    if (monoIn) juce::FloatVectorOperations::copy (block.getChannelPointer (1), block.getChannelPointer (0), (int) n);

    // --- barwa ------------------------------------------------------------------
    tone.process (block, s);
    voice.process (block, s);
    transient.process (block, s.trAttack, s.trSustain);

    // --- na źródle ----------------------------------------------------------------
    srcComp.setAmount (s.srcComp);
    srcComp.setCharacter ((int) s.compChar);
    srcComp.setManual (s.compMode > 0.5f, s.compThresh, s.compRatio, s.compAttack, s.compRelease,
                       s.compMakeup, s.compMix, s.compAllButtons > 0.5f);
    srcComp.process (block);
    deEsser.process (block, s.deEss);
    chorus.set (s.chorus, s.chRate, s.chDepth);
    chorus.process (block);
    if (trueDbl)
    {
        // dwa podejścia już są w L i P: panorama = ile ich rozsunąć
        if (nCh >= 2)
        {
            auto* l = block.getChannelPointer (0);
            auto* r = block.getChannelPointer (1);
            const float w = s.srcWidth;
            for (size_t i = 0; i < n; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]), sd = 0.5f * (l[i] - r[i]) * w;
                l[i] = m + sd; r[i] = m - sd;
            }
        }
        srcSpace.set (s.srcSmear, 0.f, s.srcLoose, s.srcSize);
    }
    else srcSpace.set (s.srcSmear, s.srcWidth, s.srcLoose, s.srcSize);
    srcSpace.process (block);
    slap.setAmount (s.slap);
    slap.process (block);

    // --- ustępowanie innym instrumentom ---------------------------------------
    duck.process (block, s, scKick, scVocal);

    // --- studio -------------------------------------------------------------------
    static const float roomHp[sxa::NumSources] = { 180.f, 120.f, 60.f, 250.f, 150.f };
    room.setInputHighpass (roomHp[source]);
    room.setMix (s.roomMix);
    room.setSize (s.roomSize);
    room.setCrush (s.roomCrush);
    room.setPredelay (s.roomPredelay);
    room.process (block);
    console.setModel ((int) s.consoleModel);
    console.setAmount (s.console);
    console.process (block);
    tape.setAmount (s.tape);
    tape.setModel ((int) std::round (s.tapeModel));
    tape.setSlowSpeed (s.tapeSpeed > 0.5f);
    tape.process (block);

    // --- na całości -----------------------------------------------------------------
    busComp.setCharacter ((int) s.busCompChar);
    busComp.setAmount (s.busComp);
    busComp.setSidechainHighpass (s.busScHpf);
    busComp.setAutoMakeup (true);
    busComp.setManual (s.busMode > 0.5f, s.busThresh, s.busRatio, s.busAttack, s.busRelease, 0.f, s.busMix, false);
    busComp.process (block);
    busSpace.set (s.smear, s.width, 0.f, 0.55f);
    busSpace.process (block);

    // --- barwa płyty (tylko na sumie) ----------------------------------------------
    if (source == sxa::Mix) album.process (block, s.albumAmt);
    if (source == sxa::Mix) midSide.process (block, s.msMonoBass, s.msSideHigh, s.msMidPresence);

    // --- wygładzanie góry (dynamiczne dołki w 2,5-14 kHz) -------------------------
    deHarsh.process (block, s.deHarsh);

    // --- ciągłe dopasowanie: wolno uśredniane widmo sumy (stała czasowa ~6 s) -------
    if (source == sxa::Mix && s.albumTrack > 0.5f)
    {
        const auto* l = block.getChannelPointer (0);
        const auto* r = nCh > 1 ? block.getChannelPointer (1) : l;
        double e = 0.0, bandE[AlbumEq::numBands] {};
        for (size_t i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            e += (double) m * m;
            for (int b = 0; b < AlbumEq::numBands; ++b) { const float y = trackBp[b].process (m); bandE[b] += (double) y * y; }
        }
        if (e / (double) n > 1e-6)   // tylko gdy coś gra
        {
            const float k = 1.f - std::exp (-(float) n / (6.f * (float) sr));
            for (int b = 0; b < AlbumEq::numBands; ++b)
            {
                const float cur = trackMs[(size_t) b].load();
                trackMs[(size_t) b] = cur + k * ((float) (bandE[b] / (double) n) - cur);
            }
            trackSeconds = trackSeconds.load() + (float) n / (float) sr;
        }
    }

    // --- pomiar -------------------------------------------------------------------------
    if (measureReset.exchange (false))
    {
        sumSq = 0.0; counted = 0;
        measuredMs = 0.f; activeSeconds = 0.f;
        for (int b = 0; b < sxa::kNumEqBands; ++b) { bandSq[b] = 0.0; bandMs[(size_t) b] = 0.f; bandBp[b].reset(); }
        midSq = sideSq = 0.0; midMs = 0.f; sideMs = 0.f;
        outSq = 0.0; outCounted = 0; outMs = 0.f;
        for (int b = 0; b < AlbumEq::numBands; ++b) { albumSq[b] = 0.0; albumMs[(size_t) b] = 0.f; albumBp[b].reset(); }
        for (int c = 0; c < 2; ++c) { kHp[c].reset(); kShelf[c].reset(); }
    }
    if (measuring.load())
    {
        double blockSq = 0.0;
        const auto* l = block.getChannelPointer (0);
        const auto* r = nCh > 1 ? block.getChannelPointer (1) : l;
        double bandBlock[sxa::kNumEqBands] {};
        double midBlock = 0.0, sideBlock = 0.0;
        double albumBlock[AlbumEq::numBands] {};
        const bool fine = source == sxa::Mix;   // 26 pasm mierzy tylko suma
        for (size_t i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            const float sd = 0.5f * (l[i] - r[i]);
            midBlock += (double) m * m; sideBlock += (double) sd * sd;
            if (fine) for (int b = 0; b < AlbumEq::numBands; ++b) { const float y = albumBp[b].process (m); albumBlock[b] += (double) y * y; }
            for (int b = 0; b < sxa::kNumEqBands; ++b) { const float y = bandBp[b].process (m); bandBlock[b] += (double) y * y; }
        }
        for (size_t i = 0; i < n; ++i)   // moc obu kanałów: panorama ze stałą mocą jej nie zmienia
        {
            const float kl = kShelf[0].process (kHp[0].process (l[i]));
            const float kr = kShelf[1].process (kHp[1].process (r[i]));
            blockSq += 0.5 * ((double) kl * kl + (double) kr * kr);
        }
        if (blockSq / (double) n > 1e-6)      // tylko gdy coś gra (> -60 dB)
        {
            sumSq += blockSq; counted += (long long) n;
            measuredMs = (float) (sumSq / (double) counted);
            for (int b = 0; b < sxa::kNumEqBands; ++b)
            {
                bandSq[b] += bandBlock[b];
                bandMs[(size_t) b] = (float) (bandSq[b] / (double) counted);
            }
            midSq += midBlock; sideSq += sideBlock;
            if (fine) for (int b = 0; b < AlbumEq::numBands; ++b) { albumSq[b] += albumBlock[b]; albumMs[(size_t) b] = (float) (albumSq[b] / (double) counted); }
            midMs = (float) (midSq / (double) counted);
            sideMs = (float) (sideSq / (double) counted);
            activeSeconds = (float) ((double) counted / sr);
        }
    }

    // --- wyjście ---------------------------------------------------------------------------
    {
        const bool silent = s.mixMute > 0.5f || s.soloMuted > 0.5f || s.mixFader <= -59.9f;
        faderGain.setTargetValue (silent ? 0.f : juce::Decibels::decibelsToGain (s.autoGain + s.outputGain + s.mixFader));
    }
    // panorama ze stałą mocą: na środku 1/1, skrajnie w lewo 1.41/0
    const float pan = juce::jlimit (-1.f, 1.f, s.pan);
    const float th = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
    panL.setTargetValue (juce::MathConstants<float>::sqrt2 * std::cos (th));
    panR.setTargetValue (juce::MathConstants<float>::sqrt2 * std::sin (th));
    {
        auto* l = block.getChannelPointer (0);
        auto* r = nCh > 1 ? block.getChannelPointer (1) : nullptr;
        for (size_t i = 0; i < n; ++i)
        {
            const float g = faderGain.getNextValue();
            const float gl = panL.getNextValue(), gr = panR.getNextValue();
            if (r) { l[i] *= g * gl; r[i] *= g * gr; }
            else     l[i] *= g;
        }
    }

    guard.process (block, s.protect);
    limiter.process (block, -1.f);
    {
        float pk = outPeak.load (std::memory_order_relaxed);
        for (size_t c = 0; c < nCh; ++c) pk = std::max (pk, block.getSubsetChannelBlock (c, 1).findMinAndMax().getEnd());
        for (size_t c = 0; c < nCh; ++c) pk = std::max (pk, -block.getSubsetChannelBlock (c, 1).findMinAndMax().getStart());
        outPeak.store (pk, std::memory_order_relaxed);
    }

    // pomiar na samym wyjściu (do doregulowania głośności sumy)
    if (measuring.load())
    {
        const auto* l = block.getChannelPointer (0);
        const auto* r = nCh > 1 ? block.getChannelPointer (1) : l;
        double sq = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const float kl = kShelfOut[0].process (kHpOut[0].process (l[i]));
            const float kr = kShelfOut[1].process (kHpOut[1].process (r[i]));
            sq += 0.5 * ((double) kl * kl + (double) kr * kr);
        }
        if (sq / (double) n > 1e-6) { outSq += sq; outCounted += (long long) n; outMs = (float) (outSq / (double) outCounted); }
    }
}
