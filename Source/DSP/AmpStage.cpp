#include "AmpStage.h"

namespace
{
    constexpr float maxDriveDb = 30.0f;   // model „Albini”
}

const AmpStage::ModelDef& AmpStage::def (int m)
{
    //                 inHpf  brightHz dB   maxDb st bias  interLp interHp  lowHz lowDb  midHz Q    midDb  hiHz  hiDb  power trim
    static const ModelDef defs[NumModels] = {
        {  90.f, 1800.f, 3.f, 30.f, 1, 0.20f, 11000.f, 20.f,  100.f, 0.f,   700.f, 0.8f, 0.f,  3000.f, 0.f,  0.f,  0.f }, // (nieużywany)
        { 100.f, 2500.f, 4.f, 44.f, 2, 0.25f,  9000.f, 70.f,  100.f, -1.f,  700.f, 0.8f, 3.5f, 3000.f, 2.f, 10.f, -9.0f }, // brytyjski 800
        { 120.f, 3000.f, 3.f, 56.f, 3, 0.15f,  8000.f, 90.f,  120.f, 3.f,   600.f, 0.7f, -5.f, 3500.f, 2.f, 14.f, -10.0f }, // prostownik
        {  60.f, 2000.f, 5.f, 24.f, 1, 0.06f, 10000.f, 30.f,  150.f, 2.f,   450.f, 0.6f, -4.f, 4000.f, 2.5f, 12.f, -5.0f }, // amerykański czysty
        {  30.f, 1500.f, 2.f, 30.f, 2, 0.20f,  5000.f, 25.f,   80.f, 3.f,   450.f, 0.8f, 1.5f, 4000.f, -3.f, 12.f, -10.0f }, // basowy SVT
        // zepsuty Fender Quad Reverb (In Utero): jedna lampa mocy, przepalony głośnik, bright włączony
        {  70.f, 2000.f, 6.f, 26.f, 1, 0.10f,  9000.f, 40.f,  120.f, 1.f,   450.f, 0.6f, -2.f, 4000.f, 2.f, 16.f, -7.0f, 0.45f, 0.35f, 0, 1.0f },
        // Marshall po przeróbce Bognera (Alice in Chains): dodatkowy stopień, czytelny dół, warczący środek
        { 100.f, 2200.f, 3.f, 52.f, 3, 0.22f,  8500.f, 80.f,  110.f, 1.5f,  800.f, 0.8f, 3.f,  3200.f, 1.5f, 10.f, -12.0f },
        // Vox AC30 top boost: klasa A, dzwoniąca góra, mniej dołu
        {  60.f, 2800.f, 5.f, 30.f, 2, 0.30f,  9000.f, 50.f,  100.f, -2.f,  900.f, 0.7f, -1.5f, 2800.f, 4.f, 14.f, -9.0f, 0.20f, 0.f, 0, 0.5f },
        // Fender Bassman (tweed): luźny dół, środek, wcześnie się rwie
        {  40.f, 1500.f, 2.f, 36.f, 2, 0.25f,  7000.f, 30.f,  110.f, 2.5f,  700.f, 0.7f, 1.5f, 3000.f, 1.f, 14.f, -10.0f, 0.f, 0.f, 0, 0.8f },
        // Fender Super Reverb (blackface, 4x10): jasno, wycięty środek
        {  60.f, 2200.f, 5.f, 26.f, 1, 0.08f, 11000.f, 40.f,  140.f, 1.f,   500.f, 0.6f, -3.5f, 4000.f, 3.5f, 13.f, -6.0f },
        // Peavey Roadmaster (Sonic Youth): jasny, przebojowy górny środek
        {  80.f, 2500.f, 4.f, 40.f, 2, 0.20f,  8500.f, 60.f,  100.f, -1.f, 1200.f, 0.8f, 2.f,  3500.f, 3.f, 12.f, -9.0f },
        // Mesa Studio preamp + końcówka tranzystorowa (Nirvana na żywo): podbity środek, twardo, bez sag
        {  90.f, 2500.f, 3.f, 46.f, 3, 0.18f,  7500.f, 80.f,  120.f, 1.f,   750.f, 0.8f, 4.f,  3000.f, 2.f,  8.f, -13.0f, 0.f, 0.f, 1, 0.f },
    };
    return defs[juce::jlimit (0, (int) NumModels - 1, m)];
}

void AmpStage::prepare (const juce::dsp::ProcessSpec& spec, int osFactorLog2)
{
    sampleRate = spec.sampleRate;
    osLog2 = juce::jlimit (1, 3, osFactorLog2);

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.05);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) spec.numChannels, (size_t) osLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) spec.maximumBlockSize);

    brightShelf.prepare (spec);
    lastShelfDrive = -1.0f;
    updateBrightShelf (driveSmoothed.getTargetValue());

    couplingHP.assign (spec.numChannels, {});
    postLP.assign (spec.numChannels, {});
    dc.assign (spec.numChannels, {});
    for (auto& f : couplingHP) f.setCutoff (sampleRate, 90.0f);   // tight, not boomy
    for (auto& f : postLP)     f.setCutoff (sampleRate, 11000.0f);
    for (auto& d : dc)         d.prepare (sampleRate);

    const auto osBlock = (size_t) spec.maximumBlockSize << osLog2;
    gainRamp.assign (osBlock, 1.0f);
    biasRamp.assign (osBlock, 0.0f);
    compRamp.assign (osBlock, 1.0f);

    const double osRate = sampleRate * (double) (1 << osLog2);
    driveSmoothed.reset (osRate, 0.05);
    sagAtt = (float) std::exp (-1.0 / (0.004 * osRate));
    blkAtt = (float) std::exp (-1.0 / (0.001 * osRate));
    blkRel = (float) std::exp (-1.0 / (0.06 * osRate));
    sagRel = (float) std::exp (-1.0 / (0.15 * osRate));

    appliedModel = -1;
    appliedPresence = appliedResonance = 99.f;
    updateModelFilters();
    updateVoiceFilters();
    reset();
}

void AmpStage::reset()
{
    inputGain.reset();
    if (oversampling) oversampling->reset();
    brightShelf.reset();
    for (auto& f : couplingHP) f.reset();
    for (auto& f : postLP)     f.reset();
    for (auto& d : dc)         d.reset();
    for (int c = 0; c < maxCh; ++c)
    {
        for (int s = 0; s < maxStages; ++s) { interLp[c][s].reset(); interHp[c][s].reset(); }
        toneLow[c].reset(); toneMid[c].reset(); toneHigh[c].reset();
        modelHp[c].reset(); presenceF[c].reset(); resonanceF[c].reset();
        buzzLp[c].reset(); buzzHp[c].reset();
        xfmrPre[c].reset(); xfmrPost[c].reset();
        for (auto& e : blockEnv[c]) e = 0.f;
    }
    sagEnv = 0.f;
}

void AmpStage::setGain (float gainDb)  { inputGain.setGainDecibels (gainDb); }
void AmpStage::setDrive (float drive)  { driveSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, drive)); }
void AmpStage::setModel (int m)        { model = juce::jlimit (0, (int) NumModels - 1, m); }

float AmpStage::getLatencyInSamples() const
{
    return oversampling ? oversampling->getLatencyInSamples() : 0.0f;
}

void AmpStage::updateBrightShelf (float drive)
{
    if (std::abs (drive - lastShelfDrive) < 0.01f && appliedModel == model)
        return;

    lastShelfDrive = drive;
    // Więcej góry do clippera przy większym drive: przester zostaje czytelny.
    float hz = 1800.f, gainDb = 1.0f + 5.0f * drive;
    if (model != Albini)
    {
        const auto& d = def (model);
        hz = d.brightHz;
        gainDb = d.brightDb * (1.f - 0.5f * drive);   // jasny kondensator słabnie przy pełnym gainie
    }
    *brightShelf.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        sampleRate, hz, 0.6f, juce::Decibels::decibelsToGain (gainDb));
}

void AmpStage::updateModelFilters()
{
    if (appliedModel == model) return;
    appliedModel = model;
    cachedDrive = -1.f;
    lastShelfDrive = -1.f;

    const auto& d = def (model);
    const double osRate = sampleRate * (double) (1 << osLog2);
    for (int c = 0; c < maxCh; ++c)
    {
        for (int s = 0; s < maxStages; ++s)
        {
            // każdy kolejny stopień nieco ciemniejszy, jak pojemności Millera w lampach
            interLp[c][s].lowpass (osRate, d.interLp * (1.f - 0.12f * (float) s), 0.7f);
            interHp[c][s].highpass (osRate, d.interHpf, 0.6f);
        }
        toneLow[c].lowshelf (osRate, d.lowHz, d.lowDb);
        toneMid[c].peak (osRate, d.midHz, d.midQ, d.midDb);
        toneHigh[c].highshelf (osRate, d.hiHz, d.hiDb);
        modelHp[c].highpass ((float) sampleRate, d.inHpf, 0.7f);
        buzzLp[c].lowpass (osRate, 250.0, 0.7);
        xfmrPre[c].lowshelf (osRate, 90.0, 5.0);
        xfmrPost[c].lowshelf (osRate, 90.0, -5.0);
        buzzHp[c].highpass (osRate, 1200.0, 0.7);
    }
}

void AmpStage::updateVoiceFilters()
{
    if (std::abs (presence - appliedPresence) < 0.005f && std::abs (resonance - appliedResonance) < 0.005f) return;
    appliedPresence = presence; appliedResonance = resonance;
    for (int c = 0; c < maxCh; ++c)
    {
        presenceF[c].highshelf ((float) sampleRate, 3500.f, 6.f * presence);
        resonanceF[c].peak ((float) sampleRate, 100.f, 0.9f, 6.f * resonance);
    }
}

void AmpStage::processModelled (juce::dsp::AudioBlock<float>& up)
{
    const auto& d = def (model);
    const auto nUp = up.getNumSamples();
    const int nCh = (int) juce::jmin (up.getNumChannels(), (size_t) maxCh);
    const float stageExp = 1.f / (float) d.stages;
    const float pg = juce::Decibels::decibelsToGain (d.powerDb);
    const float pgComp = 1.f / std::sqrt (pg);
    const float trim = juce::Decibels::decibelsToGain (d.trimDb);
    const float bOdd = -0.6f * d.bias;

    float* ch[maxCh] {};
    for (int c = 0; c < nCh; ++c) ch[c] = up.getChannelPointer ((size_t) c);

    // wzmocnienia zależne od drive liczone tylko, gdy gałka się rusza
    float gs = cachedGs, comp = cachedComp;
    for (size_t i = 0; i < nUp; ++i)
    {
        if (driveSmoothed.isSmoothing() || cachedDrive < 0.f)
        {
            const float drive = driveSmoothed.getNextValue();
            const float gTotal = juce::Decibels::decibelsToGain (drive * d.maxDb);
            gs = std::pow (gTotal, stageExp);
            comp = 1.f / std::sqrt (gTotal);
            cachedDrive = drive; cachedGs = gs; cachedComp = comp;
        }

        // zasilanie: obwiednia sygnału na końcówce, wspólna dla kanałów
        const float supply = d.solidState ? 1.f
                           : juce::jmax (0.3f, 1.f - 0.45f * sag * (1.f + d.sagExtra) * juce::jmin (1.f, sagEnv));
        float peak = 0.f;

        for (int c = 0; c < nCh; ++c)
        {
            float x = ch[c][i];
            for (int s = 0; s < d.stages; ++s)
            {
                // trioda: od góry prąd siatki (twardo, nisko), od dołu odcięcie (miękko, wyżej);
                // po mocnym uderzeniu kondensator sprzęgający „zatyka” stopień -> brud i oddech przy wybrzmieniu
                float& be = blockEnv[c][s];
                const float v = x * gs + ((s & 1) ? bOdd : d.bias) - 0.6f * be;
                const float gridCur = juce::jmax (0.f, v - 0.35f);
                be = gridCur > be ? blkAtt * be + (1.f - blkAtt) * gridCur : blkRel * be + (1.f - blkRel) * gridCur;
                x = v > 0.f ? 0.62f * fastTanh (v / 0.62f) : fastTanh (v);
                if (s & 1) x = -x;    // stopień odwracający
                x = interLp[c][s].process (interHp[c][s].process (x));
            }
            x *= comp * (float) d.stages;
            x = toneHigh[c].process (toneMid[c].process (toneLow[c].process (x)));

            // końcówka mocy przeciwsobna: symetryczne, miękkie obcinanie do napięcia zasilania
            // transformator: dół wchodzi w nasycenie mocniej (podbicie przed, zdjęcie po obcięciu)
            const float drivePow = (d.solidState ? x : xfmrPre[c].process (x)) * pg;
            peak = juce::jmax (peak, std::abs (drivePow) * 0.5f);
            const float u = drivePow / supply;
            float y;
            if (d.solidState)                       // tranzystor: prawie twarde obcięcie
                y = 0.7f * juce::jlimit (-1.f, 1.f, u) + 0.3f * fastTanh (u);
            else if (d.powerAsym > 0.f)             // jedna lampa / klasa A: połówki obcinane inaczej
                y = supply * fastTanh (u > 0.f ? u * (1.f + d.powerAsym) : u * (1.f - 0.7f * d.powerAsym));
            else
                y = supply * fastTanh (u);
            if (! d.solidState) y = xfmrPost[c].process (y);
            if (d.buzz > 0.f)
            {
                // przepalony głośnik: mocny dół „grzechocze” - nadmiar ponad próg, przefiltrowany w górę
                const float r = buzzLp[c].process (y);
                const float excess = r - juce::jlimit (-0.12f, 0.12f, r);
                y += d.buzz * 2.5f * buzzHp[c].process (excess);
            }
            ch[c][i] = y * pgComp * trim;
        }

        sagEnv = peak > sagEnv ? sagAtt * sagEnv + (1.f - sagAtt) * peak
                               : sagRel * sagEnv + (1.f - sagRel) * peak;
    }
}

void AmpStage::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh = juce::jmin (block.getNumChannels(), couplingHP.size());
    const auto n     = block.getNumSamples();

    if (passThrough)
    {
        oversampling->processSamplesUp (block);
        oversampling->processSamplesDown (block);
        return;
    }

    updateModelFilters();
    updateVoiceFilters();

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    inputGain.process (ctx);

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        if (model == Albini)
            for (size_t i = 0; i < n; ++i) d[i] = couplingHP[ch].highpass (d[i]);
        else if (ch < (size_t) maxCh)
            for (size_t i = 0; i < n; ++i) d[i] = modelHp[ch].process (d[i]);
    }

    updateBrightShelf (driveSmoothed.getTargetValue());
    brightShelf.process (ctx);

    auto up = oversampling->processSamplesUp (block);
    const auto nUp = up.getNumSamples();

    if (model == Albini)
    {
        if (driveSmoothed.isSmoothing())
        {
            for (size_t i = 0; i < nUp; ++i)
            {
                const float drive = driveSmoothed.getNextValue();
                const float g     = juce::Decibels::decibelsToGain (drive * maxDriveDb);
                gainRamp[i] = g;
                biasRamp[i] = 0.1f + 0.2f * drive;          // asymetria -> parzyste harmoniczne
                compRamp[i] = 1.0f / std::sqrt (g);         // przybliżona kompensacja poziomu
            }
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < nUp; ++i)
                {
                    const float b = biasRamp[i];
                    d[i] = (fastTanh (d[i] * gainRamp[i] + b) - fastTanh (b)) * compRamp[i];
                }
            }
        }
        else
        {
            // gałka stoi: stałe wzmocnienie, bez exp/sqrt na każdą próbkę
            const float drive = driveSmoothed.getCurrentValue();
            const float g = juce::Decibels::decibelsToGain (drive * maxDriveDb);
            const float b = 0.1f + 0.2f * drive, tb = fastTanh (b), c = 1.0f / std::sqrt (g);
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < nUp; ++i) d[i] = (fastTanh (d[i] * g + b) - tb) * c;
            }
        }
    }
    else
    {
        processModelled (up);
    }

    oversampling->processSamplesDown (block);

    const bool voice = std::abs (presence) > 0.005f || std::abs (resonance) > 0.005f;
    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        for (size_t i = 0; i < n; ++i)
            d[i] = postLP[ch].lowpass (dc[ch].process (d[i]));
        if (voice && ch < (size_t) maxCh)
            for (size_t i = 0; i < n; ++i)
                d[i] = resonanceF[ch].process (presenceF[ch].process (d[i]));
    }
}
