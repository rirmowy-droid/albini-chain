#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace albini
{
    /** 1st-order DC blocker (~10 Hz). */
    struct DCBlocker
    {
        void prepare (double sampleRate, float cutoffHz = 10.0f)
        {
            r = std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
            reset();
        }
        void reset() { x1 = y1 = 0.0f; }

        float process (float x)
        {
            const float y = x - x1 + r * y1;
            x1 = x;
            y1 = y;
            return y;
        }

        float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
    };

    /** 1-pole lowpass / highpass, cheap enough to retune every block. */
    struct OnePole
    {
        void setCutoff (double sampleRate, float hz)
        {
            hz = juce::jlimit (1.0f, (float) sampleRate * 0.49f, hz);
            a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
        }
        void reset() { z = 0.0f; }

        float lowpass (float x)  { z += a * (x - z); return z; }
        float highpass (float x) { return x - lowpass (x); }

        float a = 1.0f, z = 0.0f;
    };

    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                     juce::dsp::IIR::Coefficients<float>>;
}
