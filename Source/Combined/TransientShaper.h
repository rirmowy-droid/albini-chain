#pragma once

#include <cmath>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>

/** Kształtowanie ataku i wybrzmienia (jak SPL Transient Designer):
    dwie obwiednie - szybka i wolna. Gdy szybka wyprzedza wolną, jest atak;
    gdy wolna zostaje nad szybką, jest wybrzmienie. Nie zależy od progu,
    więc działa tak samo przy cichym i głośnym graniu. */
class TransientShaper
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const auto c = [&] (double ms) { return (float) std::exp (-1.0 / (0.001 * ms * sr)); };
        fastA = c (0.5);  fastR = c (30.0);
        slowA = c (15.0); slowR = c (180.0);
        gainS = c (2.0); gainDown = c (0.15);
        reset();
    }

    void reset() { envFast = envSlow = 0.f; gSm = 1.f; meterDb = 0.f; }

    /** attack, sustain: -1..1 (±12 dB max). */
    void process (juce::dsp::AudioBlock<float>& block, float attack, float sustain)
    {
        if (std::abs (attack) < 0.005f && std::abs (sustain) < 0.005f) { gSm = 1.f; meterDb = 0.f; return; }

        const auto n = block.getNumSamples();
        const auto nCh = block.getNumChannels();
        float minDb = 0.f, maxDb = 0.f;
        for (size_t i = 0; i < n; ++i)
        {
            float x = 0.f;
            for (size_t c = 0; c < nCh; ++c) x = std::max (x, std::abs (block.getSample ((int) c, (int) i)));

            envFast = x > envFast ? fastA * envFast + (1.f - fastA) * x : fastR * envFast + (1.f - fastR) * x;
            envSlow = x > envSlow ? slowA * envSlow + (1.f - slowA) * x : slowR * envSlow + (1.f - slowR) * x;

            const float fDb = 20.f * std::log10 (envFast + 1e-6f);
            const float sDb = 20.f * std::log10 (envSlow + 1e-6f);
            const float diff = fDb - sDb;   // > 0: atak, < 0: wybrzmienie
            float gDb = diff > 0.f ? attack * std::min (12.f, diff * 1.2f)
                                   : sustain * std::min (12.f, -diff * 0.8f);
            // ciszy (pod -60 dB) nie podbijamy - nie wyciągamy szumu
            if (envSlow < 0.001f && gDb > 0.f) gDb *= envSlow / 0.001f;

            const float g = juce::Decibels::decibelsToGain (gDb);
            // spadek wzmocnienia szybko (nowe uderzenie po podbitym ogonie), wzrost łagodnie
            const float k = g < gSm ? gainDown : gainS;
            gSm = k * gSm + (1.f - k) * g;
            for (size_t c = 0; c < nCh; ++c) block.setSample ((int) c, (int) i, block.getSample ((int) c, (int) i) * gSm);
            const float gd = 20.f * std::log10 (gSm);
            minDb = std::min (minDb, gd); maxDb = std::max (maxDb, gd);
        }
        meterDb = std::abs (minDb) > maxDb ? minDb : maxDb;
    }

    float getGainDb() const { return meterDb; }

private:
    double sr = 48000.0;
    float fastA = 0, fastR = 0, slowA = 0, slowR = 0, gainS = 0, gainDown = 0;
    float envFast = 0, envSlow = 0, gSm = 1, meterDb = 0;
};
