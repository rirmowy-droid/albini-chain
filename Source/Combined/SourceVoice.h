#pragma once

#include <array>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"
#include "Settings.h"

/** Barwa z paneli źródeł (bębny / wokal / bas) i 3-pasmowy EQ kanału konsolety.
    Wszystkie gałki na 0 dB = filtr pominięty (bez kosztu i bez zmian w dźwięku). */
class SourceVoice
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; for (auto& k : keys) k = 1e9f; for (auto& b : bands) for (auto& f : b.f) f.reset(); }

    void process (juce::dsp::AudioBlock<float>& block, const sxa::Settings& s)
    {
        const int src = (int) s.source;
        int k = 0;
        auto band = [&] (int type, float hz, float q, float db)
        {
            auto& b = bands[(size_t) k];
            const float key = (float) type * 1e6f + hz + db * 0.001f + q;
            b.active = std::abs (db) > 0.05f;
            if (b.active && std::abs (keys[(size_t) k] - key) > 1e-4f)
            {
                keys[(size_t) k] = key;
                for (auto& f : b.f)
                {
                    if (type == 0) f.lowshelf (sr, hz, db);
                    else if (type == 1) f.peak (sr, hz, q, db);
                    else f.highshelf (sr, hz, db);
                }
            }
            ++k;
        };

        // panel źródła
        if (src == sxa::Drums)
        {
            band (1, 60.f, 1.0f, s.drKick);
            band (1, 400.f, 1.2f, s.drBox);
            band (1, 200.f, 1.2f, s.drSnare);
            band (1, 5000.f, 1.0f, s.drCrack);
            band (2, 9000.f, 0.7f, s.drCymbals);
        }
        else if (src == sxa::Vocals)
        {
            band (0, 150.f, 0.7f, s.voProx);
            band (1, 1000.f, 1.4f, s.voNasal);
            band (1, 3000.f, 0.9f, s.voFront);
            band (2, 12000.f, 0.7f, s.voAir);
            band (1, 1.f, 1.f, 0.f);
        }
        else if (src == sxa::Bass)
        {
            band (0, 80.f, 0.7f, s.baLow);
            band (1, 700.f, 1.0f, s.baGrowl);
            band (1, 2500.f, 1.2f, s.baClick);
            band (1, 1.f, 1.f, 0.f);
            band (1, 1.f, 1.f, 0.f);
        }
        else
            for (int i = 0; i < 5; ++i) band (1, 1.f, 1.f, 0.f);

        // kanał konsolety
        band (0, 100.f, 0.7f, s.chLow);
        band (1, juce::jlimit (200.f, 5000.f, s.chMidHz), 0.9f, s.chMid);
        band (2, 8000.f, 0.7f, s.chHigh);

        const auto n = block.getNumSamples();
        const auto nCh = std::min<size_t> (block.getNumChannels(), 2);
        for (auto& b : bands)
        {
            if (! b.active) continue;
            for (size_t c = 0; c < nCh; ++c)
            {
                auto* d = block.getChannelPointer (c);
                for (size_t i = 0; i < n; ++i) d[i] = b.f[c].process (d[i]);
            }
        }
    }

private:
    struct Band { std::array<stonoga::Biquad, 2> f; bool active = false; };
    float sr = 48000.f;
    std::array<Band, 8> bands;
    std::array<float, 8> keys {};
};
