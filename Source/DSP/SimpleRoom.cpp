#include "SimpleRoom.h"

namespace
{
    // Base FDN line lengths in ms (mutually prime-ish), scaled by room size.
    constexpr float lineMs[4] = { 29.7f, 37.1f, 41.3f, 47.9f };

    // Early reflection taps: time (relative 0..1 of ER window) and gain, per side.
    constexpr float tapTimeL[6] = { 0.13f, 0.27f, 0.41f, 0.58f, 0.77f, 0.96f };
    constexpr float tapTimeR[6] = { 0.17f, 0.31f, 0.46f, 0.63f, 0.81f, 1.00f };
    constexpr float tapGain[6]  = { 0.80f, -0.62f, 0.50f, -0.40f, 0.31f, -0.24f };

    float sizeScale (float size)  { return 0.45f + 0.9f * size; }      // FDN length scale
    float erWindowMs (float size) { return 6.0f + 30.0f * size; }      // last reflection
    float rt60 (float size)       { return 0.22f + 1.0f * size * size + 0.25f * size; }
}

void SimpleRoom::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const auto msToSamples = [this] (float ms) { return (float) (ms * 0.001 * sampleRate); };

    er.init ((int) msToSamples (erWindowMs (1.0f) + 5.0f) + 4);
    for (int i = 0; i < numLines; ++i)
        lines[(size_t) i].init ((int) msToSamples (lineMs[i] * sizeScale (1.0f) + 2.0f) + 4);

    inputHP.setCutoff (sampleRate, 180.0f);  // room mics, not a low-end source
    inputLP.setCutoff (sampleRate, 9000.0f);
    for (auto& d : damping) d.setCutoff (sampleRate, 6500.0f);

    mixSmoothed.reset (sampleRate, 0.05);
    sizeSmoothed.reset (sampleRate, 0.25);   // slow: size moves bend pitch slightly, no clicks
    lastDecaySize = -1.0f;
    updateDecay (sizeTarget);
    reset();
}

void SimpleRoom::reset()
{
    er.clear();
    for (auto& l : lines) l.clear();
    for (auto& d : damping) d.reset();
    inputHP.reset();
    inputLP.reset();
    sizeSmoothed.setCurrentAndTargetValue (sizeTarget);
}

void SimpleRoom::updateDecay (float size)
{
    if (std::abs (size - lastDecaySize) < 0.002f)
        return;
    lastDecaySize = size;

    const float t60 = rt60 (size);
    for (int i = 0; i < numLines; ++i)
    {
        const float lenSec = lineMs[i] * sizeScale (size) * 0.001f;
        feedback[(size_t) i] = std::pow (10.0f, -3.0f * lenSec / t60);
    }

    // Small rooms are brighter, bigger rooms lose more top end.
    for (auto& d : damping)
        d.setCutoff (sampleRate, 8500.0f - 3500.0f * size);
}

void SimpleRoom::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh = block.getNumChannels();
    const auto n     = block.getNumSamples();
    if (numCh == 0)
        return;

    if (! mixSmoothed.isSmoothing() && mixSmoothed.getTargetValue() <= 0.0f)
        return; // fully dry: let the tail die without costing CPU

    updateDecay (sizeTarget);

    const float msToS = (float) (0.001 * sampleRate);
    auto* left  = block.getChannelPointer (0);
    auto* right = numCh > 1 ? block.getChannelPointer (1) : nullptr;

    for (size_t i = 0; i < n; ++i)
    {
        const float size = sizeSmoothed.getNextValue();
        const float mix  = mixSmoothed.getNextValue();

        const float dryL = left[i];
        const float dryR = right ? right[i] : dryL;

        // --- input: mono, band-limited --------------------------------------
        float in = 0.5f * (dryL + dryR);
        in = inputLP.lowpass (inputHP.highpass (in));
        er.push (in);

        // --- early reflections -----------------------------------------------
        const float erWin = erWindowMs (size) * msToS;
        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < numTaps; ++t)
        {
            erL += tapGain[t] * er.read (1.0f + tapTimeL[t] * erWin);
            erR += tapGain[t] * er.read (1.0f + tapTimeR[t] * erWin);
        }

        // --- late tail (4x4 Hadamard FDN) -------------------------------------
        const float scale = sizeScale (size);
        float o[numLines];
        for (int l = 0; l < numLines; ++l)
        {
            const float d = lineMs[l] * scale * msToS;
            o[l] = damping[(size_t) l].lowpass (lines[(size_t) l].read (d)) * feedback[(size_t) l];
        }

        const float h0 = 0.5f * ( o[0] + o[1] + o[2] + o[3]);
        const float h1 = 0.5f * ( o[0] - o[1] + o[2] - o[3]);
        const float h2 = 0.5f * ( o[0] + o[1] - o[2] - o[3]);
        const float h3 = 0.5f * ( o[0] - o[1] - o[2] + o[3]);

        // Feed the tail with input + a bit of the ER (diffusion)
        const float feed = 0.6f * in + 0.25f * (erL + erR);
        lines[0].push (feed + h0);
        lines[1].push (feed + h1);
        lines[2].push (feed + h2);
        lines[3].push (feed + h3);

        const float lateL = 0.6f * (o[0] - o[2]);
        const float lateR = 0.6f * (o[1] - o[3]);

        const float wetL = 0.55f * erL + lateL;
        const float wetR = 0.55f * erR + lateR;

        left[i] = dryL + mix * wetL;
        if (right)
            right[i] = dryR + mix * wetR;
    }

    // any extra channels just stay dry
}
