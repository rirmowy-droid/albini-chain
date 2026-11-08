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

    /** 0 ogólna, 1 w stylu Studera A827, 2 w stylu MCI JH-24, 3 w stylu Ampexa MM1200. */
    enum Model { Generic = 0, Studer, MCI, Ampex, NumModels };
    void setModel (int m)         { model = juce::jlimit (0, (int) NumModels - 1, m); }
    /** false = 30 cali/s, true = 15 cali/s (niższy i większy head bump, mniej góry, więcej nasycenia). */
    void setSlowSpeed (bool slow) { slowSpeed = slow; }
    int getLatencyInSamples() const { return baseDelay; }

private:
    void updateFilters (float a);

    struct ModelDef { float bumpHz, bumpDb, bumpQ, dipHz, dipDb, hfLoss, drive0, driveA, asym, wowMs, flutterMs, flutterHz; };
    static const ModelDef& def (int m);
    int model = Generic, appliedModel = -1;
    bool slowSpeed = false, appliedSlow = false;

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
