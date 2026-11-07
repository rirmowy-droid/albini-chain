#include "CabinetStage.h"

using Coeffs = juce::dsp::IIR::Coefficients<float>;

CabinetStage::CabinetStage() = default;

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

    for (auto* f : { &fbHighPass, &fbPeak, &fbLowPass1, &fbLowPass2 })
        f->prepare (spec);

    reset();
}

void CabinetStage::reset()
{
    for (auto& c : conv) c.reset();
    for (auto* f : { &fbHighPass, &fbPeak, &fbLowPass1, &fbLowPass2 })
        f->reset();
}

bool CabinetStage::loadIR (int slot, const juce::File& file)
{
    slot &= 1;
    if (! file.existsAsFile())
        return false;

    // Trim::no keeps the original pre-delay of each IR, so two mics captured
    // in the same session stay time-aligned when blended.
    conv[slot].loadImpulseResponse (file,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::no,
                                    0,
                                    juce::dsp::Convolution::Normalise::yes);
    loaded[slot] = true;
    return true;
}

void CabinetStage::clearIR (int slot)
{
    loaded[slot & 1] = false;
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
        fbHighPass.process (ctx);
        fbPeak.process (ctx);
        fbLowPass1.process (ctx);
        fbLowPass2.process (ctx);
        return;
    }

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
