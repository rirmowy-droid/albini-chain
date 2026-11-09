#pragma once

#include <cmath>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Kostki modulacji z płyt grunge / alternatywy.
      1 Small Clone  - jeden głos BBD, „wodnisty” (Come As You Are)
      2 Polychorus   - chorus/flanger ze sprzężeniem, bardziej metaliczny (In Utero)
      3 Phase 90     - 4 stopnie fazowe, bez sprzężenia, miękko
      4 Small Stone  - 4 stopnie + sprzężenie („kolor”), ostrzej (Alice in Chains)
      5 Flanger      - krótkie opóźnienie 0,3-5 ms ze sprzężeniem
      6 Rotovibe     - Uni-Vibe: 4 niejednakowe stopnie + lekka modulacja głośności (Cantrell)
    rate 0..1 (0,1-8 Hz), depth 0..1, colour 0..1 (sprzężenie / miks). Strój: modulacja
    opóźnienia ograniczona do typowych wartości tych kostek. */
class ModFx
{
public:
    enum Model { Off = 0, SmallClone, Polychorus, Phase90, SmallStone, Flanger, Rotovibe, NumModels };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : dl) d.init ((size_t) (0.03f * sr) + 8);
        reset();
    }

    void reset()
    {
        for (auto& d : dl) d.clear();
        for (auto& c : ap) for (auto& z : c) z = 0.f;
        fb[0] = fb[1] = 0.f; lfo = 0.f;
        for (auto& f : tone) f.reset();
        tone[0].lowpass (sr, 6500.f, 0.6f); tone[1].lowpass (sr, 6500.f, 0.6f);
    }

    void process (juce::dsp::AudioBlock<float>& block, int model, float rate, float depth, float colour)
    {
        if (model <= Off || model >= NumModels) return;
        const auto n = block.getNumSamples();
        const int nCh = (int) std::min<size_t> (block.getNumChannels(), 2);
        const float hz = 0.1f * std::pow (80.f, juce::jlimit (0.f, 1.f, rate));
        const float inc = 2.f * stonoga::kPi * hz / sr;
        depth = juce::jlimit (0.f, 1.f, depth);
        colour = juce::jlimit (0.f, 1.f, colour);

        for (size_t i = 0; i < n; ++i)
        {
            lfo += inc; if (lfo > 2.f * stonoga::kPi) lfo -= 2.f * stonoga::kPi;
            const float tri = 2.f * std::abs (lfo / stonoga::kPi - 1.f) - 1.f;    // trójkąt -1..1 (BBD)
            const float sn = std::sin (lfo);

            for (int c = 0; c < nCh; ++c)
            {
                // drugi kanał z lekko przesuniętą fazą LFO tylko w Polychorusie (szerzej); reszta mono-zgodnie
                const float m = (model == Polychorus && c == 1) ? std::sin (lfo + 1.2f) : (model == SmallClone || model == Flanger ? tri : sn);
                const float x = block.getSample (c, (int) i);
                float y = x;
                switch (model)
                {
                    case SmallClone:
                    {
                        const float dMs = 3.5f + (1.0f + 3.5f * depth) * (0.5f + 0.5f * m);
                        dl[c].push (x);
                        const float w = tone[c].process (dl[c].tap (dMs * 0.001f * sr));
                        const float mix = 0.5f * (0.6f + 0.4f * colour);
                        y = x * (1.f - mix) + w * mix;
                        break;
                    }
                    case Polychorus:
                    case Flanger:
                    {
                        const float base = model == Polychorus ? 2.5f : 0.3f;
                        const float range = model == Polychorus ? 6.f * depth : 4.7f * depth;
                        const float dMs = base + range * (0.5f + 0.5f * m);
                        const float g = (model == Flanger ? 0.85f : 0.6f) * colour;
                        dl[c].push (x + g * fb[c]);
                        const float w = dl[c].tap (std::max (1.f, dMs * 0.001f * sr));
                        fb[c] = std::tanh (w);
                        y = 0.5f * (x + w);
                        break;
                    }
                    case Phase90:
                    case SmallStone:
                    case Rotovibe:
                    {
                        // 4 stopnie wszechprzepustowe 1. rzędu, częstotliwość sterowana LFO (wykładniczo)
                        static const float vibe[4] = { 1.f, 1.9f, 3.4f, 6.1f };
                        const float lo = model == Rotovibe ? 120.f : 200.f, hi = model == Rotovibe ? 1600.f : 3200.f;
                        const float span = std::pow (hi / lo, 0.3f + 0.7f * depth);
                        const float f0 = lo * std::pow (span, 0.5f + 0.5f * m);
                        const float g = model == SmallStone ? 0.7f * colour : (model == Phase90 ? 0.1f : 0.f);
                        float v = x + g * fb[c];
                        for (int k = 0; k < 4; ++k)
                        {
                            const float fk = model == Rotovibe ? f0 * vibe[k] : f0;
                            const float t = std::tan (stonoga::kPi * std::min (fk, 0.45f * sr) / sr);
                            const float a = (t - 1.f) / (t + 1.f);
                            const float out = a * v + ap[c][k];
                            ap[c][k] = v - a * out;
                            v = out;
                        }
                        fb[c] = v;
                        y = 0.5f * (x + v);
                        if (model == Rotovibe) y *= 1.f - 0.15f * depth * (0.5f + 0.5f * m);   // fotokomórka: lekka modulacja głośności
                        break;
                    }
                    default: break;
                }
                block.setSample (c, (int) i, y * outGain[model]);
            }
        }
    }

private:
    // wyrównanie głośności: miks z sygnałem przesuniętym w fazie gasi część pasma
    static constexpr float outGain[NumModels] = { 1.f, 1.43f, 1.64f, 1.53f, 1.57f, 1.35f, 1.58f };
    float sr = 48000.f, lfo = 0.f;
    stonoga::Delay dl[2];
    float ap[2][4] {};
    float fb[2] {};
    stonoga::Biquad tone[2];
};
