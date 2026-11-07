#pragma once

#include "DSPUtils.h"
#include <vector>
#include <memory>

/** Preamp/power-amp stage: tight low end, a bit of "bright cap" before the
    clipper, asymmetric soft clipping at 4x oversampling.
    Not a modern high-gain amp: it goes from clean to crunchy, with a natural
    roll-off from the clipping rather than a scooped tone stack. */
class AmpStage
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    void setGain (float gainDb);
    void setDrive (float drive); // 0..1
    /** true: sygnał przechodzi tylko przez oversampling (ta sama latencja, bez wzmacniacza). */
    void setPassThrough (bool p) { passThrough = p; }

    float getLatencyInSamples() const;

private:
    void updateBrightShelf (float drive);

    double sampleRate = 44100.0;
    bool passThrough = false;

    juce::dsp::Gain<float> inputGain;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    albini::StereoIIR brightShelf;
    float lastShelfDrive = -1.0f;

    std::vector<albini::OnePole> couplingHP, postLP;
    std::vector<albini::DCBlocker> dc;

    juce::SmoothedValue<float> driveSmoothed { 0.4f };
    std::vector<float> gainRamp, biasRamp, compRamp;
};
