#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <cmath>

/** Limiter z wyprzedzeniem (lookahead) na samym końcu toru.

    Sygnał jest opóźniany o ~1,5 ms, a limiter „widzi” szczyt, zanim ten nadejdzie,
    i płynnie ścisza na czas jego trwania - zamiast przycinać (co brzmi jak brud).
    Szczyty mierzone także pomiędzy próbkami (przybliżenie true-peak: interpolacja
    4x między próbkami), sufit -1 dBFS. Puszczanie zależne od programu: szybkie
    po krótkich szczytach, wolniejsze przy długim, gęstym ściskaniu (bez pompowania). */
class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = (float) sampleRate;
        look = juce::jmax (8, (int) std::round (0.0015 * sampleRate));
        nch = juce::jlimit (1, 2, numChannels);
        for (auto& d : delay) d.assign ((size_t) look + 1, 0.f);
        writePos = 0;
        qCap = look + 4;
        qIdx.assign ((size_t) qCap, 0);
        qVal.assign ((size_t) qCap, 1.f);
        qHead = qTail = 0;
        sampleIndex = 0;
        env = 1.f; held = 0.f;
        prev[0] = prev[1] = 0.f;
        relFast = std::exp (-1.f / (0.05f * sr));
        relSlow = std::exp (-1.f / (0.4f * sr));
    }

    int getLatencyInSamples() const { return look; }

    void process (juce::dsp::AudioBlock<float>& block, float ceilingDb = -1.f)
    {
        const float ceil = juce::Decibels::decibelsToGain (ceilingDb);
        const auto numCh = juce::jmin ((size_t) nch, block.getNumChannels());
        float* ch[2] = { block.getChannelPointer (0), numCh > 1 ? block.getChannelPointer (1) : nullptr };
        float minG = 1.f;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            // szczyt tej próbki, także pomiędzy próbkami (prosta interpolacja)
            float pk = 0.f;
            for (size_t c = 0; c < numCh; ++c)
            {
                const float x = ch[c][i], p = prev[c];
                for (int k = 1; k <= 4; ++k) pk = std::max (pk, std::fabs (p + (x - p) * (float) k * 0.25f));
                prev[c] = x;
            }
            const float need = pk > ceil ? ceil / pk : 1.f;   // wzmocnienie potrzebne dla tej próbki

            // minimum z okna wyprzedzenia (kolejka monotoniczna na stałym buforze - bez alokacji)
            while (qTail != qHead && qVal[(size_t) prevIdx (qTail)] >= need) qTail = prevIdx (qTail);
            qIdx[(size_t) qTail] = sampleIndex; qVal[(size_t) qTail] = need; qTail = nextIdx (qTail);
            while (qIdx[(size_t) qHead] <= sampleIndex - (look + 2)) qHead = nextIdx (qHead);
            const float target = qVal[(size_t) qHead];

            // atak: liniowo w czasie wyprzedzenia; puszczanie zależne od programu
            if (target < env) env = std::max (target, env - (1.f - target) / (float) look - 1e-6f);
            else
            {
                held = target < 0.98f ? held : held * 0.9995f;
                const float k = relFast + (relSlow - relFast) * juce::jlimit (0.f, 1.f, held);
                env = target + (env - target) * k;
            }
            if (env < 0.98f) held = std::min (1.f, held + 1.f / (0.3f * sr));
            minG = std::min (minG, env);

            // opóźniony sygnał * wzmocnienie
            for (size_t c = 0; c < numCh; ++c)
            {
                auto& d = delay[c];
                const float out = d[(size_t) writePos];
                d[(size_t) writePos] = ch[c][i];
                float y = out * env;
                if (std::fabs (y) > ceil) y = std::copysign (ceil, y);   // bezpiecznik
                ch[c][i] = y;
            }
            if (++writePos > look) writePos = 0;
            ++sampleIndex;
        }
        meter.store (juce::Decibels::gainToDecibels (minG));
    }

    float getReductionDb() const { return meter.load(); }

private:
    float sr = 48000.f, env = 1.f, held = 0.f, relFast = 0.99f, relSlow = 0.999f;
    int look = 72, nch = 2, writePos = 0;
    long long sampleIndex = 0;
    std::vector<float> delay[2];
    std::vector<long long> qIdx;
    std::vector<float> qVal;
    int qCap = 80, qHead = 0, qTail = 0;
    int nextIdx (int i) const { return i + 1 == qCap ? 0 : i + 1; }
    int prevIdx (int i) const { return i == 0 ? qCap - 1 : i - 1; }
    float prev[2] {};
    std::atomic<float> meter { 0.f };
};
