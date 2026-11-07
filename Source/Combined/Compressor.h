#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/** Kompresor z jedną gałką (0 = wyłączony).

    Sustain - na przesterze: szybki, mocniejszy, wyrównuje i wydłuża dźwięk.
    Glue    - na całości: wolny, łagodny, skleja przester, chór i pogłosy.

    Stereo sprzężone (jeden detektor dla L i R, panorama się nie przesuwa),
    miękkie kolano, automatyczne wyrównanie głośności. */
class Compressor
{
public:
    enum class Mode { Sustain, Glue };

    void prepare (double sampleRate, Mode m)
    {
        sr = (float) sampleRate;
        mode = m;
        const float att = mode == Mode::Sustain ? 0.001f : 0.020f;
        const float rel = mode == Mode::Sustain ? 0.12f  : 0.30f;
        attackC  = 1.f - std::exp (-1.f / (att * sr));
        releaseC = 1.f - std::exp (-1.f / (rel * sr));
        rmsC     = 1.f - std::exp (-1.f / ((mode == Mode::Sustain ? 0.0015f : 0.005f) * sr));
        amount.reset (sampleRate, 0.05);
        reset();
    }

    void reset() { ms = 0.f; grDb = 0.f; }

    void setAmount (float a) { amount.setTargetValue (juce::jlimit (0.f, 1.f, a)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! amount.isSmoothing() && amount.getTargetValue() <= 0.0001f)
        {
            grDb = 0.f;
            return;
        }

        // ustawienia na blok (gałka jest wygładzana)
        const float a = amount.getCurrentValue();
        amount.skip ((int) block.getNumSamples());
        const bool sustain = mode == Mode::Sustain;
        thrDb  = sustain ? -6.f - 26.f * a : -4.f - 20.f * a;
        ratio  = sustain ? 2.f + 6.f * a   : 1.5f + 2.5f * a;
        kneeDb = sustain ? 8.f : 10.f;
        const float makeupDb = -0.8f * gainComputer (-10.f);

        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        auto* L = block.getChannelPointer (0);
        auto* R = numCh > 1 ? block.getChannelPointer (1) : nullptr;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float x = R ? std::max (std::fabs (L[i]), std::fabs (R[i])) : std::fabs (L[i]);
            ms += rmsC * (x * x - ms);
            const float levelDb = 10.f * std::log10 (ms + 1e-10f) + 3.f; // ~szczyt sinusa

            const float target = gainComputer (levelDb);
            grDb += (target < grDb ? attackC : releaseC) * (target - grDb);

            const float g = juce::Decibels::decibelsToGain (grDb + makeupDb);
            L[i] *= g;
            if (R) R[i] *= g;
        }
    }

    float getGainReductionDb() const { return grDb; }

private:
    float gainComputer (float levelDb) const
    {
        const float over = levelDb - thrDb;
        const float slope = 1.f / ratio - 1.f;
        if (over <= -0.5f * kneeDb) return 0.f;
        if (over <   0.5f * kneeDb) { const float t = over + 0.5f * kneeDb; return slope * t * t / (2.f * kneeDb); }
        return slope * over;
    }

    float sr = 48000.f;
    Mode mode = Mode::Sustain;
    float attackC = 0.1f, releaseC = 0.01f, rmsC = 0.1f;
    float ms = 0.f, grDb = 0.f;
    float thrDb = -12.f, ratio = 2.f, kneeDb = 8.f;
    juce::SmoothedValue<float> amount { 0.f };
};
