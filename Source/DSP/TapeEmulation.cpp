#include "TapeEmulation.h"

namespace
{
    constexpr double wowHz      = 0.55;
    constexpr float  driftMs    = 0.05f;
    constexpr float  baseDelayMs = 0.6f;   // must exceed total modulation depth
}

const TapeEmulation::ModelDef& TapeEmulation::def (int m)
{
    //                  bumpHz bumpDb Q    dipHz dipDb  hfLoss  drive0 driveA asym   wowMs  flutMs  flutHz
    static const ModelDef defs[NumModels] = {
        { 70.f, 2.8f, 0.9f, 170.f, -0.9f, 7500.f, 0.40f, 0.8f, 0.00f, 0.10f, 0.008f, 6.3f },   // ogólna
        { 60.f, 2.2f, 1.0f, 140.f, -1.0f, 5500.f, 0.40f, 0.7f, 0.00f, 0.06f, 0.005f, 5.8f },   // Studer: czysto, zwarty dół, otwarta góra
        { 80.f, 3.5f, 0.9f, 200.f, -1.2f, 8500.f, 0.45f, 1.0f, 0.08f, 0.10f, 0.012f, 7.0f },   // MCI: uderzenie, więcej brudu
        { 55.f, 3.2f, 0.8f, 120.f, -0.8f, 10000.f, 0.40f, 0.9f, 0.05f, 0.09f, 0.009f, 6.0f },  // Ampex: ciepło, miękka góra
    };
    return defs[juce::jlimit (0, (int) NumModels - 1, m)];
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
    if (std::abs (a - lastFilterAmount) < 0.005f && appliedModel == model && appliedSlow == slowSpeed)
        return;
    lastFilterAmount = a;
    appliedModel = model; appliedSlow = slowSpeed;
    const auto& md = def (model);
    // 15 cali/s: head bump niżej i mocniej, więcej strat góry
    const float bumpHz = slowSpeed ? md.bumpHz * 0.62f : md.bumpHz;
    const float bumpDb = slowSpeed ? md.bumpDb * 1.35f : md.bumpDb;
    const float dipHz  = slowSpeed ? md.dipHz * 0.65f : md.dipHz;
    const float hfLossHz = slowSpeed ? md.hfLoss + 3000.f : md.hfLoss;

    using AC = juce::dsp::IIR::ArrayCoefficients<float>;
    const auto db = [] (float d) { return juce::Decibels::decibelsToGain (d); };

    const float nyq = (float) sampleRate * 0.45f;
    const float hf  = juce::jmin (nyq, 20000.0f - hfLossHz * a);

    *lowCut.state   = AC::makeHighPass  (sampleRate, 18.0f, 0.7f);
    *headBump.state = AC::makePeakFilter (sampleRate, bumpHz, md.bumpQ, db (bumpDb * a));
    *bumpDip.state  = AC::makePeakFilter (sampleRate, dipHz, 1.2f, db (md.dipDb * a));
    *hfLoss.state   = AC::makeLowPass   (sampleRate, hf, 0.62f);
}

void TapeEmulation::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh = block.getNumChannels();
    const auto n     = block.getNumSamples();

    // --- shared transport modulation + saturation ramps ----------------------
    const auto& md = def (model);
    const double wowInc     = juce::MathConstants<double>::twoPi * wowHz / sampleRate;
    const double flutterInc = juce::MathConstants<double>::twoPi * (double) md.flutterHz / sampleRate;
    const float wowDepth = md.wowMs * (slowSpeed ? 1.6f : 1.f), flutterDepth = md.flutterMs * (slowSpeed ? 1.6f : 1.f);
    const float drive0 = md.drive0, driveA = md.driveA * (slowSpeed ? 1.25f : 1.f), asym = md.asym;
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

        const float mod = (float) std::sin (wowPhase) * wowDepth
                        + (float) std::sin (flutterPhase) * flutterDepth
                        + driftValue * driftMs;

        wowPhase     += wowInc;     if (wowPhase     > juce::MathConstants<double>::twoPi) wowPhase     -= juce::MathConstants<double>::twoPi;
        flutterPhase += flutterInc; if (flutterPhase > juce::MathConstants<double>::twoPi) flutterPhase -= juce::MathConstants<double>::twoPi;

        modRamp[i]   = (float) baseDelay + a * mod * msToS;
        driveRamp[i] = drive0 + driveA * a;     // <1 = więcej zapasu niż jedność
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
                // asymetria (parzyste harmoniczne) - stałą składową usuwa filtr 18 Hz
                const float xg = x * g;
                const float u = xg + asym * 0.5f * xg * xg;
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
