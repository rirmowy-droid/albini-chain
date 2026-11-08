#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"
#include <vector>

/** Ochrona uszu ze Stonogi, przeniesiona na sam koniec łańcucha (po taśmie):
    dynamiczne przyciemnianie syczącej góry, powolne ściszanie długo za głośnego
    dźwięku i limiter szczytowy z sufitem -1 dBFS. */
class EarGuard
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = (float) sampleRate;
        ch.assign ((size_t) juce::jmax (1, numChannels), {});
        for (auto& c : ch)
        {
            c.hfHp.highpass (sr, 3500.f, 0.7f);
            c.shelf.highshelf (sr, 2800.f, 0.f);
        }
    }

    void process (juce::dsp::AudioBlock<float>& block, float amount)
    {
        const auto numCh = juce::jmin (block.getNumChannels(), ch.size());
        for (size_t c = 0; c < numCh; ++c)
        {
            auto* d = block.getChannelPointer (c);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                d[i] = tick (ch[c], d[i], amount);
        }
    }

private:
    struct Chan
    {
        stonoga::Biquad hfHp, shelf;
        float msAll = 0, msHf = 0, cutDb = 0, gain = 1.f, limEnv = 0;
        unsigned counter = 0;
    };

    float tick (Chan& s, float o, float prot)
    {
        if (! std::isfinite (o)) o = 0.f;

        const float a300 = 1.f / (0.3f * sr), a50 = 1.f / (0.05f * sr);
        s.msAll += (o * o - s.msAll) * a300;
        const float hf = s.hfHp.process (o);
        s.msHf += (hf * hf - s.msHf) * a50;

        if ((s.counter++ & 15) == 0)
        {
            const float rms = std::sqrt (s.msAll) + 1e-6f, hfr = std::sqrt (s.msHf);
            const float ratio = hfr / rms;
            float tgt = prot * (stonoga::clampf ((ratio - 0.35f) / 0.3f, 0.f, 1.f) * 18.f
                              + stonoga::clampf ((hfr - 0.07f) / 0.15f, 0.f, 1.f) * 12.f);
            tgt = std::min (tgt, 24.f);
            const float dt = 16.f / sr;
            s.cutDb += (tgt - s.cutDb) * (tgt > s.cutDb ? dt / 0.08f : dt / 0.8f);
            s.shelf.highshelf (sr, 2800.f, -s.cutDb);

            const float thr = 0.35f;
            float gT = rms > thr ? std::pow (thr / rms, 0.8f) : 1.f;
            gT = 1.f - prot * (1.f - gT);
            s.gain += (gT - s.gain) * (gT < s.gain ? dt / 1.2f : dt / 2.f);
        }

        float y = s.shelf.process (o) * s.gain;
        if (! ownLimiter) return y;            // szczyty łapie limiter z wyprzedzeniem za nami

        const float ceil = 0.89f, a = std::fabs (y);
        s.limEnv = a > s.limEnv ? a : s.limEnv + (a - s.limEnv) * (1.f / (0.12f * sr));
        if (s.limEnv > ceil) y *= ceil / s.limEnv;
        return ceil * std::tanh (y / ceil);
    }

public:
    bool ownLimiter = true;
private:
    float sr = 48000.f;
    std::vector<Chan> ch;
};
