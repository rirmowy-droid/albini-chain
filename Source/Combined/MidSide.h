#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Obróbka środka i boków sumy:
    - mono dół: boki bez dołu poniżej wybranej częstotliwości (20 Hz = wył.),
    - góra boków: półka 5 kHz tylko na bokach (powietrze w szerokości bez syku w środku),
    - prezencja środka: wokal i werbel do przodu albo do tyłu (2,5 kHz). */
class MidSide
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; key = {}; }

    void process (juce::dsp::AudioBlock<float>& block, float monoBassHz, float sideHighDb, float midPresenceDb)
    {
        if (block.getNumChannels() < 2) return;
        const bool bassOn = monoBassHz > 21.f, hiOn = std::abs (sideHighDb) > 0.05f, midOn = std::abs (midPresenceDb) > 0.05f;
        if (! bassOn && ! hiOn && ! midOn) return;

        if (std::abs (key[0] - monoBassHz) > 0.5f || std::abs (key[1] - sideHighDb) > 0.02f || std::abs (key[2] - midPresenceDb) > 0.02f)
        {
            key = { monoBassHz, sideHighDb, midPresenceDb };
            for (auto* f : { &sideHp1, &sideHp2 }) f->highpass (sr, std::max (20.f, monoBassHz), 0.7071f);
            sideHi.highshelf (sr, 5000.f, sideHighDb);
            midPk.peak (sr, 2500.f, 0.8f, midPresenceDb);
        }

        auto* l = block.getChannelPointer (0);
        auto* r = block.getChannelPointer (1);
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
            if (bassOn) s = sideHp2.process (sideHp1.process (s));
            if (hiOn)   s = sideHi.process (s);
            if (midOn)  m = midPk.process (m);
            l[i] = m + s; r[i] = m - s;
        }
    }

private:
    float sr = 48000.f;
    std::array<float, 3> key {};
    stonoga::Biquad sideHp1, sideHp2, sideHi, midPk;
};
