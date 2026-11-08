#include "CabinetStage.h"

using Coeffs = juce::dsp::IIR::Coefficients<float>;

CabinetStage::CabinetStage() { formats.registerBasicFormats(); }

void CabinetStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (auto& c : conv) c.prepare (spec);
    for (auto& g : gain) g.reset (sampleRate, 0.03);

    scratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, true, true);

    *fbHighPass.state = *Coeffs::makeHighPass (sampleRate, 75.0f, 0.8f);
    *fbPeak.state     = *Coeffs::makePeakFilter (sampleRate, 2400.0f, 1.1f, juce::Decibels::decibelsToGain (3.5f));
    *fbLowPass1.state = *Coeffs::makeLowPass (sampleRate, 5200.0f, 0.75f);
    *fbLowPass2.state = *Coeffs::makeLowPass (sampleRate, 6500.0f, 0.6f);
    appliedBassVoicing = ! bassVoicing;
    applyFallbackVoicing();

    for (auto* f : { &fbHighPass, &fbPeak, &fbLowPass1, &fbLowPass2 })
        f->prepare (spec);

    for (auto& d : bounce) d.init ((size_t) (sampleRate * 0.004) + 4);
    appliedMic = -1;
    updateMicFilters();

    reset();
}

void CabinetStage::applyFallbackVoicing()
{
    if (appliedBassVoicing == bassVoicing)
        return;
    appliedBassVoicing = bassVoicing;

    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    const auto db = [] (float d) { return juce::Decibels::decibelsToGain (d); };
    if (bassVoicing)
    {
        *fbHighPass.state = AC::makeHighPass (sampleRate, 35.0f, 0.7f);
        *fbPeak.state     = AC::makePeakFilter (sampleRate, 700.0f, 0.9f, db (3.0f));   // warkot
        *fbLowPass1.state = AC::makeLowPass (sampleRate, 3800.0f, 0.7f);
        *fbLowPass2.state = AC::makeLowPass (sampleRate, 5000.0f, 0.6f);
    }
    else
    {
        *fbHighPass.state = AC::makeHighPass (sampleRate, 75.0f, 0.8f);
        *fbPeak.state     = AC::makePeakFilter (sampleRate, 2400.0f, 1.1f, db (3.5f));
        *fbLowPass1.state = AC::makeLowPass (sampleRate, 5200.0f, 0.75f);
        *fbLowPass2.state = AC::makeLowPass (sampleRate, 6500.0f, 0.6f);
    }
}

void CabinetStage::reset()
{
    for (auto& c : conv) c.reset();
    for (auto* f : { &fbHighPass, &fbPeak, &fbLowPass1, &fbLowPass2 })
        f->reset();
    for (int c = 0; c < 2; ++c)
    {
        for (auto* b : { &micA[c], &micB[c], &micC[c], &posF[c], &proxF[c], &bounceLp[c] }) b->reset();
        if (! bounce[c].b.empty()) bounce[c].clear();
    }
}

bool CabinetStage::readIR (const juce::File* file, const void* data, size_t bytes, double maxSeconds,
                           juce::AudioBuffer<float>& out, double& rate)
{
    std::unique_ptr<juce::AudioFormatReader> reader;
    if (file != nullptr)
        reader.reset (formats.createReaderFor (*file));
    else
        reader.reset (formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, bytes, false)));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
        return false;

    rate = reader->sampleRate;
    auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (rate * 2.0));   // max 2 s
    if (maxSeconds > 0) len = juce::jmin (len, (int) (maxSeconds * rate));
    // mono: z pliku stereo bierzemy lewy kanał (jak wcześniej)
    out.setSize (1, len);
    reader->read (&out, 0, len, 0, true, false);

    // łagodne wygaszenie końca przyciętego IR (bez trzasku)
    const int fade = juce::jmin (len / 4, (int) (rate * 0.01));
    for (int i = 0; i < fade; ++i)
        out.setSample (0, len - 1 - i, out.getSample (0, len - 1 - i) * (float) i / (float) fade);
    return true;
}

void CabinetStage::computeAlignment (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
                                     double rate, int& shift, bool& flip)
{
    // korelacja wzajemna pierwszych 10 ms, przesunięcia do ±2 ms
    const int maxLag = (int) (rate * 0.002);
    const int win = juce::jmin ((int) (rate * 0.01), juce::jmin (a.getNumSamples(), b.getNumSamples()));
    const float* pa = a.getReadPointer (0);
    const float* pb = b.getReadPointer (0);
    double best = 0.0; int bestLag = 0;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double c = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const int j = i + lag;
            if (j >= 0 && j < b.getNumSamples()) c += (double) pa[i] * pb[j];
        }
        if (std::abs (c) > std::abs (best)) { best = c; bestLag = lag; }
    }
    shift = bestLag;     // > 0: B jest spóźniony o tyle próbek
    flip = best < 0.0;
}

void CabinetStage::pushToConvolution (int slot)
{
    if (! hasRaw[slot]) return;

    juce::AudioBuffer<float> ir;
    ir.makeCopyOf (raw[slot]);

    if (slot == 1)
    {
        int shift = 0; bool flip = false;
        if (align && hasRaw[0] && std::abs (rawRate[0] - rawRate[1]) < 1.0)
            computeAlignment (raw[0], raw[1], rawRate[1], shift, flip);
        alignShift = shift; alignFlip = flip;

        if (shift != 0 || flip)
        {
            const int len = ir.getNumSamples();
            juce::AudioBuffer<float> moved (1, len);
            moved.clear();
            for (int i = 0; i < len; ++i)
            {
                const int src = i + shift;
                if (src >= 0 && src < len) moved.setSample (0, i, raw[1].getSample (0, src) * (flip ? -1.f : 1.f));
            }
            ir = std::move (moved);
        }
    }

    // Trim::no: zachowujemy początek IR (wyrównanie liczymy sami powyżej)
    conv[slot].loadImpulseResponse (std::move (ir), rawRate[slot],
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::no,
                                    juce::dsp::Convolution::Normalise::yes);
    loaded[slot] = true;
}

bool CabinetStage::loadIR (int slot, const juce::File& file)
{
    slot &= 1;
    if (! file.existsAsFile() || ! readIR (&file, nullptr, 0, 0.0, raw[slot], rawRate[slot]))
        return false;
    hasRaw[slot] = true;
    pushToConvolution (slot);
    if (slot == 0 && hasRaw[1]) pushToConvolution (1);   // wyrównanie B zależy od A
    return true;
}

bool CabinetStage::loadIRFromMemory (int slot, const void* data, size_t bytes, double maxSeconds)
{
    slot &= 1;
    if (data == nullptr || bytes == 0 || ! readIR (nullptr, data, bytes, maxSeconds, raw[slot], rawRate[slot]))
        return false;
    hasRaw[slot] = true;
    pushToConvolution (slot);
    if (slot == 0 && hasRaw[1]) pushToConvolution (1);
    return true;
}

void CabinetStage::clearIR (int slot)
{
    slot &= 1;
    loaded[slot] = false;
    hasRaw[slot] = false;
    if (slot == 0 && hasRaw[1]) pushToConvolution (1);   // bez A nie ma do czego wyrównywać
}

void CabinetStage::setAlign (bool on)
{
    if (on == align) return;
    align = on;
    if (hasRaw[1]) pushToConvolution (1);
}

void CabinetStage::setMic (int m, float position01, float distance01)
{
    micModel = juce::jlimit (0, (int) NumMics - 1, m);
    micPos = juce::jlimit (0.f, 1.f, position01);
    micDist = juce::jlimit (0.f, 1.f, distance01);
}

void CabinetStage::updateMicFilters()
{
    if (appliedMic == micModel && std::abs (appliedPos - micPos) < 0.003f && std::abs (appliedDist - micDist) < 0.003f)
        return;
    appliedMic = micModel; appliedPos = micPos; appliedDist = micDist;
    const float sr = (float) sampleRate;

    // proximity: dynamiczne i wstęgowe mocniej, pojemnościowy (kardioida) słabiej
    const float proxScale = micModel == MicRibbon ? 1.4f : micModel == MicCondenser ? 0.7f : 1.f;
    for (int c = 0; c < 2; ++c)
    {
        switch (micModel)
        {
            case Mic421:        // MD421: więcej dołu i pełni, łagodniejsza prezencja niż 57
                micA[c].lowshelf (sr, 110.f, 2.f);
                micB[c].peak (sr, 2300.f, 1.0f, -1.5f);
                micC[c].peak (sr, 6500.f, 1.4f, 1.5f);
                break;
            case MicRibbon:     // w stylu R-121: ciemno, gładko, bez szpilki prezencji
                micA[c].lowshelf (sr, 160.f, 1.5f);
                micB[c].peak (sr, 4500.f, 1.0f, -2.5f);
                micC[c].highshelf (sr, 7000.f, -5.f);
                break;
            case MicCondenser:  // w stylu U87: równiej, więcej powietrza, mniej „nosa” 57
                micA[c].peak (sr, 200.f, 0.8f, 1.f);
                micB[c].peak (sr, 5500.f, 1.2f, -2.f);
                micC[c].highshelf (sr, 9000.f, 2.5f);
                break;
            default:            // jak w IR / SM57 (IR-y są zwykle nagrane 57)
                micA[c].set (1, 0, 0, 1, 0, 0);
                micB[c].set (1, 0, 0, 1, 0, 0);
                micC[c].set (1, 0, 0, 1, 0, 0);
                break;
        }

        // pozycja: 0 = środek membrany (jasno), 0,5 = jak w IR, 1 = brzeg (ciemno)
        const float p = (micPos - 0.5f) * 2.f;
        posF[c].highshelf (sr, p > 0 ? 2500.f : 4000.f, p > 0 ? -7.f * p : -3.f * p);

        // odległość: 0,3 = jak w IR (ok. 2-3 cm); bliżej = więcej dołu, dalej = mniej dołu + odbicie
        const float prox = micDist < 0.3f ? 4.f * proxScale * (0.3f - micDist) / 0.3f
                                          : -2.5f * (micDist - 0.3f) / 0.7f;
        proxF[c].lowshelf (sr, 160.f, prox);
        bounceLp[c].lowpass (sr, 6000.f, 0.7f);
    }

    const float far = juce::jmax (0.f, micDist - 0.3f) / 0.7f;
    bounceGain = 0.35f * far;
    bounceDelay = (float) (sampleRate * (0.0003 + 0.0025 * far));   // dłuższa droga odbicia przy dalszym mikrofonie

    micActive = micModel != MicAsIR || std::abs (micPos - 0.5f) > 0.003f || std::abs (micDist - 0.3f) > 0.003f;
}

void CabinetStage::process (juce::dsp::AudioBlock<float>& block)
{
    const bool a = loaded[0].load();
    const bool b = loaded[1].load();

    // Clear convolution history when a slot gets (re)activated so stale
    // tails from a previous IR never leak in.
    for (int s = 0; s < 2; ++s)
    {
        const bool now = (s == 0 ? a : b);
        if (now && ! wasLoaded[s])
            conv[s].reset();
        wasLoaded[s] = now;
    }

    juce::dsp::ProcessContextReplacing<float> ctx (block);

    if (! a && ! b)
    {
        applyFallbackVoicing();
        fbHighPass.process (ctx);
        fbPeak.process (ctx);
        fbLowPass1.process (ctx);
        fbLowPass2.process (ctx);
    }
    else
    {
        processIRs (block);
    }

    updateMicFilters();
    if (! micActive) return;
    const auto nm = block.getNumSamples();
    const auto chm = juce::jmin (block.getNumChannels(), (size_t) 2);
    for (size_t c = 0; c < chm; ++c)
    {
        auto* d = block.getChannelPointer (c);
        for (size_t i = 0; i < nm; ++i)
        {
            float x = micC[c].process (micB[c].process (micA[c].process (d[i])));
            x = proxF[c].process (posF[c].process (x));
            if (bounceGain > 0.f)
            {
                const float refl = bounceLp[c].process (bounce[c].tap (bounceDelay));
                bounce[c].push (x);
                x += bounceGain * refl;
            }
            d[i] = x;
        }
    }
}

void CabinetStage::processIRs (juce::dsp::AudioBlock<float>& block)
{
    const bool a = loaded[0].load();
    const bool b = loaded[1].load();

    const float targetA = a ? (b ? 1.0f - blend : 1.0f) : 0.0f;
    const float targetB = b ? (a ? blend : 1.0f) : 0.0f;
    gain[0].setTargetValue (targetA);
    gain[1].setTargetValue (targetB);

    const auto n     = block.getNumSamples();
    const auto numCh = juce::jmin (block.getNumChannels(), (size_t) scratch.getNumChannels());

    auto scratchBlock = juce::dsp::AudioBlock<float> (scratch).getSubsetChannelBlock (0, numCh).getSubBlock (0, n);
    auto mainBlock    = block.getSubsetChannelBlock (0, numCh);

    // B path works on a copy of the dry input
    if (b)
    {
        scratchBlock.copyFrom (mainBlock);
        juce::dsp::ProcessContextReplacing<float> ctxB (scratchBlock);
        conv[1].process (ctxB);
    }
    else
    {
        scratchBlock.clear();
    }

    if (a)
    {
        juce::dsp::ProcessContextReplacing<float> ctxA (mainBlock);
        conv[0].process (ctxA);
    }
    else
    {
        mainBlock.clear();
    }

    for (size_t i = 0; i < n; ++i)
    {
        const float gA = gain[0].getNextValue();
        const float gB = gain[1].getNextValue();
        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* out = mainBlock.getChannelPointer (ch);
            out[i] = out[i] * gA + scratchBlock.getChannelPointer (ch)[i] * gB;
        }
    }
}
