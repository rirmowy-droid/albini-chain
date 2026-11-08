#pragma once

#include <cmath>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Odsyczanie: pasmo nad ok. 4,5 kHz jest ściszane tylko wtedy, gdy syczące
    głoski (s, sz, cz) przeważają nad resztą głosu. Podział na pasma jest
    komplementarny (góra = sygnał - dół), więc bez syczenia sygnał przechodzi
    bez zmian. */
class DeEsser
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int c = 0; c < 2; ++c) { split[c].lowpass (sr, 4500.f, 0.7071f); split[c].reset(); }
        for (auto& f : detHi) { f.highpass (sr, 5500.f, 0.7f); f.reset(); }
        for (auto& f : detLo) { f.bandpass (sr, 800.f, 0.5f); f.reset(); }
        att = (float) std::exp (-1.0 / (0.0008 * sr));
        rel = (float) std::exp (-1.0 / (0.06 * sr));
        envHi = envLo = 0.f; gSm = 1.f; meter = 0.f;
    }

    void process (juce::dsp::AudioBlock<float>& block, float amount)
    {
        if (amount < 0.005f) { meter = 0.f; return; }
        const auto n = block.getNumSamples();
        const auto nCh = std::min<size_t> (block.getNumChannels(), 2);
        const float maxCut = 3.f + 9.f * amount;          // do 12 dB
        const float thresh = 1.6f - 1.0f * amount;        // stosunek syczenia do głosu, od którego tniemy
        float worst = 0.f;
        for (size_t i = 0; i < n; ++i)
        {
            float hi = 0.f, lo = 0.f;
            for (size_t c = 0; c < nCh; ++c)
            {
                const float x = block.getSample ((int) c, (int) i);
                hi = std::max (hi, std::abs (detHi[c].process (x)));
                lo = std::max (lo, std::abs (detLo[c].process (x)));
            }
            envHi = hi > envHi ? att * envHi + (1 - att) * hi : rel * envHi + (1 - rel) * hi;
            envLo = lo > envLo ? att * envLo + (1 - att) * lo : rel * envLo + (1 - rel) * lo;

            const float ratio = envHi / (envLo + 1e-5f);
            float cutDb = 0.f;
            if (ratio > thresh && envHi > 0.003f)
                cutDb = std::min (maxCut, 20.f * std::log10 (ratio / thresh) * 1.5f);
            const float g = juce::Decibels::decibelsToGain (-cutDb);
            gSm = g < gSm ? 0.5f * gSm + 0.5f * g : 0.995f * gSm + 0.005f * g;
            worst = std::max (worst, -20.f * std::log10 (gSm));

            for (size_t c = 0; c < nCh; ++c)
            {
                const float x = block.getSample ((int) c, (int) i);
                const float low = split[c].process (x);
                block.setSample ((int) c, (int) i, low + (x - low) * gSm);
            }
        }
        meter = worst;
    }

    float getReductionDb() const { return meter; }

private:
    float sr = 48000.f, att = 0, rel = 0, envHi = 0, envLo = 0, gSm = 1, meter = 0;
    stonoga::Biquad split[2], detHi[2], detLo[2];
};
