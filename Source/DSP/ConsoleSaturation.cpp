#include "ConsoleSaturation.h"

const ConsoleSaturation::Voicing& ConsoleSaturation::voicing (int m)
{
    //                       shelfHz shelfDb drive bias  hard  hfMin    colHz  colQ colDb
    static const Voicing v[NumModels] = {
        { 110.f, 4.0f, 0.7f, 0.12f, 0.0f, 15000.f,  1000.f, 0.7f,  0.0f },  // ogólna
        {  90.f, 5.5f, 0.9f, 0.20f, 0.0f, 14000.f,   160.f, 0.8f,  0.8f },  // brytyjska
        { 150.f, 2.0f, 1.1f, 0.06f, 0.7f, 17000.f,  2500.f, 0.9f,  0.9f },  // amerykańska
        { 110.f, 1.0f, 0.4f, 0.05f, 0.2f, 18500.f,  1000.f, 0.7f,  0.0f },  // czysta in-line
    };
    return v[juce::jlimit (0, (int) NumModels - 1, m)];
}

void ConsoleSaturation::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) spec.numChannels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) spec.maximumBlockSize);

    preShelf.prepare (spec);
    postShelf.prepare (spec);
    colour.prepare (spec);
    appliedModel = -1;
    lastColourAmount = -1.0f;
    applyModel();

    hfSoften.assign (spec.numChannels, {});
    dc.assign (spec.numChannels, {});
    for (auto& d : dc) d.prepare (sampleRate, 8.0f);
    lastHfAmount = -1.0f;

    const auto osBlock = (size_t) spec.maximumBlockSize * 2;
    driveRamp.assign (osBlock, 1.0f);
    biasRamp.assign (osBlock, 0.0f);
    normRamp.assign (osBlock, 1.0f);
    wetRamp.assign (osBlock, 0.0f);

    amount.reset (sampleRate * 2.0, 0.05);
    reset();
}

void ConsoleSaturation::applyModel()
{
    if (appliedModel == model)
        return;
    appliedModel = model;

    // RBJ shelves with +G and -G are exact inverses -> linear when not driven.
    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    const auto& v = voicing (model);
    *preShelf.state  = AC::makeLowShelf (sampleRate, v.shelfHz, 0.7f, juce::Decibels::decibelsToGain (v.shelfDb));
    *postShelf.state = AC::makeLowShelf (sampleRate, v.shelfHz, 0.7f, juce::Decibels::decibelsToGain (-v.shelfDb));
    lastColourAmount = -1.0f;
    lastHfAmount = -1.0f;
}

void ConsoleSaturation::reset()
{
    if (oversampling) oversampling->reset();
    preShelf.reset();
    postShelf.reset();
    colour.reset();
    for (auto& f : hfSoften) f.reset();
    for (auto& d : dc) d.reset();
}

void ConsoleSaturation::process (juce::dsp::AudioBlock<float>& block)
{
    applyModel();
    const auto& v = voicing (model);
    const auto numCh = juce::jmin (block.getNumChannels(), dc.size());
    const auto n     = block.getNumSamples();

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    preShelf.process (ctx);

    auto up = oversampling->processSamplesUp (block);
    const auto nUp = up.getNumSamples();

    for (size_t i = 0; i < nUp; ++i)
    {
        const float a     = amount.getNextValue();
        const float drive = 1.0f + v.driveMax * a;
        const float bias  = v.biasMax * a;
        const float tb    = std::tanh (bias);
        driveRamp[i] = drive;
        biasRamp[i]  = bias;
        normRamp[i]  = 1.0f / (drive * (1.0f - tb * tb));   // jednostkowe wzmocnienie małych sygnałów
        wetRamp[i]   = juce::jmin (1.0f, a * 4.0f);           // 0 = dokładnie liniowo
    }

    const float hard = v.hardness;
    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        auto* d = up.getChannelPointer (ch);
        for (size_t i = 0; i < nUp; ++i)
        {
            const float x = d[i];
            const float b = biasRamp[i];
            const float u = x * driveRamp[i] + b;
            float shaped = std::tanh (u);
            if (hard > 0.0f)
            {
                // twardsze kolano: u / (1 + |u|^4)^(1/4), nachylenie 1 w zerze jak tanh
                const float u2 = u * u;
                const float hardShape = u / std::sqrt (std::sqrt (1.0f + u2 * u2));
                shaped += hard * (hardShape - shaped);
            }
            const float bShaped = hard > 0.0f ? std::tanh (b) + hard * (b / std::sqrt (std::sqrt (1.0f + b * b * b * b)) - std::tanh (b)) : std::tanh (b);
            const float y = (shaped - bShaped) * normRamp[i];
            d[i] = x + wetRamp[i] * (y - x);
        }
    }

    oversampling->processSamplesDown (block);
    postShelf.process (ctx);

    // stały charakter modelu (pogrubiony niski środek / wypchnięta średnica)
    const float a = amount.getCurrentValue();
    if (std::abs (v.colourDb) > 0.01f)
    {
        if (std::abs (a - lastColourAmount) > 0.01f)
        {
            lastColourAmount = a;
            using AC = juce::dsp::IIR::ArrayCoefficients<float>;
            *colour.state = AC::makePeakFilter (sampleRate, v.colourHz, v.colourQ,
                                                juce::Decibels::decibelsToGain (v.colourDb * juce::jmin (1.0f, a * 2.0f)));
        }
        if (a > 0.001f) colour.process (ctx);
    }

    // trochę zaokrąglonej góry, bardziej przy mocnym nasyceniu
    if (std::abs (a - lastHfAmount) > 0.005f)
    {
        lastHfAmount = a;
        for (auto& f : hfSoften) f.setCutoff (sampleRate, 20000.0f - (20000.0f - v.hfMinHz) * a);
    }

    const bool soften = a > 0.001f;
    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        for (size_t i = 0; i < n; ++i)
        {
            const float lp = hfSoften[ch].lowpass (d[i]); // keep state warm
            d[i] = dc[ch].process (soften ? lp : d[i]);
        }
    }
}
