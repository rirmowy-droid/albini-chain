#include "ChainEngine.h"

void ChainEngine::prepare (double sampleRate, int maxBlock, int numChannels)
{
    sr = sampleRate;
    maxBlockSize = juce::jmax (1, maxBlock);
    const int nch = juce::jmax (1, numChannels);
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) nch };

    pedalOS = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) nch, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    pedalOS->initProcessing ((size_t) maxBlockSize);
    pedal.builtInProtection = false;
    pedal.prepare (sampleRate * 4.0, nch);
    osDry.setSize (nch, maxBlockSize * 4, false, true, true);
    diBuf.setSize (nch, maxBlockSize, false, true, true);
    pedalMix.reset (sampleRate * 4.0, 0.02);
    pedalMix.setCurrentAndTargetValue (1.f);
    for (auto& f : diLp) { f.lowpass ((float) sampleRate, 250.f, 0.7f); f.reset(); }

    amp.prepare (spec);
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
    }
}

float ChainEngine::getLatencyInSamples() const
{
    return (pedalOS ? pedalOS->getLatencyInSamples() : 0.f)
         + amp.getLatencyInSamples()
         + console.getLatencyInSamples()
         + (float) tape.getLatencyInSamples();
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

    // DI basu: czysty sygnał sprzed kostki i pieca (dół pod wzmacniaczem)
    const bool bass = source == sxa::Bass;
    if (bass)
        for (size_t c = 0; c < nCh; ++c)
            juce::FloatVectorOperations::copy (diBuf.getWritePointer ((int) c), block.getChannelPointer (c), (int) n);

    // --- kostka (zawsze przez oversampling: stałe opóźnienie) ----------------
    {
        auto up = pedalOS->processSamplesUp (block);
        const auto nUp = up.getNumSamples();
        if (pedalSource)
        {
            stonoga::Params p = pedalIn;
            for (size_t c = 0; c < nCh; ++c)
                juce::FloatVectorOperations::copy (osDry.getWritePointer ((int) c), up.getChannelPointer (c), (int) nUp);

            std::array<float*, 8> ptrs {};
            const int engCh = (int) juce::jmin (nCh, ptrs.size());
            for (int c = 0; c < engCh; ++c) ptrs[(size_t) c] = up.getChannelPointer ((size_t) c);
            pedal.process (ptrs.data(), engCh, (int) nUp, p);

            pedalMix.setTargetValue (s.pedalOn > 0.5f ? 1.f : 0.f);
            if (pedalMix.isSmoothing() || pedalMix.getTargetValue() < 1.f)
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
    }

    // --- wzmacniacz i kolumna -------------------------------------------------
    amp.setPassThrough (! pedalSource);
    amp.setGain (s.ampInput);
    amp.setDrive (s.ampDrive);
    amp.process (block);

    if (pedalSource)
    {
        cab.setBassVoicing (bass);
        cab.setBlend (s.cabBlend);
        cab.process (block);
    }

    if (bass)   // DI: czysty dół pod wzmacniaczem, jak przy nagraniu basu z linii + pieca
        for (size_t c = 0; c < nCh && c < diLp.size(); ++c)
        {
            auto* d = block.getChannelPointer (c);
            const auto* di = diBuf.getReadPointer ((int) c);
            for (size_t i = 0; i < n; ++i)
                d[i] = 0.8f * d[i] + 0.9f * diLp[c].process (di[i]);
        }

    // --- barwa ------------------------------------------------------------------
    tone.process (block, s);

    // --- na źródle ----------------------------------------------------------------
    srcComp.setAmount (s.srcComp);
    srcComp.process (block);
    chorus.set (s.chorus, s.chRate, s.chDepth);
    chorus.process (block);
    srcSpace.set (s.srcSmear, s.srcWidth, s.srcLoose, s.srcSize);
    srcSpace.process (block);

    // --- studio -------------------------------------------------------------------
    static const float roomHp[sxa::NumSources] = { 180.f, 120.f, 60.f, 250.f, 150.f };
    room.setInputHighpass (roomHp[source]);
    room.setMix (s.roomMix);
    room.setSize (s.roomSize);
    room.setCrush (s.roomCrush);
    room.process (block);
    console.setAmount (s.console);
    console.process (block);
    tape.setAmount (s.tape);
    tape.process (block);

    // --- na całości -----------------------------------------------------------------
    busComp.setAmount (s.busComp);
    busComp.process (block);
    busSpace.set (s.smear, s.width, 0.f, 0.55f);
    busSpace.process (block);

    // --- pomiar -------------------------------------------------------------------------
    if (measureReset.exchange (false))
    {
        sumSq = 0.0; counted = 0;
        measuredMs = 0.f; activeSeconds = 0.f;
        for (int c = 0; c < 2; ++c) { kHp[c].reset(); kShelf[c].reset(); }
    }
    if (measuring.load())
    {
        double blockSq = 0.0;
        const auto* l = block.getChannelPointer (0);
        const auto* r = nCh > 1 ? block.getChannelPointer (1) : l;
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
            activeSeconds = (float) ((double) counted / sr);
        }
    }

    // --- wyjście ---------------------------------------------------------------------------
    faderGain.setTargetValue (juce::Decibels::decibelsToGain (s.autoGain + s.outputGain));
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
}
