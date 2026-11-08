#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "../Stonoga/StonogaDSP.h"
#include "Settings.h"

/** Instrumenty ustępują sobie na żywo (sygnał od innych kopii wtyczki):

    Ustępuj stopie   - bas (albo cokolwiek) ścisza się przy każdym uderzeniu
                       stopy; „tylko dół” ścisza wyłącznie pasmo < 150 Hz,
                       więc atak i warkot basu zostają.
    Ustępuj wokalowi - kiedy wokal śpiewa, w paśmie ~2,5 kHz robi się dołek
                       (dynamiczny korektor), więc gitary nie zasłaniają słów.

    Do tego nadajnik: perkusja publikuje obwiednię stopy, wokal obwiednię głosu
    (0..1, znormalizowane do ostatnich szczytów). */
class DuckStage
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& f : lowSplit) { f.lowpass (sr, 150.f, 0.7071f); f.reset(); }
        for (auto& f : vocalDip) { f.peak (sr, 2500.f, 0.9f, 0.f); f.reset(); }
        kickLp.lowpass (sr, 100.f, 0.7071f);
        kickLp2.lowpass (sr, 100.f, 0.7071f);
        vocHp.highpass (sr, 300.f, 0.7071f);
        duckDb = vocDb = 0.f; kickEnv = kickPeak = vocEnv = vocPeak = 0.f;
        lastVocDb = 99.f;
    }

    // --- nadajnik (perkusja / wokal), na surowym wejściu ------------------------
    float analyseKick (const juce::dsp::AudioBlock<float>& b)
    {
        const float atk = 1.f - std::exp (-1.f / (0.001f * sr)), rel = 1.f - std::exp (-1.f / (0.05f * sr));
        const float peakFall = std::exp (-1.f / (3.0f * sr));
        float out = 0.f;
        for (size_t i = 0; i < b.getNumSamples(); ++i)
        {
            float x = b.getSample (0, (int) i); if (b.getNumChannels() > 1) x = 0.5f * (x + b.getSample (1, (int) i));
            const float a = std::fabs (kickLp2.process (kickLp.process (x)));
            kickEnv += (a > kickEnv ? atk : rel) * (a - kickEnv);
            kickPeak = std::max (kickEnv, kickPeak * peakFall);
            out = std::max (out, kickEnv / std::max (kickPeak, 0.01f));
        }
        return stonoga::clampf (out, 0.f, 1.f);
    }

    float analyseVocal (const juce::dsp::AudioBlock<float>& b)
    {
        const float atk = 1.f - std::exp (-1.f / (0.01f * sr)), rel = 1.f - std::exp (-1.f / (0.15f * sr));
        const float peakFall = std::exp (-1.f / (5.0f * sr));
        float sum = 0.f;
        for (size_t i = 0; i < b.getNumSamples(); ++i)
        {
            float x = b.getSample (0, (int) i); if (b.getNumChannels() > 1) x = 0.5f * (x + b.getSample (1, (int) i));
            const float a = std::fabs (vocHp.process (x));
            vocEnv += (a > vocEnv ? atk : rel) * (a - vocEnv);
            vocPeak = std::max (vocEnv, vocPeak * peakFall);
            sum += vocEnv / std::max (vocPeak, 0.005f);
        }
        return stonoga::clampf (sum / (float) juce::jmax ((size_t) 1, b.getNumSamples()), 0.f, 1.f);
    }

    // --- odbiornik --------------------------------------------------------------
    void process (juce::dsp::AudioBlock<float>& block, const sxa::Settings& s, float kickNorm, float vocalNorm)
    {
        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        const auto n = block.getNumSamples();

        // stopa
        if (s.duckKick > 0.05f || duckDb < -0.05f)
        {
            const float target = -s.duckKick * kickNorm;
            const float atk = 1.f - std::exp (-1.f / (0.002f * sr));
            const float rel = 1.f - std::exp (-1.f / (juce::jmax (0.01f, s.duckRelease * 0.001f) * sr));
            const bool lowOnly = s.duckLowOnly > 0.5f;
            for (size_t i = 0; i < n; ++i)
            {
                duckDb += (target < duckDb ? atk : rel) * (target - duckDb);
                const float g = juce::Decibels::decibelsToGain (duckDb);
                for (size_t c = 0; c < numCh; ++c)
                {
                    auto* d = block.getChannelPointer (c);
                    const float low = lowSplit[c].process (d[i]);
                    d[i] = lowOnly ? d[i] + (g - 1.f) * low : d[i] * g;
                }
            }
        }
        meter.store (duckDb);

        // wokal
        const float vocTarget = -s.duckVocal * vocalNorm;
        const float k = vocTarget < vocDb ? 0.35f : 0.06f;      // szybciej w dół, wolniej w górę (na blok)
        vocDb += k * (vocTarget - vocDb);
        if (std::abs (vocDb) > 0.05f)
        {
            if (std::abs (vocDb - lastVocDb) > 0.05f)
            {
                lastVocDb = vocDb;
                for (auto& f : vocalDip) f.peak (sr, 2500.f, 0.9f, vocDb);
            }
            for (size_t c = 0; c < numCh; ++c)
            {
                auto* d = block.getChannelPointer (c);
                for (size_t i = 0; i < n; ++i) d[i] = vocalDip[c].process (d[i]);
            }
        }
        vocMeter.store (vocDb);
    }

    float getKickDuckDb() const  { return meter.load(); }
    float getVocalDuckDb() const { return vocMeter.load(); }

private:
    float sr = 48000.f;
    stonoga::Biquad lowSplit[2], vocalDip[2], kickLp, kickLp2, vocHp;
    float duckDb = 0, vocDb = 0, lastVocDb = 99.f;
    float kickEnv = 0, kickPeak = 0, vocEnv = 0, vocPeak = 0;
    std::atomic<float> meter { 0.f }, vocMeter { 0.f };
};
