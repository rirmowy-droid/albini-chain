#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Stonoga/StonogaDSP.h"
#include <array>

/** Gruby, szeroki chorus: 6 głosów, każdy z własnym tempem, losowym
    dryfem i miejscem w panoramie (od lewej do prawej krawędzi).
    Dół pasma (poniżej ~180 Hz) nie wchodzi do chorusa, więc bas zostaje
    zwarty i w środku - grubo, ale bez błota. */
class EnsembleChorus
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        buf.init ((size_t) (0.045f * sr) + 16);
        hp.highpass (sr, 180.f, 0.7f);
        static const float base[6] = { 7.1f, 9.7f, 12.3f, 15.1f, 18.7f, 22.9f };
        static const float mult[6] = { 1.00f, 1.17f, 0.83f, 1.33f, 0.71f, 1.49f };
        static const float pos[6]  = { -1.0f, 1.0f, -0.85f, 0.85f, -0.55f, 0.55f };
        for (int v = 0; v < 6; ++v)
        {
            auto& x = voices[(size_t) v];
            x.baseMs = base[v]; x.mult = mult[v];
            const float a = (pos[v] + 1.f) * stonoga::kPi * 0.25f;
            x.gl = std::cos (a); x.gr = std::sin (a);
            x.ph = (float) v * 1.047f;
        }
        amount.reset (sampleRate, 0.05);
        reset();
    }

    void reset()
    {
        buf.clear(); hp.reset();
        for (auto& v : voices) v.drift = v.driftT = 0.f;
        counter = 0;
    }

    void set (float amount01, float rate01, float depth01)
    {
        amount.setTargetValue (stonoga::clampf (amount01, 0.f, 1.f));
        rateHz  = 0.08f + rate01 * rate01 * 2.9f;
        depthMs = 0.4f + depth01 * 6.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! amount.isSmoothing() && amount.getTargetValue() <= 0.0001f)
            return;

        auto* L = block.getChannelPointer (0);
        auto* R = block.getNumChannels() > 1 ? block.getChannelPointer (1) : nullptr;
        const float msS = 0.001f * sr;
        const float inc = 2.f * stonoga::kPi * rateHz / sr;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float a = amount.getNextValue();
            const float xl = L[i], xr = R ? R[i] : xl;
            buf.push (hp.process (0.5f * (xl + xr)));

            if ((counter++ % 2048) == 0)
                for (auto& v : voices) v.driftT = random.nextFloat() * 2.f - 1.f;

            float wl = 0.f, wr = 0.f;
            for (auto& v : voices)
            {
                v.ph += inc * v.mult; if (v.ph > 2.f * stonoga::kPi) v.ph -= 2.f * stonoga::kPi;
                v.drift += (v.driftT - v.drift) * 0.0003f;
                const float d = v.baseMs + depthMs * 0.5f * (1.f + std::sin (v.ph)) + 0.4f * depthMs * 0.15f * v.drift;
                const float s = buf.tap (d * msS);
                wl += s * v.gl;
                wr += s * v.gr;
            }
            // głosy z lewej i prawej są inne, więc boki się rozjeżdżają (szeroko),
            // a suchy sygnał zostaje w środku (gruby rdzeń)
            const float k = 0.62f * a;
            L[i] = xl * (1.f - 0.35f * a) + k * wl;
            if (R) R[i] = xr * (1.f - 0.35f * a) + k * wr;
        }
    }

private:
    struct Voice { float baseMs = 10, mult = 1, gl = 0.7f, gr = 0.7f, ph = 0, drift = 0, driftT = 0; };

    float sr = 48000.f, rateHz = 0.5f, depthMs = 3.f;
    stonoga::Delay buf;
    stonoga::Biquad hp;
    std::array<Voice, 6> voices;
    unsigned counter = 0;
    juce::Random random { 0xC0A5 };
    juce::SmoothedValue<float> amount { 0.f };
};
