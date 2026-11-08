#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

/** Kompresor.

    Tryb automatyczny (jedna gałka):
      Sustain - na źródle: szybki, wyrównuje i wydłuża dźwięk,
      Glue    - na całości: wolny, łagodny, skleja.

    Tryb ręczny: próg, ratio, atak, puszczanie, wzmocnienie, mix (równoległa)
    i charakter:
      Czysty (VCA) - przewidywalny, średnio miękkie kolano,
      Szybki (FET) - detektor szczytowy, twardo i agresywnie, lekko brudzi,
      Optyczny     - wolniejszy atak, puszczanie wydłuża się przy mocnym ściskaniu,
      Lampowy      - bardzo miękkie kolano (ratio rośnie z poziomem), ciepło.

    Stereo sprzężone, wskaźnik ściskania (dB) dla UI. */
class Compressor
{
public:
    enum class Mode { Sustain, Glue };
    enum Character { Clean = 0, Fet, Opto, Tube };

    void prepare (double sampleRate, Mode m)
    {
        sr = (float) sampleRate;
        mode = m;
        amount.reset (sampleRate, 0.05);
        makeupSm.reset (sampleRate, 0.05);
        mixSm.reset (sampleRate, 0.05);
        reset();
    }

    void reset() { ms = 0.f; grDb = 0.f; meterDb = 0.f; }

    void setAmount (float a) { amount.setTargetValue (juce::jlimit (0.f, 1.f, a)); }

    void setManual (bool on, int character, float thresholdDb, float ratioIn, float attackMs, float releaseMs, float makeupDb, float mix)
    {
        manual = on; chr = juce::jlimit (0, 3, character);
        mThr = thresholdDb; mRatio = juce::jmax (1.f, ratioIn);
        mAtt = juce::jmax (0.05f, attackMs); mRel = juce::jmax (5.f, releaseMs);
        makeupSm.setTargetValue (makeupDb);
        mixSm.setTargetValue (juce::jlimit (0.f, 1.f, mix));
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! manual && ! amount.isSmoothing() && amount.getTargetValue() <= 0.0001f)
        {
            grDb = 0.f; meterDb = 0.f;
            return;
        }

        float att, rel, rmsTime, makeupDb = 0.f;
        bool peakDetect = false;
        if (manual)
        {
            thrDb = mThr; ratio = mRatio;
            att = mAtt * 0.001f; rel = mRel * 0.001f;
            switch (chr)
            {
                case Fet:  kneeDb = 2.f;  rmsTime = 0.0002f; peakDetect = true; break;
                case Opto: kneeDb = 10.f; rmsTime = 0.008f;  att = std::max (att, 0.008f); break;
                case Tube: kneeDb = 18.f; rmsTime = 0.006f;  break;
                default:   kneeDb = 6.f;  rmsTime = 0.004f;  break;
            }
        }
        else
        {
            const float a = amount.getCurrentValue();
            amount.skip ((int) block.getNumSamples());
            const bool sustain = mode == Mode::Sustain;
            thrDb  = sustain ? -6.f - 26.f * a : -4.f - 20.f * a;
            ratio  = sustain ? 2.f + 6.f * a   : 1.5f + 2.5f * a;
            kneeDb = sustain ? 8.f : 10.f;
            att = sustain ? 0.001f : 0.020f;
            rel = sustain ? 0.12f  : 0.30f;
            rmsTime = sustain ? 0.0015f : 0.005f;
            makeupDb = -0.8f * gainComputer (-10.f);
        }

        const float attackC = 1.f - std::exp (-1.f / (att * sr));
        const float rmsC    = 1.f - std::exp (-1.f / (rmsTime * sr));
        const float releaseFixed = 1.f - std::exp (-1.f / (rel * sr));

        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        auto* L = block.getChannelPointer (0);
        auto* R = numCh > 1 ? block.getChannelPointer (1) : nullptr;
        float blockMin = 0.f;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float x = R ? std::max (std::fabs (L[i]), std::fabs (R[i])) : std::fabs (L[i]);
            float levelDb;
            if (peakDetect)
            {
                ms = std::max (x, ms * (1.f - rmsC));            // szczytowy, szybko opada
                levelDb = 20.f * std::log10 (ms + 1e-5f);
            }
            else
            {
                ms += rmsC * (x * x - ms);
                levelDb = 10.f * std::log10 (ms + 1e-10f) + 3.f;
            }

            const float target = gainComputer (levelDb);
            // optyczny: im mocniej ściska, tym wolniej puszcza
            const float releaseC = (manual && chr == Opto)
                ? 1.f - std::exp (-1.f / (rel * (1.f + std::fabs (grDb) / 6.f) * sr))
                : releaseFixed;
            grDb += (target < grDb ? attackC : releaseC) * (target - grDb);
            blockMin = std::min (blockMin, grDb);

            const float mk = manual ? makeupSm.getNextValue() : makeupDb;
            const float g = juce::Decibels::decibelsToGain (grDb + mk);
            const float mix = manual ? mixSm.getNextValue() : 1.f;

            float l = L[i] * g, r = R ? R[i] * g : 0.f;
            if (manual && chr == Fet)  { l = colourFet (l);  r = colourFet (r); }
            if (manual && chr == Tube) { l = colourTube (l); r = colourTube (r); }
            L[i] = L[i] + mix * (l - L[i]);
            if (R) R[i] = R[i] + mix * (r - R[i]);
        }
        meterDb.store (blockMin);
    }

    /** Ile ostatnio ściskał (dB, <= 0) - do wskaźnika. */
    float getGainReductionDb() const { return meterDb.load(); }

private:
    float gainComputer (float levelDb) const
    {
        const float over = levelDb - thrDb;
        const float slope = 1.f / ratio - 1.f;
        if (over <= -0.5f * kneeDb) return 0.f;
        if (over <   0.5f * kneeDb) { const float t = over + 0.5f * kneeDb; return slope * t * t / (2.f * kneeDb); }
        return slope * over;
    }

    static float colourFet (float x)  { return x + 0.35f * (std::tanh (1.4f * x) / 1.4f - x); }
    static float colourTube (float x) { const float b = 0.12f; return x + 0.3f * ((std::tanh (x + b) - std::tanh (b)) - x); }

    float sr = 48000.f;
    Mode mode = Mode::Sustain;
    float ms = 0.f, grDb = 0.f;
    std::atomic<float> meterDb { 0.f };
    float thrDb = -12.f, ratio = 2.f, kneeDb = 8.f;

    bool manual = false; int chr = Clean;
    float mThr = -18.f, mRatio = 4.f, mAtt = 10.f, mRel = 150.f;
    juce::SmoothedValue<float> amount { 0.f }, makeupSm { 0.f }, mixSm { 1.f };
};
