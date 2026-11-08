#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"

/** Przestrzeń: panorama + pogłos. Dwa tryby:

    DoubleTrack (na źródle / przesterze) - panorama jak dwa nagrane podejścia:
      lewy kanał = oryginał, prawy = drugie „podejście” (opóźnione, pływające
      w czasie, trochę ciemniejsze). „Rozjazd” ustala, jak bardzo te podejścia
      się różnią - od równego dubla po luźne, żywe granie.

    Widen (na całości) - poszerzenie M/S tylko powyżej ~150 Hz (dół zostaje
      w środku), bez dubla, więc nie psuje całego miksu.

    Pogłos: rozpraszacze + gęsty, modulowany pogłos o regulowanej długości. */
class SpaceSmear
{
public:
    enum class Mode { DoubleTrack, Widen };

    void prepare (double sampleRate, Mode m)
    {
        sr = (float) sampleRate;
        mode = m;
        static const float apMs[2][4] = { { 4.1f, 7.7f, 11.3f, 17.9f }, { 4.7f, 8.3f, 12.9f, 19.1f } };
        for (int s = 0; s < 2; ++s)
        {
            dbl[s].init ((size_t) (0.045f * sr) + 8);
            for (int k = 0; k < 4; ++k)
            {
                apLen[s][k] = apMs[s][k] * 0.001f * sr;
                ap[s][k].init ((size_t) apLen[s][k] + 8);
            }
            hp[s].highpass (sr, 160.f, 0.7f);
            rv[s].init (sr);
            for (int k = 0; k < 8; ++k)
                rv[s].len[k] = stonoga::kLen[k] * 1.25f * 0.001f * sr + (float) s * 0.0023f * sr;
        }
        sideHp.highpass (sr, 150.f, 0.7f);
        damp = 1.f - std::exp (-2.f * stonoga::kPi * 5200.f / sr);
        smear.reset (sampleRate, 0.1);
        driftK = 1.f / (1.2f * sr);
        curDelay = 14.f * 0.001f * sr;
        width.reset (sampleRate, 0.1);
        lastSize = -1.f;
        reset();
    }

    void reset()
    {
        for (int s = 0; s < 2; ++s)
        {
            dbl[s].clear(); rv[s].clear(); hp[s].reset(); tone[s] = 0.f;
            for (auto& a : ap[s]) a.clear();
        }
        sideHp.reset();
        drift = driftT = 0.f; lfo = 0.f; counter = 0;
    }

    void set (float smear01, float width01, float loose01, float size01)
    {
        smear.setTargetValue (stonoga::clampf (smear01, 0.f, 1.f));
        width.setTargetValue (stonoga::clampf (width01, 0.f, 1.f));
        loose = stonoga::clampf (loose01, 0.f, 1.f);
        size  = stonoga::clampf (size01, 0.f, 1.f);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (block.getNumChannels() < 2)
            return;
        if (! smear.isSmoothing() && ! width.isSmoothing()
            && smear.getTargetValue() <= 0.0001f && width.getTargetValue() <= 0.0001f)
            return;

        updateTail();
        float* io[2] = { block.getChannelPointer (0), block.getChannelPointer (1) };
        const float msS = 0.001f * sr;
        const float toneC = 1.f - std::exp (-2.f * stonoga::kPi * (12000.f - 5000.f * loose) / sr);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float sm = smear.getNextValue();
            const float w  = width.getNextValue();
            float l = io[0][i], r = io[1][i];

            if (mode == Mode::DoubleTrack && w > 0.0001f)
            {
                // nowy cel co ~1,5 s, wolne dojście - rytm „pływa”, ale strój nie
                if ((counter++ % (unsigned) (1.5f * sr)) == 0) driftT = random.nextFloat() * 2.f - 1.f;
                drift += (driftT - drift) * driftK;
                lfo += 2.f * stonoga::kPi * 0.17f / sr; if (lfo > 2.f * stonoga::kPi) lfo -= 2.f * stonoga::kPi;

                // drugie podejście: 12-20 ms później, pływa w czasie, trochę ciemniejsze
                const float target = (12.f + 4.f * loose + loose * (6.f * drift + 1.2f * std::sin (lfo))) * msS;
                // ograniczenie prędkości zmiany opóźnienia: maks. ok. 3 centy odstrojenia
                curDelay += juce::jlimit (-maxSlope, maxSlope, target - curDelay);
                dbl[0].push (l); dbl[1].push (r);
                float takeB = dbl[1].tap (curDelay);
                tone[1] += toneC * (takeB - tone[1]);
                takeB = tone[1];

                // lewo: oryginał, prawo: przechodzi w drugie podejście
                const float rr = r * (1.f - w) + takeB * w;
                const float ll = l * (1.f + 0.15f * w);
                l = ll; r = rr * (1.f + 0.15f * w);
            }
            else if (mode == Mode::Widen && w > 0.0001f)
            {
                const float m = 0.5f * (l + r);
                float sd = 0.5f * (l - r);
                sd += 1.2f * w * sideHp.process (sd);
                l = m + sd; r = m - sd;
            }

            if (sm > 0.0001f)
            {
                float in[2] = { l, r };
                for (int s = 0; s < 2; ++s)
                {
                    float diff = hp[s].process (in[s]);
                    for (int k = 0; k < 4; ++k)
                        diff = stonoga::allpass (ap[s][k], diff, apLen[s][k], 0.62f);
                    const float tail = stonoga::reverbTick (rv[s], diff * 0.5f, damp, 0.f, 1.f);
                    const float wet = 0.45f * diff + 1.3f * tail;
                    in[s] = in[s] * (1.f - 0.3f * sm) + sm * wet;
                }
                l = in[0]; r = in[1];
            }

            io[0][i] = l; io[1][i] = r;
        }
    }

private:
    void updateTail()
    {
        if (std::abs (size - lastSize) < 0.005f) return;
        lastSize = size;
        const float t60 = 0.8f + 4.2f * size * size + 0.6f * size;     // ~0,8 .. 5,6 s
        for (int s = 0; s < 2; ++s)
            for (int k = 0; k < 8; ++k)
                rv[s].g[k] = std::pow (10.f, -3.f * rv[s].len[k] / (t60 * sr));
    }

    float sr = 48000.f, damp = 0.5f, lfo = 0.f;
    Mode mode = Mode::DoubleTrack;
    stonoga::Delay dbl[2], ap[2][4];
    float apLen[2][4] {}, tone[2] {};
    stonoga::Biquad hp[2], sideHp;
    stonoga::Reverb rv[2];
    static constexpr float maxSlope = 0.0017f;   // próbek opóźnienia na próbkę (~3 centy)
    float driftK = 0.f, curDelay = 600.f;
    float drift = 0, driftT = 0, loose = 0.3f, size = 0.45f, lastSize = -1.f;
    unsigned counter = 0;
    juce::Random random { 0x5EAD };
    juce::SmoothedValue<float> smear { 0.f }, width { 0.f };
};
