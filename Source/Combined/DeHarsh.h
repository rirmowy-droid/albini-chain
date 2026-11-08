#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "../Stonoga/StonogaDSP.h"

/** Wygładzanie góry: dynamiczny korektor w 6 pasmach (2,5 - 14 kHz).

    W każdym paśmie porównuje szybką obwiednię z wolną: gdy pasmo nagle wyskakuje
    ponad swój zwykły poziom (syczące „s”, ostry talerz, kłujący atak gitary),
    robi w nim chwilowy dołek. Gdy góra jest spokojna - nic nie robi, więc nie
    przyciemnia stale jak zwykły filtr. Sterowanie wspólne dla L i R. */
class DeHarsh
{
public:
    static constexpr int numBands = 6;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        static const float hz[numBands] = { 2500.f, 3500.f, 5000.f, 7000.f, 10000.f, 14000.f };
        for (int b = 0; b < numBands; ++b)
        {
            freq[(size_t) b] = std::min (hz[b], sr * 0.42f);
            det[(size_t) b].bandpass (sr, freq[(size_t) b], 2.0f);
            for (auto& ch : cut) ch[(size_t) b].peak (sr, freq[(size_t) b], 1.6f, 0.f);
            fast[(size_t) b] = slow[(size_t) b] = 0.f;
            gainDb[(size_t) b] = 0.f;
        }
        fastAtk = 1.f - std::exp (-1.f / (0.003f * sr));
        fastRel = 1.f - std::exp (-1.f / (0.05f * sr));
        slowC   = 1.f - std::exp (-1.f / (0.5f * sr));
        counter = 0;
    }

    void process (juce::dsp::AudioBlock<float>& block, float amount)
    {
        if (amount <= 0.001f && maxCutNow <= 0.05f)
        {
            meter.store (0.f);
            return;
        }
        const float thr = 6.f - 4.f * amount;          // dB ponad zwykły poziom pasma
        const float maxCut = 3.f + 7.f * amount;
        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        auto* L = block.getChannelPointer (0);
        auto* R = numCh > 1 ? block.getChannelPointer (1) : nullptr;
        float blockMax = 0.f;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float m = R ? 0.5f * (L[i] + R[i]) : L[i];
            for (int b = 0; b < numBands; ++b)
            {
                const float a = std::fabs (det[(size_t) b].process (m));
                fast[(size_t) b] += (a > fast[(size_t) b] ? fastAtk : fastRel) * (a - fast[(size_t) b]);
                slow[(size_t) b] += slowC * (a - slow[(size_t) b]);
            }

            if ((counter++ & 63) == 0)   // co 64 próbki przelicz dołki
            {
                maxCutNow = 0.f;
                for (int b = 0; b < numBands; ++b)
                {
                    const float excess = 20.f * std::log10 ((fast[(size_t) b] + 1e-7f) / (slow[(size_t) b] + 1e-7f)) - thr;
                    const float target = amount > 0.001f ? -std::min (maxCut, std::max (0.f, excess * 0.7f)) : 0.f;
                    float& g = gainDb[(size_t) b];
                    g += (target < g ? 0.6f : 0.15f) * (target - g);       // szybko w dół, łagodnie w górę
                    if (std::abs (g - appliedDb[(size_t) b]) > 0.1f)
                    {
                        appliedDb[(size_t) b] = g;
                        for (auto& ch : cut) ch[(size_t) b].peak (sr, freq[(size_t) b], 1.6f, g);
                    }
                    maxCutNow = std::max (maxCutNow, -g);
                }
                blockMax = std::max (blockMax, maxCutNow);
            }

            for (int b = 0; b < numBands; ++b)
            {
                if (appliedDb[(size_t) b] > -0.05f) continue;
                L[i] = cut[0][(size_t) b].process (L[i]);
                if (R) R[i] = cut[1][(size_t) b].process (R[i]);
            }
        }
        meter.store (-blockMax);
    }

    float getReductionDb() const { return meter.load(); }

private:
    float sr = 48000.f, fastAtk = 0.1f, fastRel = 0.01f, slowC = 0.001f, maxCutNow = 0.f;
    std::array<float, numBands> freq {}, fast {}, slow {}, gainDb {}, appliedDb {};
    std::array<stonoga::Biquad, numBands> det;
    std::array<std::array<stonoga::Biquad, numBands>, 2> cut;
    unsigned counter = 0;
    std::atomic<float> meter { 0.f };
};
