#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Krótkie, „prawie podwójne” echo (slap) jak na wokalach z początku lat 90.:
    dwa odbicia (~85 i ~170 ms), z obciętym dołem i górą, lekko rozsunięte w stereo. */
class SlapEcho
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : line) d.init ((size_t) (0.25f * sr) + 8);
        for (auto& f : hp) f.highpass (sr, 300.f, 0.7f);
        for (auto& f : lp) f.lowpass (sr, 5000.f, 0.7f);
        amount.reset (sampleRate, 0.05);
        reset();
    }
    void reset() { for (auto& d : line) d.clear(); for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); }
    void setAmount (float a) { amount.setTargetValue (stonoga::clampf (a, 0.f, 1.f)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! amount.isSmoothing() && amount.getTargetValue() <= 0.0001f) return;
        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        const float t1[2] = { 0.083f * sr, 0.089f * sr }, t2[2] = { 0.168f * sr, 0.176f * sr };
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float a = amount.getNextValue();
            for (size_t c = 0; c < numCh; ++c)
            {
                auto* d = block.getChannelPointer (c);
                auto& ln = line[c];
                const float e = 0.55f * ln.tap (t1[c]) + 0.3f * ln.tap (t2[c]);
                ln.push (lp[c].process (hp[c].process (d[i])));
                d[i] += 0.6f * a * e;
            }
        }
    }

private:
    float sr = 48000.f;
    stonoga::Delay line[2];
    stonoga::Biquad hp[2], lp[2];
    juce::SmoothedValue<float> amount { 0.f };
};
