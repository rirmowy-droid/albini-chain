#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <complex>
#include <cmath>
#include "../Stonoga/StonogaDSP.h"

/** Korektor „barwa płyty”: 26 pasm co 1/3 oktawy (40 Hz - 12,5 kHz).

    Dopasowanie (fit) liczy wzmocnienia tak, żeby CAŁA kaskada filtrów dała
    zadaną krzywą w środkach pasm - sąsiednie pasma się nakładają, więc samo
    wpisanie celu w każde pasmo dałoby za dużo; kilka iteracji to poprawia.

    Wzmocnienia ustawia wątek wiadomości (auto-miks), wątek audio tylko je czyta
    i przelicza filtry, gdy się zmienią. */
class AlbumEq
{
public:
    static constexpr int numBands = 26;
    static constexpr float q = 4.32f;            // szerokość 1/3 oktawy

    static float bandHz (int b) { return 31.5f * std::pow (2.f, (float) (b + 1) / 3.f); }   // b=0 -> 40 Hz

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& g : applied) g = 99.f;
        for (auto& ch : f) for (auto& bq : ch) bq.reset();
    }

    /** Wzmocnienia (dB) ustawiane z zewnątrz (atomowo, bez blokad). */
    void setGains (const float* gainsDb) { for (int b = 0; b < numBands; ++b) gains[(size_t) b].store (gainsDb[b]); }
    void getGains (float* out) const     { for (int b = 0; b < numBands; ++b) out[b] = gains[(size_t) b].load(); }

    void process (juce::dsp::AudioBlock<float>& block, float amount)
    {
        bool any = false;
        for (int b = 0; b < numBands; ++b)
        {
            const float g = gains[(size_t) b].load() * amount;
            if (std::abs (g - applied[(size_t) b]) > 0.02f)
            {
                applied[(size_t) b] = g;
                for (auto& ch : f) ch[(size_t) b].peak (sr, bandHz (b), q, g);
            }
            active[(size_t) b] = std::abs (applied[(size_t) b]) > 0.02f;
            any = any || active[(size_t) b];
        }
        if (! any) return;

        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        for (size_t c = 0; c < numCh; ++c)
        {
            auto* d = block.getChannelPointer (c);
            auto& fl = f[c];
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                float x = d[i];
                for (int b = 0; b < numBands; ++b) if (active[(size_t) b]) x = fl[(size_t) b].process (x);
                d[i] = x;
            }
        }
    }

    /** Odpowiedź (dB) jednego filtra peak w częstotliwości hz. */
    static float peakResponseDb (float sampleRate, float f0, float qq, float gainDb, float hz)
    {
        if (std::abs (gainDb) < 1e-4f) return 0.f;
        stonoga::Biquad bq; bq.peak (sampleRate, f0, qq, gainDb);
        const double w = 2.0 * juce::MathConstants<double>::pi * hz / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const std::complex<double> num = (double) bq.b0 + (double) bq.b1 * z1 + (double) bq.b2 * z2;
        const std::complex<double> den = 1.0 + (double) bq.a1 * z1 + (double) bq.a2 * z2;
        return (float) (20.0 * std::log10 (std::abs (num / den)));
    }

    /** Wzmocnienia, przy których kaskada daje krzywą target (dB w środkach pasm). */
    static void fit (float sampleRate, const float* targetDb, float* gainsOut, float limitDb = 8.f)
    {
        for (int b = 0; b < numBands; ++b) gainsOut[b] = targetDb[b];
        for (int it = 0; it < 12; ++it)
        {
            float resp[numBands] {};
            for (int k = 0; k < numBands; ++k)
                for (int b = 0; b < numBands; ++b)
                    resp[k] += peakResponseDb (sampleRate, bandHz (b), q, gainsOut[b], bandHz (k));
            for (int b = 0; b < numBands; ++b)
                gainsOut[b] = juce::jlimit (-limitDb, limitDb, gainsOut[b] + 0.7f * (targetDb[b] - resp[b]));
        }
    }

private:
    float sr = 48000.f;
    std::array<std::array<stonoga::Biquad, numBands>, 2> f;
    std::array<std::atomic<float>, numBands> gains {};
    std::array<float, numBands> applied {};
    std::array<bool, numBands> active {};
};
