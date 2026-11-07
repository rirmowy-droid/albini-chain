#pragma once

#include "DSPUtils.h"
#include <vector>

/** 2" 16/24-track at 30 ips, printed hot.

    - soft, symmetric tape compression (no hard clip)
    - head bump around 70 Hz with the small dip above it
    - high-frequency loss that grows with the Tape amount
    - very subtle wow & flutter (same modulation on all tracks, like real tape)

    The transport delay always runs (integer base delay), so latency is
    constant and amount = 0 is bit-transparent apart from that delay. */
class TapeEmulation
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    void setAmount (float amount01) { amount.setTargetValue (juce::jlimit (0.0f, 1.0f, amount01)); }
    int getLatencyInSamples() const { return baseDelay; }

private:
    void updateFilters (float a);

    double sampleRate = 44100.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> transport;
    int baseDelay = 32;

    albini::StereoIIR lowCut, headBump, bumpDip, hfLoss;
    float lastFilterAmount = -1.0f;
    bool filtersActive = false;

    juce::SmoothedValue<float> amount { 0.2f };

    double wowPhase = 0.0, flutterPhase = 0.0;
    float driftValue = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    juce::Random random { 0x5A1B };

    std::vector<float> modRamp, driveRamp, wetRamp;
};
