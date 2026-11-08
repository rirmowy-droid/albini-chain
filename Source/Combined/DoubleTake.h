#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** „Drugie podejście” gitary PRZED kostką i piecem.

    Lewy kanał dostaje oryginał, prawy jego wersję zagraną „inaczej”:
    pływające w czasie wejście (różnice rytmiczne kilku ms), minimalne
    rozstrojenie (kilka centów), inna siła uderzenia i odrobinę inna barwa.
    Ponieważ to wejście dopiero potem przechodzi przez przester, harmoniczne
    w obu kanałach się różnią - jak dwie nagrane ścieżki, a nie kopia z opóźnieniem. */
class DoubleTake
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        line.init ((size_t) (0.04f * sr) + 8);
        tone.lowpass (sr, 7000.f, 0.6f);
        reset();
    }

    void reset()
    {
        line.clear(); tone.reset();
        walk = walkT = 0.f; lvl = lvlT = 0.f; ph1 = ph2 = 0.f; counter = 0;
    }

    /** loose 0..1: jak bardzo podejścia się różnią (gałka „Rozjazd”). */
    void process (juce::dsp::AudioBlock<float>& block, float loose)
    {
        if (block.getNumChannels() < 2) return;
        auto* l = block.getChannelPointer (0);
        auto* r = block.getChannelPointer (1);
        const float msS = 0.001f * sr;
        const float spread = 0.4f + 0.6f * loose;
        const float inc1 = 2.f * stonoga::kPi * 0.31f / sr, inc2 = 2.f * stonoga::kPi * 0.73f / sr;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float x = 0.5f * (l[i] + r[i]);

            // co ~0,4 s nowy cel „spóźnienia” - jak muzyk, który raz się spieszy, raz ciągnie
            if ((counter++ % (unsigned) (0.4f * sr)) == 0)
            {
                walkT = random.nextFloat() * 2.f - 1.f;
                lvlT  = random.nextFloat() * 2.f - 1.f;
            }
            walk += (walkT - walk) * 0.00012f;
            lvl  += (lvlT - lvl) * 0.00005f;
            ph1 += inc1; if (ph1 > 2.f * stonoga::kPi) ph1 -= 2.f * stonoga::kPi;
            ph2 += inc2; if (ph2 > 2.f * stonoga::kPi) ph2 -= 2.f * stonoga::kPi;

            // 7 ms ± do 6 ms rytmu; wolna modulacja daje kilka centów rozstrojenia
            const float dMs = 7.f + spread * (6.f * walk + 0.5f * std::sin (ph1) + 0.15f * std::sin (ph2));
            line.push (x);
            float b = line.tap (juce::jmax (1.f, dMs * msS));
            const float t = tone.process (b);
            b = b + 0.25f * spread * (t - b);                           // trochę miękkie kostkowanie
            b *= juce::Decibels::decibelsToGain (1.2f * spread * lvl);  // inna siła uderzenia

            l[i] = x;
            r[i] = b;
        }
    }

private:
    float sr = 48000.f, walk = 0, walkT = 0, lvl = 0, lvlT = 0, ph1 = 0, ph2 = 0;
    unsigned counter = 0;
    stonoga::Delay line;
    stonoga::Biquad tone;
    juce::Random random { 0x2DB1 };
};
