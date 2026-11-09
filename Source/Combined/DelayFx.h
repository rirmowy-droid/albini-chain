#pragma once

#include <cmath>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Delay gitarowy.
      1 Analogowy (w stylu Ibanez AD-80 / Memory Man): każde powtórzenie ciemniejsze i brudniejsze,
        lekkie falowanie; sprzężenie powyżej 100% wpada w kontrolowany „gwizd” (Lee Ranaldo)
      2 Cyfrowy (w stylu Fender DGL-1): czyste powtórzenia
    timeMs 40-1000, feedback 0..1.1, mix 0..1. Opóźnienie zmienia się płynnie (bez trzasków). */
class DelayFx
{
public:
    enum Model { Off = 0, Analog, Digital, NumModels };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : line) d.init ((size_t) (1.1f * sr) + 8);
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        cur = 300.f * 0.001f * sr;
        lfo = 0.f; appliedModel = -1;
        for (auto& f : fbState) f = 0.f;
    }

    void process (juce::dsp::AudioBlock<float>& block, int model, float timeMs, float feedback, float mix)
    {
        if (model <= Off || model >= NumModels || mix < 0.001f)
        {
            // linia dalej pracuje cicho, żeby włączenie nie wypluło starych powtórzeń
            if (model <= Off) return;
        }
        if (model != appliedModel)
        {
            appliedModel = model;
            for (int c = 0; c < 2; ++c)
            {
                lp[c].lowpass (sr, model == Analog ? 2600.f : 9000.f, 0.6f);
                hp[c].highpass (sr, model == Analog ? 120.f : 60.f, 0.6f);
            }
        }
        const float target = juce::jlimit (40.f, 1000.f, timeMs) * 0.001f * sr;
        const auto n = block.getNumSamples();
        const int nCh = (int) std::min<size_t> (block.getNumChannels(), 2);
        feedback = juce::jlimit (0.f, 1.1f, feedback);
        const float wobInc = 2.f * stonoga::kPi * 0.6f / sr;

        for (size_t i = 0; i < n; ++i)
        {
            // płynna zmiana czasu (jak przekręcanie gałki w analogowym delayu - lekkie „ziuuu”)
            cur += (target - cur) * 0.0004f;
            lfo += wobInc; if (lfo > 2.f * stonoga::kPi) lfo -= 2.f * stonoga::kPi;
            const float d = cur + (model == Analog ? 0.00012f * sr * std::sin (lfo) : 0.f);

            for (int c = 0; c < nCh; ++c)
            {
                const float x = block.getSample (c, (int) i);
                float w = line[c].tap (std::max (1.f, d));
                w = hp[c].process (lp[c].process (w));
                float back = x + feedback * w;
                if (model == Analog) back = std::tanh (back * 1.2f) / 1.2f;   // BBD się nasyca: gwizd nie eksploduje
                else back = juce::jlimit (-1.5f, 1.5f, back);
                line[c].push (back);
                block.setSample (c, (int) i, x + mix * w);
            }
        }
    }

private:
    float sr = 48000.f, cur = 0.f, lfo = 0.f;
    int appliedModel = -1;
    stonoga::Delay line[2];
    stonoga::Biquad lp[2], hp[2];
    float fbState[2] {};
};
