#pragma once

#include "DSPUtils.h"
#include <vector>
#include <memory>

/** Console channel + transformer colour.

    Low shelf boost -> asymmetric soft saturation (2x OS) -> exact inverse low
    shelf. The lows hit the "iron" harder than the mids, which is where the
    weight and grit of a hard-driven console come from. At 0 the stage is
    linear (the saturator is crossfaded out inside the oversampled domain). */
class ConsoleSaturation
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    void setAmount (float amount01) { amount.setTargetValue (juce::jlimit (0.0f, 1.0f, amount01)); }
    float getLatencyInSamples() const { return oversampling ? oversampling->getLatencyInSamples() : 0.0f; }

private:
    double sampleRate = 44100.0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    albini::StereoIIR preShelf, postShelf;

    std::vector<albini::OnePole> hfSoften;
    std::vector<albini::DCBlocker> dc;
    float lastHfAmount = -1.0f;

    juce::SmoothedValue<float> amount { 0.25f };
    std::vector<float> driveRamp, biasRamp, normRamp, wetRamp;
};
