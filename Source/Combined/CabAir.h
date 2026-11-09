#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Powietrze kolumny: kilka wczesnych odbić od podłogi i ścian małego pokoju (2-21 ms),
    inne w lewym i prawym kanale. Kolumna przestaje brzmieć jak nagrana w próżni,
    a dźwięk zostaje blisko - to nie pogłos, tylko „pokój wokół pieca”. */
class CabAir
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : line) { d.init ((size_t) (0.03f * sr) + 4); d.clear(); }
        for (int c = 0; c < 2; ++c) { lp[c].lowpass (sr, 4500.f, 0.6f); hp[c].highpass (sr, 150.f, 0.6f); }
    }

    void process (juce::dsp::AudioBlock<float>& block, float amount)
    {
        if (amount < 0.005f) return;
        static const float tapsMs[2][6] = { { 2.3f, 4.1f, 6.8f, 9.6f, 13.4f, 18.9f },
                                            { 2.9f, 5.0f, 7.7f, 11.1f, 15.2f, 21.0f } };
        static const float gains[6] = { 0.55f, -0.45f, 0.38f, -0.30f, 0.24f, -0.18f };
        const auto n = block.getNumSamples();
        const int nCh = (int) std::min<size_t> (block.getNumChannels(), 2);
        const float mix = 0.7f * amount;
        for (int c = 0; c < nCh; ++c)
        {
            auto* d = block.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < n; ++i)
            {
                line[c].push (d[i]);
                float er = 0.f;
                for (int k = 0; k < 6; ++k) er += gains[k] * line[c].tap (tapsMs[c][k] * 0.001f * sr);
                er = hp[c].process (lp[c].process (er));
                d[i] = d[i] * (1.f - 0.25f * amount) + mix * er;
            }
        }
    }

private:
    float sr = 48000.f;
    stonoga::Delay line[2];
    stonoga::Biquad lp[2], hp[2];
};
