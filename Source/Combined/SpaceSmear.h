#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Przestrzeń na końcu łańcucha.

    Używany dwa razy: na samym przesterze (krótszy ogon) i na całości.

    Panorama (szerokość): „podwójne nagranie” - lewy i prawy kanał dostają osobno
    opóźnioną i lekko pływającą kopię (jak dwie zagrane ścieżki), plus
    poszerzenie M/S.
    Pogłos (rozmycie): rozpraszacze (allpassy) + długi, gęsty, modulowany pogłos,
    które rozlewają ścianę przesteru w przestrzeni. */
class SpaceSmear
{
public:
    void prepare (double sampleRate, float tailSeconds = 3.2f)
    {
        sr = (float) sampleRate;
        static const float apMs[2][4] = { { 4.1f, 7.7f, 11.3f, 17.9f }, { 4.7f, 8.3f, 12.9f, 19.1f } };
        for (int s = 0; s < 2; ++s)
        {
            dbl[s].init ((size_t) (0.03f * sr) + 8);
            for (int k = 0; k < 4; ++k)
            {
                apLen[s][k] = apMs[s][k] * 0.001f * sr;
                ap[s][k].init ((size_t) apLen[s][k] + 8);
            }
            hp[s].highpass (sr, 160.f, 0.7f);
            rv[s].init (sr);
            for (int k = 0; k < 8; ++k)
            {
                rv[s].len[k] = stonoga::kLen[k] * 1.25f * 0.001f * sr + (float) s * 0.0023f * sr;
                rv[s].g[k]   = std::pow (10.f, -3.f * rv[s].len[k] / (tailSeconds * sr));
            }
        }
        damp = 1.f - std::exp (-2.f * stonoga::kPi * 5200.f / sr);
        smear.reset (sampleRate, 0.1);
        width.reset (sampleRate, 0.1);
        reset();
    }

    void reset()
    {
        for (int s = 0; s < 2; ++s)
        {
            dbl[s].clear(); rv[s].clear(); hp[s].reset();
            for (auto& a : ap[s]) a.clear();
            drift[s] = driftT[s] = 0.f;
        }
        lfo = 0.f; counter = 0;
    }

    void setSmear (float v) { smear.setTargetValue (stonoga::clampf (v, 0.f, 1.f)); }
    void setWidth (float v) { width.setTargetValue (stonoga::clampf (v, 0.f, 1.f)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (block.getNumChannels() < 2)
            return; // szerokość i rozmycie mają sens tylko w stereo

        if (! smear.isSmoothing() && ! width.isSmoothing()
            && smear.getTargetValue() <= 0.0001f && width.getTargetValue() <= 0.0001f)
            return;

        float* io[2] = { block.getChannelPointer (0), block.getChannelPointer (1) };
        const float baseMs[2] = { 11.0f, 16.5f };
        const float msS = 0.001f * sr;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float sm = smear.getNextValue();
            const float w  = width.getNextValue();

            if ((counter++ % 4096) == 0)
                for (int s = 0; s < 2; ++s) driftT[s] = random.nextFloat() * 2.f - 1.f;
            lfo += 2.f * stonoga::kPi * 0.21f / sr; if (lfo > 2.f * stonoga::kPi) lfo -= 2.f * stonoga::kPi;

            float o[2];
            for (int s = 0; s < 2; ++s)
            {
                const float x = io[s][i];
                drift[s] += (driftT[s] - drift[s]) * 0.0002f;

                // podwójne nagranie
                dbl[s].push (x);
                const float mod = 0.9f * drift[s] + 0.35f * std::sin (lfo + (float) s * 1.9f);
                const float d = dbl[s].tap ((baseMs[s] + mod) * msS);
                float y = x * (1.f - 0.2f * w) + d * 0.65f * w;

                // rozmycie
                if (sm > 0.0001f)
                {
                    float diff = hp[s].process (y);
                    for (int k = 0; k < 4; ++k)
                        diff = stonoga::allpass (ap[s][k], diff, apLen[s][k], 0.62f);
                    const float tail = stonoga::reverbTick (rv[s], diff * 0.5f, damp, 0.f, 1.f);
                    const float wet = 0.45f * diff + 1.3f * tail;
                    y = y * (1.f - 0.3f * sm) + sm * wet;
                }
                o[s] = y;
            }

            // M/S
            const float m = 0.5f * (o[0] + o[1]);
            const float sd = 0.5f * (o[0] - o[1]) * (1.f + 0.8f * w);
            io[0][i] = m + sd;
            io[1][i] = m - sd;
        }
    }

private:
    float sr = 48000.f, damp = 0.5f, lfo = 0.f;
    stonoga::Delay dbl[2], ap[2][4];
    float apLen[2][4] {};
    stonoga::Biquad hp[2];
    stonoga::Reverb rv[2];
    float drift[2] {}, driftT[2] {};
    unsigned counter = 0;
    juce::Random random { 0x5EAD };
    juce::SmoothedValue<float> smear { 0.f }, width { 0.f };
};
