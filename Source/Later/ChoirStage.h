#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"
#include <array>

/** Chór: z przesterowanej gitary robi warstwę „głosów”.

    - wyrównany poziom wejścia (chór brzmi równo, nie skacze z dynamiką gry)
    - 6 głosów: oktawa w dół, 2x unison rozstrojony o kilka centów, kwinta,
      oktawa w górę (cicho) - każdy z własnym wibrato i miejscem w panoramie
    - formanty samogłosek a/o/u, które powoli płyną
    - ciemny, długi pogłos, w którym głosy się zlewają

    Nie ma tu pętli zwrotnej z przesuwaniem wysokości (to w starym chórze
    narastało w pisk), więc chór nie może się „rozpędzić”. */
class ChoirStage
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        win = 0.065f * sr;
        buf.init ((size_t) (win + 0.002f * sr) + 16);

        hp.highpass (sr, 140.f, 0.7f);
        lp.lowpass (sr, 2600.f, 0.7f);
        for (int s = 0; s < 2; ++s)
        {
            outLp[s].lowpass (sr, 4200.f, 0.7f);
            rv[s].init (sr);
            for (int k = 0; k < 8; ++k)
            {
                rv[s].len[k] = stonoga::kLen[k] * 1.35f * 0.001f * sr + (float) s * 0.0029f * sr;
                rv[s].g[k]   = std::pow (10.f, -3.f * rv[s].len[k] / (4.5f * sr));
            }
        }
        damp = 1.f - std::exp (-2.f * stonoga::kPi * 3800.f / sr);

        //            ratio      gain   pan    vib Hz
        setVoice (0, 0.5f,       0.80f, 0.50f, 4.7f);   // oktawa w dół (basy chóru)
        setVoice (1, 0.99538f,   0.75f, 0.15f, 5.3f);   // unison -8 c
        setVoice (2, 1.00463f,   0.75f, 0.85f, 5.0f);   // unison +8 c
        setVoice (3, 1.49831f,   0.55f, 0.30f, 5.6f);   // kwinta
        setVoice (4, 1.50351f,   0.45f, 0.70f, 4.9f);   // kwinta, rozstrojona
        setVoice (5, 2.0f,       0.22f, 0.50f, 5.8f);   // oktawa w górę, cicho

        amount.reset (sampleRate, 0.1);
        reset();
    }

    void reset()
    {
        buf.clear();
        for (int s = 0; s < 2; ++s) { rv[s].clear(); fA[s].reset(); fB[s].reset(); outLp[s].reset(); }
        hp.reset(); lp.reset();
        env = 0.f; gate = 0.f; vowelPh = 0.f; counter = 0;
        for (size_t i = 0; i < voices.size(); ++i) voices[i].ph = (float) i / (float) voices.size();
    }

    void setAmount (float a) { amount.setTargetValue (stonoga::clampf (a, 0.f, 1.f)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! amount.isSmoothing() && amount.getTargetValue() <= 0.0001f)
            return;

        auto* L = block.getChannelPointer (0);
        auto* R = block.getNumChannels() > 1 ? block.getChannelPointer (1) : nullptr;
        const float atk = 1.f - std::exp (-1.f / (0.03f * sr));
        const float rel = 1.f - std::exp (-1.f / (0.4f * sr));
        const float gateUp = 1.f / (0.35f * sr), gateDown = 1.f / (1.5f * sr);
        const float vibDepth = 0.00022f * sr;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float a = amount.getNextValue();
            float x = R ? 0.5f * (L[i] + R[i]) : L[i];
            x = lp.process (hp.process (x));

            // wyrównanie poziomu + łagodne narastanie (chór "wchodzi", nie uderza)
            const float ax = std::fabs (x);
            env += (ax > env ? atk : rel) * (ax - env);
            gate = stonoga::clampf (gate + (env > 0.004f ? gateUp : -gateDown), 0.f, 1.f);
            const float level = std::min (6.f, 0.18f / (env + 1e-3f));
            buf.push (x * level * gate * gate);

            if ((counter++ & 31) == 0)
                updateVowel();

            float vl = 0.f, vr = 0.f;
            for (auto& v : voices)
            {
                v.ph += (1.f - v.ratio) / win;
                if (v.ph >= 1.f) v.ph -= 1.f;
                if (v.ph < 0.f)  v.ph += 1.f;
                float ph2 = v.ph + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;

                v.vib += 2.f * stonoga::kPi * v.vibHz / sr;
                if (v.vib > 2.f * stonoga::kPi) v.vib -= 2.f * stonoga::kPi;
                const float mod = vibDepth * (1.f + std::sin (v.vib));

                const float o = buf.tap (v.ph * win + 2.f + mod) * std::sin (stonoga::kPi * v.ph)
                              + buf.tap (ph2  * win + 2.f + mod) * std::sin (stonoga::kPi * ph2);
                vl += o * v.gain * (1.f - v.pan);
                vr += o * v.gain * v.pan;
            }

            float out[2] = { vl, vr };
            for (int s = 0; s < 2; ++s)
            {
                const float f = fA[s].process (out[s]) * 2.2f + fB[s].process (out[s]) * 1.5f + out[s] * 0.2f;
                const float shaped = outLp[s].process (f);
                const float tail = stonoga::reverbTick (rv[s], shaped * 0.5f, damp, 0.f, 1.f);
                out[s] = 0.35f * shaped + 1.1f * tail;
            }

            const float g = a * 0.9f;
            if (R) { L[i] += g * out[0]; R[i] += g * out[1]; }
            else     L[i] += g * 0.5f * (out[0] + out[1]);
        }
    }

private:
    struct Voice { float ratio = 1, gain = 0, pan = 0.5f, vibHz = 5, ph = 0, vib = 0; };

    void setVoice (int i, float ratio, float gain, float pan, float vibHz)
    {
        auto& v = voices[(size_t) i];
        v.ratio = ratio; v.gain = gain; v.pan = pan; v.vibHz = vibHz;
        v.vib = (float) i * 1.3f;
    }

    // samogłoska płynie powoli: a -> o -> u -> o -> a ...
    void updateVowel()
    {
        vowelPh += 2.f * stonoga::kPi * 0.045f * 32.f / sr;
        if (vowelPh > 2.f * stonoga::kPi) vowelPh -= 2.f * stonoga::kPi;
        const float t = 0.5f + 0.5f * std::sin (vowelPh);           // 0 = a, 1 = u
        const float f1 = t < 0.5f ? 800.f + (500.f - 800.f) * (t * 2.f)  : 500.f + (350.f - 500.f) * (t * 2.f - 1.f);
        const float f2 = t < 0.5f ? 1150.f + (880.f - 1150.f) * (t * 2.f) : 880.f + (650.f - 880.f) * (t * 2.f - 1.f);
        for (int s = 0; s < 2; ++s)
        {
            const float spread = s == 0 ? 0.97f : 1.03f;                // lewy i prawy "śpiewają" trochę inaczej
            fA[s].bandpass (sr, f1 * spread, 4.5f);
            fB[s].bandpass (sr, f2 * spread, 5.5f);
        }
    }

    float sr = 48000.f, win = 3000.f, damp = 0.4f;
    stonoga::Delay buf;
    std::array<Voice, 6> voices;
    stonoga::Biquad hp, lp, fA[2], fB[2], outLp[2];
    stonoga::Reverb rv[2];
    float env = 0, gate = 0, vowelPh = 0;
    unsigned counter = 0;
    juce::SmoothedValue<float> amount { 0.0f };
};
