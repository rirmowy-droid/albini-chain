#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Settings.h"
#include <array>

/** Barwa źródła: charakter mikrofonu, brzmienie źródła (bas/bębny/wokal),
    obcięcie dołu, „miejsce na wokal” (dołek w środku pasma) i barwa ciemno/jasno.
    Filtry przeliczają się tylko, gdy coś się zmieni (bez alokacji). */
class ToneStage
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& ch : f) for (auto& b : ch) b.reset();
        last = {};
        last.source = -1;
    }

    void process (juce::dsp::AudioBlock<float>& block, const sxa::Settings& s)
    {
        configure (s);
        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        for (size_t c = 0; c < numCh; ++c)
        {
            auto* d = block.getChannelPointer (c);
            auto& fl = f[c];
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                float x = d[i];
                for (int k = 0; k < numActive; ++k) x = fl[(size_t) active[k]].process (x);
                d[i] = x;
            }
        }
    }

private:
    enum Slot { LowCut, V1, V2, V3, M1, M2, Space, TiltLo, TiltHi, Eq0, NumSlots = Eq0 + sxa::kNumEqBands };

    struct Key { int source = -1, mic = -1; float lowCut = 0, space = 1, tilt = 9; std::array<float, sxa::kNumEqBands> eq {}; };

    template <typename Fn>
    void set (int slot, Fn&& fn)
    {
        for (auto& ch : f) fn (ch[(size_t) slot]);
        on[(size_t) slot] = true;
    }

    void configure (const sxa::Settings& s)
    {
        Key k { (int) s.source, (int) s.mic, s.lowCut, s.vocalSpace, s.tilt, {} };
        bool eqSame = true;
        for (int b = 0; b < sxa::kNumEqBands; ++b)
        {
            k.eq[(size_t) b] = s.*(sxa::kEqMembers[b]) * s.autoEqAmt;
            eqSame = eqSame && std::abs (k.eq[(size_t) b] - last.eq[(size_t) b]) < 0.05f;
        }
        if (eqSame && k.source == last.source && k.mic == last.mic && std::abs (k.lowCut - last.lowCut) < 0.5f
            && std::abs (k.space - last.space) < 0.05f && std::abs (k.tilt - last.tilt) < 0.01f)
            return;
        last = k;
        on.fill (false);

        if (k.lowCut > 25.f) set (LowCut, [&] (auto& b) { b.highpass (sr, k.lowCut, 0.71f); });

        switch (k.source)
        {
            case sxa::Bass:
                set (V1, [&] (auto& b) { b.peak (sr, 80.f, 0.9f, 1.5f); });
                set (V2, [&] (auto& b) { b.peak (sr, 250.f, 1.0f, -1.5f); });
                break;
            case sxa::Drums:
                set (V1, [&] (auto& b) { b.lowshelf (sr, 70.f, 2.f); });
                set (V2, [&] (auto& b) { b.peak (sr, 400.f, 1.0f, -2.5f); });
                set (V3, [&] (auto& b) { b.highshelf (sr, 7000.f, 2.5f); });
                break;
            case sxa::Vocals:
                set (V1, [&] (auto& b) { b.highpass (sr, 90.f, 0.71f); });
                set (V2, [&] (auto& b) { b.peak (sr, 300.f, 1.0f, -2.f); });
                set (V3, [&] (auto& b) { b.highshelf (sr, 10000.f, 2.f); });
                break;
            default: break;
        }

        switch (k.mic)
        {
            case sxa::MicDynamic:    // blisko, z prezencją (SM57/SM58)
                set (M1, [&] (auto& b) { b.peak (sr, 5200.f, 1.2f, 4.f); });
                set (M2, [&] (auto& b) { b.lowshelf (sr, 120.f, -1.5f); });
                break;
            case sxa::MicRibbon:     // ciemno i gładko
                set (M1, [&] (auto& b) { b.highshelf (sr, 6000.f, -4.f); });
                set (M2, [&] (auto& b) { b.lowshelf (sr, 150.f, 2.f); });
                break;
            case sxa::MicCondenser:  // powietrze i pełnia
                set (M1, [&] (auto& b) { b.highshelf (sr, 9000.f, 3.f); });
                set (M2, [&] (auto& b) { b.peak (sr, 180.f, 0.8f, 1.f); });
                break;
            default: break;
        }

        if (k.space < -0.1f) set (Space, [&] (auto& b) { b.peak (sr, 2800.f, 1.0f, k.space); });

        if (std::abs (k.tilt) > 0.01f)
        {
            set (TiltLo, [&] (auto& b) { b.lowshelf (sr, 300.f, -4.f * k.tilt); });
            set (TiltHi, [&] (auto& b) { b.highshelf (sr, 2500.f, 4.f * k.tilt); });
        }

        for (int b = 0; b < sxa::kNumEqBands; ++b)
            if (std::abs (k.eq[(size_t) b]) > 0.05f)
            {
                const float g = k.eq[(size_t) b], hz = sxa::kEqHz[b];
                set (Eq0 + b, [&] (auto& bq) { bq.peak (sr, hz, 1.1f, g); });
            }

        numActive = 0;
        for (int i = 0; i < NumSlots; ++i)
            if (on[(size_t) i]) active[numActive++] = i;
    }

    float sr = 48000.f;
    std::array<std::array<stonoga::Biquad, NumSlots>, 2> f;
    std::array<bool, NumSlots> on {};
    std::array<int, NumSlots> active {};
    int numActive = 0;
    Key last;
};
