#include "ConsoleSaturation.h"

namespace
{
    constexpr float shelfHz = 110.0f;
    constexpr float shelfDb = 4.0f;
}

void ConsoleSaturation::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) spec.numChannels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) spec.maximumBlockSize);

    // RBJ shelves with +G and -G are exact inverses -> linear when not driven.
    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    *preShelf.state  = AC::makeLowShelf (sampleRate, shelfHz, 0.7f, juce::Decibels::decibelsToGain (shelfDb));
    *postShelf.state = AC::makeLowShelf (sampleRate, shelfHz, 0.7f, juce::Decibels::decibelsToGain (-shelfDb));
    preShelf.prepare (spec);
    postShelf.prepare (spec);

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

void ConsoleSaturation::reset()
{
    if (oversampling) oversampling->reset();
    preShelf.reset();
    postShelf.reset();
    for (auto& f : hfSoften) f.reset();
    for (auto& d : dc) d.reset();
}

void ConsoleSaturation::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh = juce::jmin (block.getNumChannels(), dc.size());
    const auto n     = block.getNumSamples();

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    preShelf.process (ctx);

    auto up = oversampling->processSamplesUp (block);
    const auto nUp = up.getNumSamples();

    for (size_t i = 0; i < nUp; ++i)
    {
        const float a     = amount.getNextValue();
        const float drive = 1.0f + 0.7f * a;   // console headroom is big: grit, not fuzz
        const float bias  = 0.12f * a;
        const float tb    = std::tanh (bias);
        driveRamp[i] = drive;
        biasRamp[i]  = bias;
        // unity small-signal gain: d/dx tanh(dx+b)/d = 1 - tanh^2(b)
        normRamp[i]  = 1.0f / (drive * (1.0f - tb * tb));
        // wet amount: fades the shaper in over the first quarter of the knob,
        // so 0 is exactly linear (always running -> constant latency)
        wetRamp[i]   = juce::jmin (1.0f, a * 4.0f);
    }

    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        auto* d = up.getChannelPointer (ch);
        for (size_t i = 0; i < nUp; ++i)
        {
            const float b = biasRamp[i];
            const float x = d[i];
            const float y = (std::tanh (x * driveRamp[i] + b) - std::tanh (b)) * normRamp[i];
            d[i] = x + wetRamp[i] * (y - x);
        }
    }

    oversampling->processSamplesDown (block);
    postShelf.process (ctx);

    // A touch of top-end softening, more as it is pushed (20 kHz -> 15 kHz)
    const float a = amount.getCurrentValue();
    if (std::abs (a - lastHfAmount) > 0.005f)
    {
        lastHfAmount = a;
        for (auto& f : hfSoften) f.setCutoff (sampleRate, 20000.0f - 5000.0f * a);
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
