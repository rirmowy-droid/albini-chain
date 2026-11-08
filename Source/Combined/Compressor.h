#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include "../Stonoga/StonogaDSP.h"

/** Kompresor z charakterami w stylu klasycznych urządzeń studyjnych.

    Tryb automatyczny (jedna gałka): Sustain (na źródle) albo Glue (na całości),
    próg i ratio z gałki, a czasy, kolano, detektor i zabarwienie z charakteru.

    Tryb ręczny: próg, ratio, atak, puszczanie, wzmocnienie, mix (równoległa),
    w granicach, jakie miało dane urządzenie:

      0 Czysty (VCA)       - przewidywalny, średnie kolano
      1 W stylu 1176 (FET) - ratio przełączane 4/8/12/20, atak 0,02-0,8 ms,
                             puszczanie 50-1100 ms, szczytowy detektor, brudzi;
                             „wszystkie przyciski”: ratio ~ nieskończone, twarde
                             kolano, mocniejszy brud i pompowanie
      2 W stylu LA-2A      - stały atak ~10 ms, ratio ~3:1 (albo „limit” powyżej 10:1),
                             dwustopniowe puszczanie: szybko do połowy, potem powoli
                             (im dłużej ściskał, tym wolniej)
      3 W stylu dbx 160    - twarde kolano, prawdziwe RMS, puszczanie ze stałą
                             szybkością ~120 dB/s (charakterystyczne „pompowanie”)
      4 W stylu vari-mu    - lampowy (Fairchild): bardzo miękkie kolano, ratio rośnie
                             z poziomem, ciepłe parzyste harmoniczne
      5 W stylu szyny SSL  - ratio 2/4/10, atak 0,1/0,3/1/3/10/30 ms, puszczanie
                             0,1/0,3/0,6/1,2 s albo auto (zależne od programu)

    Stereo sprzężone, wskaźnik ściskania (dB) dla UI. */
class Compressor
{
public:
    enum class Mode { Sustain, Glue };
    enum Character { Clean = 0, Fet1176, OptoLA2A, Dbx160, VariMu, SslBus, NumCharacters };

    void prepare (double sampleRate, Mode m)
    {
        sr = (float) sampleRate;
        mode = m;
        amount.reset (sampleRate, 0.05);
        makeupSm.reset (sampleRate, 0.05);
        mixSm.reset (sampleRate, 0.05);
        reset();
    }

    void reset() { ms = 0.f; grDb = 0.f; meterDb = 0.f; heldSeconds = 0.f; peakGr = 0.f; }

    void setAmount (float a) { amount.setTargetValue (juce::jlimit (0.f, 1.f, a)); }
    void setCharacter (int c) { chr = juce::jlimit (0, (int) NumCharacters - 1, c); }

    /** Ręczny tryb z automatycznym wyrównaniem głośności (dla kleju sumy). */
    void setAutoMakeup (bool on) { autoMakeup = on; }

    /** Filtr w torze detekcji: dół (stopa, bas) nie wywołuje ściskania całości. */
    void setSidechainHighpass (float hz)
    {
        if (std::abs (hz - scHz) < 0.5f) return;
        scHz = hz;
        for (auto& f : scHp) f.highpass (sr, juce::jmax (20.f, hz), 0.7071f);
    }

    void setManual (bool on, float thresholdDb, float ratioIn, float attackMs, float releaseMs,
                    float makeupDb, float mix, bool allButtons)
    {
        manual = on; allBtn = allButtons;
        mThr = thresholdDb; mRatio = juce::jmax (1.f, ratioIn);
        mAtt = juce::jmax (0.01f, attackMs); mRel = juce::jmax (5.f, releaseMs);
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

        // --- charakter: kolano, detektor, czasy domyślne -------------------------
        float att = 0.005f, rel = 0.15f, rmsTime = 0.004f;
        bool peakDetect = false;
        colour = 0;
        switch (chr)
        {
            case Fet1176:  kneeDb = 2.f;  rmsTime = 0.0002f; peakDetect = true; att = 0.0002f; rel = 0.08f;  colour = 1; break;
            case OptoLA2A: kneeDb = 10.f; rmsTime = 0.008f;  att = 0.010f; rel = 0.06f; break;
            case Dbx160:   kneeDb = 0.5f; rmsTime = 0.005f;  att = 0.008f; rel = 0.1f;  break;
            case VariMu:   kneeDb = 18.f; rmsTime = 0.006f;  att = 0.0004f; rel = 0.3f; colour = 2; break;
            case SslBus:   kneeDb = 3.f;  rmsTime = 0.003f;  att = 0.003f; rel = 0.3f;  break;
            default:       kneeDb = 6.f;  rmsTime = 0.004f;  att = 0.005f; rel = 0.15f; break;
        }

        float makeupDb = 0.f;
        bool autoRelease = false;
        if (manual)
        {
            thrDb = mThr; ratio = mRatio;
            att = mAtt * 0.001f; rel = mRel * 0.001f;
            switch (chr)
            {
                case Fet1176:
                {
                    static const float ratios[] = { 4.f, 8.f, 12.f, 20.f };
                    ratio = snap (ratio, ratios, 4);
                    att = juce::jlimit (0.00002f, 0.0008f, att);
                    rel = juce::jlimit (0.05f, 1.1f, rel);
                    if (allBtn) { ratio = 60.f; kneeDb = 0.f; rel *= 0.6f; colour = 3; }
                    break;
                }
                case OptoLA2A: ratio = mRatio >= 10.f ? 20.f : 3.f; att = 0.010f; break;
                case SslBus:
                {
                    static const float ratios[] = { 2.f, 4.f, 10.f };
                    static const float atts[]   = { 0.0001f, 0.0003f, 0.001f, 0.003f, 0.01f, 0.03f };
                    static const float rels[]   = { 0.1f, 0.3f, 0.6f, 1.2f };
                    ratio = snap (ratio, ratios, 3);
                    att   = snap (att, atts, 6);
                    autoRelease = rel > 1.3f;                 // powyżej 1,2 s = „auto”
                    rel   = autoRelease ? 0.3f : snap (rel, rels, 4);
                    break;
                }
                default: break;
            }
        }
        else
        {
            const float a = amount.getCurrentValue();
            amount.skip ((int) block.getNumSamples());
            const bool sustain = mode == Mode::Sustain;
            thrDb  = sustain ? -6.f - 26.f * a : -4.f - 20.f * a;
            ratio  = sustain ? 2.f + 6.f * a   : 1.5f + 2.5f * a;
            if (chr == Clean) { kneeDb = sustain ? 8.f : 10.f; att = sustain ? 0.001f : 0.020f; rel = sustain ? 0.12f : 0.30f; rmsTime = sustain ? 0.0015f : 0.005f; }
            if (! sustain && chr == SslBus) { att = 0.01f; autoRelease = true; }
            if (chr == OptoLA2A) ratio = std::min (ratio, 4.f);
            makeupDb = -0.8f * gainComputer (-10.f);
        }

        manualAutoMakeup = (manual && autoMakeup) ? -0.5f * gainComputer (-12.f) : 0.f;
        const float attackC = 1.f - std::exp (-1.f / (att * sr));
        const float rmsC    = 1.f - std::exp (-1.f / (rmsTime * sr));
        const float releaseFixed = 1.f - std::exp (-1.f / (rel * sr));
        const float releaseSlowAuto = 1.f - std::exp (-1.f / (1.2f * sr));
        const float dbxRate = 120.f / sr;    // dB na próbkę

        const auto numCh = juce::jmin ((size_t) 2, block.getNumChannels());
        auto* L = block.getChannelPointer (0);
        auto* R = numCh > 1 ? block.getChannelPointer (1) : nullptr;
        float blockMin = 0.f;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            float dl = L[i], dr = R ? R[i] : 0.f;
            if (scHz > 25.f) { dl = scHp[0].process (dl); dr = scHp[1].process (dr); }
            const float x = R ? std::max (std::fabs (dl), std::fabs (dr)) : std::fabs (dl);
            float levelDb;
            if (peakDetect)
            {
                ms = std::max (x, ms * (1.f - rmsC));
                levelDb = 20.f * std::log10 (ms + 1e-5f);
            }
            else
            {
                ms += rmsC * (x * x - ms);
                levelDb = 10.f * std::log10 (ms + 1e-10f) + 3.f;
            }

            const float target = gainComputer (levelDb);
            if (target < grDb)
            {
                grDb += attackC * (target - grDb);
                heldSeconds += 1.f / sr;
                peakGr = std::min (peakGr, grDb);
            }
            else
            {
                float rc = releaseFixed;
                if (chr == OptoLA2A)
                {
                    // dwustopniowo: szybko do połowy, potem powoli (dłużej ściskał = wolniej)
                    const bool firstHalf = grDb < 0.5f * peakGr;
                    const float slow = 0.5f + std::min (4.5f, heldSeconds * 0.8f);
                    rc = firstHalf ? releaseFixed : 1.f - std::exp (-1.f / (slow * sr));
                }
                else if (autoRelease)
                {
                    // auto: szybki powrót po krótkich szczytach, wolny po długim ściskaniu
                    const float k = juce::jlimit (0.f, 1.f, heldSeconds / 0.5f);
                    rc = releaseFixed + k * (releaseSlowAuto - releaseFixed);
                }
                if (chr == Dbx160) grDb = std::min (target, grDb + dbxRate);
                else               grDb += rc * (target - grDb);
                if (grDb > -0.1f) { heldSeconds = 0.f; peakGr = 0.f; }
            }
            blockMin = std::min (blockMin, grDb);

            const float mk = manual ? makeupSm.getNextValue() + (autoMakeup ? manualAutoMakeup : 0.f) : makeupDb;
            const float g = juce::Decibels::decibelsToGain (grDb + mk);
            const float mix = manual ? mixSm.getNextValue() : 1.f;

            float l = L[i] * g, r = R ? R[i] * g : 0.f;
            if (colour) { l = colourise (l); r = colourise (r); }
            L[i] = L[i] + mix * (l - L[i]);
            if (R) R[i] = R[i] + mix * (r - R[i]);
        }
        meterDb.store (blockMin);
    }

    /** Ile ostatnio ściskał (dB, <= 0) - do wskaźnika. */
    float getGainReductionDb() const { return meterDb.load(); }

private:
    static float snap (float v, const float* list, int n)
    {
        float best = list[0];
        for (int i = 1; i < n; ++i)
            if (std::abs (std::log (v / list[i])) < std::abs (std::log (v / best))) best = list[i];
        return best;
    }

    float gainComputer (float levelDb) const
    {
        const float over = levelDb - thrDb;
        float r = ratio;
        if (chr == VariMu) r = 1.f + (ratio - 1.f) * juce::jlimit (0.f, 1.f, over / 18.f + 0.5f);   // ratio rośnie z poziomem
        const float slope = 1.f / r - 1.f;
        if (kneeDb <= 0.01f) return over > 0.f ? slope * over : 0.f;
        if (over <= -0.5f * kneeDb) return 0.f;
        if (over <   0.5f * kneeDb) { const float t = over + 0.5f * kneeDb; return slope * t * t / (2.f * kneeDb); }
        return slope * over;
    }

    float colourise (float x) const
    {
        switch (colour)
        {
            case 1:  return x + 0.35f * (std::tanh (1.4f * x) / 1.4f - x);                       // FET
            case 2:  { const float b = 0.12f; return x + 0.3f * ((std::tanh (x + b) - std::tanh (b)) - x); } // lampa
            case 3:  return x + 0.6f * (std::tanh (2.0f * x) / 2.0f - x);                        // wszystkie przyciski
            default: return x;
        }
    }

    float sr = 48000.f;
    float scHz = 0.f;
    stonoga::Biquad scHp[2];
    Mode mode = Mode::Sustain;
    float ms = 0.f, grDb = 0.f, heldSeconds = 0.f, peakGr = 0.f;
    std::atomic<float> meterDb { 0.f };
    float thrDb = -12.f, ratio = 2.f, kneeDb = 8.f;
    int chr = Clean, colour = 0;

    bool manual = false, allBtn = false, autoMakeup = false;
    float manualAutoMakeup = 0.f;
    float mThr = -18.f, mRatio = 4.f, mAtt = 10.f, mRel = 150.f;
    juce::SmoothedValue<float> amount { 0.f }, makeupSm { 0.f }, mixSm { 1.f };
};
