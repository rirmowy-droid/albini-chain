#pragma once

#include "DSPUtils.h"
#include <atomic>

/** Two cabinet IR slots (e.g. SM57 close + ribbon/condenser) with a blend.

    - Both IRs loaded: linear crossfade A -> B (like summing two mic faders;
      phase relationships between the IRs are kept, so the blend comb-filters
      exactly the way two real mics would).
    - One IR loaded: that IR only.
    - None loaded: a simple 4x12-ish speaker curve so the amp never sounds fizzy.

    loadIR()/clearIR() must be called from a non-audio thread (UI / state restore). */
class CabinetStage
{
public:
    CabinetStage();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    bool loadIR (int slot, const juce::File& file);
    void clearIR (int slot);
    bool hasIR (int slot) const { return loaded[slot & 1].load(); }

    void setBlend (float blend01) { blend = juce::jlimit (0.0f, 1.0f, blend01); }

private:
    juce::dsp::Convolution conv[2];
    std::atomic<bool> loaded[2] { false, false };
    bool wasLoaded[2] { false, false };

    float blend = 0.5f;
    juce::SmoothedValue<float> gain[2];

    juce::AudioBuffer<float> scratch;

    // Fallback speaker curve
    albini::StereoIIR fbHighPass, fbPeak, fbLowPass1, fbLowPass2;

    double sampleRate = 44100.0;
};
