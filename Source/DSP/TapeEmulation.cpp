#include "TapeEmulation.h"

namespace
{
    constexpr double wowHz      = 0.55;
    constexpr double flutterHz  = 6.3;
    constexpr float  wowMs      = 0.10f;   // ~0.035 % peak speed deviation
    constexpr float  flutterMs  = 0.008f;  // ~0.03 %
    constexpr float  driftMs    = 0.05f;
    constexpr float  baseDelayMs = 0.6f;   // must exceed total modulation depth
}

void TapeEmulation::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    baseDelay = (int) std::ceil (baseDelayMs * 0.001 * sampleRate);
    transport.prepare (spec);
    transport.setMaximumDelayInSamples (baseDelay * 2 + 8);

    lowCut.prepare (spec);
    headBump.prepare (spec);
    bumpDip.prepare (spec);
    hfLoss.prepare (spec);
    lastFilterAmount = -1.0f;
    updateFilters (amount.getTargetValue());

    modRamp.assign (spec.maximumBlockSize, 0.0f);
    driveRamp.assign (spec.maximumBlockSize, 1.0f);
    wetRamp.assign (spec.maximumBlockSize, 0.0f);

    amount.reset (sampleRate, 0.05);
    reset();
}

void TapeEmulation::reset()
{
    transport.reset();
    lowCut.reset();
    headBump.reset();
    bumpDip.reset();
    hfLoss.reset();
    wowPhase = flutterPhase = 0.0;
    driftValue = driftTarget = 0.0f;
    driftCounter = 0;
}

void TapeEmulation::updateFilters (float a)
{
    if (std::abs (a - lastFilterAmount) < 0.005f)
        return;
    lastFilterAmount = a;

    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    const auto db = [] (float d) { return juce::Decibels::decibelsToGain (d); };

    const float nyq = (float) sampleRate * 0.45f;
    const float hf  = juce::jmin (nyq, 20000.0f - 7500.0f * a);

    *lowCut.state   = AC::makeHighPass  (sampleRate, 18.0f, 0.7f);
    *headBump.state = AC::makePeakFilter (sampleRate, 70.0f, 0.9f, db (2.8f * a));
    *bumpDip.state  = AC::makePeakFilter (sampleRate, 170.0f, 1.2f, db (-0.9f * a));
    *hfLoss.state   = AC::makeLowPass   (sampleRate, hf, 0.62f);
}

void TapeEmulation::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh = block.getNumChannels();
    const auto n     = block.getNumSamples();

    // --- shared transport modulation + saturation ramps ----------------------
    const double wowInc     = juce::MathConstants<double>::twoPi * wowHz / sampleRate;
    const double flutterInc = juce::MathConstants<double>::twoPi * flutterHz / sampleRate;
    const float  msToS      = (float) (0.001 * sampleRate);
    const int    driftPeriod = juce::jmax (1, (int) (sampleRate * 0.25));

    for (size_t i = 0; i < n; ++i)
    {
        const float a = amount.getNextValue();

        if (--driftCounter <= 0)
        {
            driftCounter = driftPeriod;
            driftTarget  = random.nextFloat() * 2.0f - 1.0f;
        }
        driftValue += (driftTarget - driftValue) * 0.00005f;

        const float mod = (float) std::sin (wowPhase) * wowMs
                        + (float) std::sin (flutterPhase) * flutterMs
                        + driftValue * driftMs;

        wowPhase     += wowInc;     if (wowPhase     > juce::MathConstants<double>::twoPi) wowPhase     -= juce::MathConstants<double>::twoPi;
        flutterPhase += flutterInc; if (flutterPhase > juce::MathConstants<double>::twoPi) flutterPhase -= juce::MathConstants<double>::twoPi;

        modRamp[i]   = (float) baseDelay + a * mod * msToS;
        driveRamp[i] = 0.4f + 0.8f * a;     // <1 = more headroom than unity
        wetRamp[i]   = juce::jmin (1.0f, a * 4.0f);
    }

    const bool active = amount.isSmoothing() || amount.getTargetValue() > 0.0001f;

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        for (size_t i = 0; i < n; ++i)
        {
            transport.pushSample ((int) ch, d[i]);
            float x = transport.popSample ((int) ch, modRamp[i], true);

            if (active)
            {
                // x / sqrt(1 + x^2): very soft knee, unity small-signal gain
                const float g = driveRamp[i];
                const float u = x * g;
                const float y = u / std::sqrt (1.0f + u * u) / g;
                x += wetRamp[i] * (y - x);
            }
            d[i] = x;
        }
    }

    if (! active)
    {
        filtersActive = false;
        return;
    }

    if (! filtersActive)
    {
        lowCut.reset(); headBump.reset(); bumpDip.reset(); hfLoss.reset();
        filtersActive = true;
    }

    updateFilters (amount.getCurrentValue());
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    lowCut.process (ctx);
    headBump.process (ctx);
    bumpDip.process (ctx);
    hfLoss.process (ctx);
}
