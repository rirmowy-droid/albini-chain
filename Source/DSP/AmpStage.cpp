#include "AmpStage.h"

namespace
{
    constexpr size_t osFactorLog2 = 2;   // 4x
    constexpr float maxDriveDb   = 30.0f;
}

void AmpStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.05);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) spec.numChannels, osFactorLog2,
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

    const auto osBlock = (size_t) spec.maximumBlockSize << osFactorLog2;
    gainRamp.assign (osBlock, 1.0f);
    biasRamp.assign (osBlock, 0.0f);
    compRamp.assign (osBlock, 1.0f);

    driveSmoothed.reset (sampleRate * (double) (1 << osFactorLog2), 0.05);
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
}

void AmpStage::setGain (float gainDb)  { inputGain.setGainDecibels (gainDb); }
void AmpStage::setDrive (float drive)  { driveSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, drive)); }

float AmpStage::getLatencyInSamples() const
{
    return oversampling ? oversampling->getLatencyInSamples() : 0.0f;
}

void AmpStage::updateBrightShelf (float drive)
{
    if (std::abs (drive - lastShelfDrive) < 0.01f)
        return;

    lastShelfDrive = drive;
    // More treble into the clipper as drive goes up: keeps the crunch
    // articulate instead of turning to mush.
    const float gainDb = 1.0f + 5.0f * drive;
    *brightShelf.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        sampleRate, 1800.0f, 0.6f, juce::Decibels::decibelsToGain (gainDb));
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

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    inputGain.process (ctx);

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        for (size_t i = 0; i < n; ++i)
            d[i] = couplingHP[ch].highpass (d[i]);
    }

    updateBrightShelf (driveSmoothed.getTargetValue());
    brightShelf.process (ctx);

    // --- nonlinearity at 4x --------------------------------------------------
    auto up = oversampling->processSamplesUp (block);
    const auto nUp = up.getNumSamples();

    for (size_t i = 0; i < nUp; ++i)
    {
        const float drive = driveSmoothed.getNextValue();
        const float g     = juce::Decibels::decibelsToGain (drive * maxDriveDb);
        gainRamp[i] = g;
        biasRamp[i] = 0.1f + 0.2f * drive;          // asymmetry -> even harmonics
        compRamp[i] = 1.0f / std::sqrt (g);         // rough level compensation
    }

    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        auto* d = up.getChannelPointer (ch);
        for (size_t i = 0; i < nUp; ++i)
        {
            const float b = biasRamp[i];
            d[i] = (std::tanh (d[i] * gainRamp[i] + b) - std::tanh (b)) * compRamp[i];
        }
    }

    oversampling->processSamplesDown (block);

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        for (size_t i = 0; i < n; ++i)
            d[i] = postLP[ch].lowpass (dc[ch].process (d[i]));
    }
}
