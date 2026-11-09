#pragma once

#include <cmath>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/AmpStage.h"   // DBiquad, fastTanh

/** Kostka przed Stonogą - modele klasycznych przesterów z płyt grunge / noise rock.
    Działa w domenie oversamplingu kostki (4x albo 8x), żeby przester nie aliasował.

      1 DS-1        - twardy, ziarnisty (Nirvana: Bleach / Nevermind)
      2 DS-2        - DS-1 z trybem Turbo: podbity środek, więcej gainu (In Utero, na żywo)
      3 Rat         - wzmacniacz operacyjny + diody do masy, filtr; agresywny środek
      4 Turbo Rat   - Rat z diodami LED: głośniej, otwarciej, mniej kompresji (Thurston Moore)
      5 Big Muff    - dwa stopnie fuzzu, wycięty środek, długi sustain (Lithium, Smashing Pumpkins, Dinosaur Jr.)
      6 SansAmp     - lampowy charakter w kostce, środek do przodu (In Utero)
      7 Tube Screamer - przester tylko powyżej ok. 720 Hz, środek „garbem”, dół czysty
      8 Kompresor / sustainer - jak Boss CS-2 (Lee Ranaldo przed Ratem)

    gain 0..1, tone 0..1 (ciemno..jasno; w kompresorze = szybkość ataku), levelDb wyjście. */
class DrivePedal
{
public:
    enum Model { Off = 0, DS1, DS2, Rat, TurboRat, BigMuff, SansAmp, TubeScreamer, Comp, NumModels };

    void prepare (double osSampleRate)
    {
        sr = osSampleRate;
        applied = -1;
        for (auto& c : ch) c = {};
        env = 0.f;
        att = (float) std::exp (-1.0 / (0.002 * sr));
        rel = (float) std::exp (-1.0 / (0.25 * sr));
    }

    void process (juce::dsp::AudioBlock<float>& up, int model, float gain, float tone, float levelDb)
    {
        if (model <= Off || model >= NumModels) return;
        gain = juce::jlimit (0.f, 1.f, gain);
        tone = juce::jlimit (0.f, 1.f, tone);
        if (model != applied || std::abs (gain - aGain) > 0.002f || std::abs (tone - aTone) > 0.002f)
            configure (model, gain, tone);

        const float out = juce::Decibels::decibelsToGain (levelDb) * trim;
        const auto n = up.getNumSamples();
        const int nCh = (int) std::min<size_t> (up.getNumChannels(), 2);

        if (model == Comp)
        {
            // kompresor ze wzmocnieniem „sustain”: detektor szczytowy, ratio ok. 10:1
            for (size_t i = 0; i < n; ++i)
            {
                float pk = 0.f;
                for (int c = 0; c < nCh; ++c) pk = std::max (pk, std::abs (up.getSample (c, (int) i)));
                env = pk > env ? cAtt * env + (1 - cAtt) * pk : rel * env + (1 - rel) * pk;
                const float eDb = 20.f * std::log10 (env + 1e-6f);
                const float over = std::max (0.f, eDb - thrDb);
                const float g = juce::Decibels::decibelsToGain (-over * 0.9f + makeup);
                for (int c = 0; c < nCh; ++c)
                    up.setSample (c, (int) i, ch[c].post1.process (up.getSample (c, (int) i) * g) * out);
            }
            return;
        }

        for (int c = 0; c < nCh; ++c)
        {
            auto& s = ch[c];
            auto* d = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < n; ++i)
            {
                float x = s.inHp.process (d[i]);
                float y = 0.f;
                switch (model)
                {
                    case DS1: case DS2:
                    {
                        // tranzystorowy booster -> wzmacniacz operacyjny -> diody krzemowe (twardo, symetrycznie)
                        x = s.pre.process (x) * g1;
                        x = s.slew.process (x);
                        y = hardClip (x, 0.55f, 6.f);
                        // tone: przenikanie dolnoprzepustowy / górnoprzepustowy -> wycięcie środka
                        const float lo = s.toneLp.process (y), hi = s.toneHp.process (y);
                        y = lo + toneMix * (hi - lo);
                        y = s.post1.process (y);
                        break;
                    }
                    case Rat: case TurboRat:
                    {
                        // gain rosnący z częstotliwością, wolny LM308 przy dużym gainie, diody do masy
                        x = s.pre.process (x) * g1;
                        x = s.slew.process (x);
                        y = model == Rat ? hardClip (x, 0.5f, 5.f) : 1.7f * fastTanh (x / 1.7f);
                        y = s.toneLp.process (y);      // filtr: w prawo ciemniej
                        y = s.post1.process (y);
                        break;
                    }
                    case BigMuff:
                    {
                        x = s.pre.process (x) * g1;
                        x = fastTanh (x + 0.08f) - 0.0798f;           // pierwszy stopień, lekka asymetria
                        x = s.inter.process (x) * g2;
                        y = fastTanh (x - 0.05f) + 0.04996f;          // drugi stopień
                        const float lo = s.toneLp.process (y), hi = s.toneHp.process (y);
                        y = lo + toneMix * (hi - lo);                  // wycięty środek ok. 1 kHz
                        y = s.post1.process (y);
                        break;
                    }
                    case SansAmp:
                    {
                        x = s.pre.process (x) * g1;                    // garb środka przed lampą
                        y = fastTanh (x + 0.15f) - 0.1489f;            // asymetria = parzyste harmoniczne
                        y = s.inter.process (y);
                        y = 1.2f * fastTanh (y * g2 / 1.2f);           // „końcówka”
                        y = s.toneLp.process (s.toneHp.process (y));   // głośnik + high
                        y = s.post1.process (y);
                        break;
                    }
                    case TubeScreamer:
                    {
                        // tylko pasmo nad ok. 720 Hz jest wzmacniane i obcinane; dół przechodzi czysto
                        const float h = s.pre.process (x);
                        y = x + 0.6f * fastTanh (h * g1 / 0.6f);
                        y = s.toneLp.process (y);
                        y = s.post1.process (y);
                        break;
                    }
                    default: y = x; break;
                }
                d[i] = y * out;
            }
        }
    }

private:
    static float hardClip (float x, float level, float hardness)
    {
        // diody: mieszanka twardego obcięcia i tanh (tanie; aliasy tłumi oversampling)
        const float u = x / level;
        const float hard = juce::jlimit (-1.f, 1.f, u);
        const float w = juce::jlimit (0.f, 1.f, (hardness - 2.f) / 5.f);
        return level * (w * hard + (1.f - w) * fastTanh (u));
    }

    void configure (int model, float gain, float tone)
    {
        applied = model; aGain = gain; aTone = tone;
        const double s = sr;
        for (auto& c : ch)
        {
            c.post1.lowpass (s, 12000.0, 0.707);
            switch (model)
            {
                case DS1: case DS2:
                    c.inHp.highpass (s, 100.0, 0.707);
                    c.pre.peak (s, 700.0, 0.8, model == DS2 ? 6.0 : 0.0);   // Turbo: garb środka
                    c.slew.lowpass (s, 9000.0, 0.707);
                    c.toneLp.lowpass (s, 700.0, 0.6);
                    c.toneHp.highpass (s, 1300.0, 0.6);
                    c.post1.lowpass (s, 7500.0, 0.707);
                    break;
                case Rat: case TurboRat:
                    c.inHp.highpass (s, 60.0, 0.707);
                    c.pre.highshelf (s, 1500.0, 12.0 * gain);                 // gain rośnie od ok. 1,5 kHz
                    c.slew.lowpass (s, 22000.0 / (1.0 + 6.0 * gain), 0.707);  // LM308: przy dużym gainie ciemniej
                    c.toneLp.lowpass (s, 500.0 * std::pow (60.0, (double) tone), 0.707);
                    c.post1.lowpass (s, 11000.0, 0.707);
                    break;
                case BigMuff:
                    c.inHp.highpass (s, 80.0, 0.707);
                    c.pre.highshelf (s, 1000.0, -2.0);
                    c.inter.highpass (s, 110.0, 0.707);
                    c.toneLp.lowpass (s, 420.0, 0.6);
                    c.toneHp.highpass (s, 1800.0, 0.6);
                    c.post1.lowpass (s, 8000.0, 0.707);
                    break;
                case SansAmp:
                    c.inHp.highpass (s, 90.0, 0.707);
                    c.pre.peak (s, 900.0, 0.7, 5.0);
                    c.inter.lowpass (s, 6500.0, 0.707);
                    c.toneHp.highpass (s, 70.0, 0.707);
                    c.toneLp.lowpass (s, 2500.0 + 6000.0 * tone, 0.8);
                    c.post1.peak (s, 120.0, 1.0, 2.0);
                    break;
                case TubeScreamer:
                    c.inHp.highpass (s, 30.0, 0.707);
                    c.pre.highpass (s, 720.0, 0.707);
                    c.toneLp.lowpass (s, 800.0 * std::pow (6.0, (double) tone), 0.707);
                    c.post1.lowpass (s, 9000.0, 0.707);
                    break;
                case Comp:
                    c.post1.highshelf (s, 3000.0, -1.5);   // CS-2 lekko przyciemnia
                    break;
                default: break;
            }
        }

        const float dB = [&]
        {
            switch (model)
            {
                case DS1:          return 6.f + 34.f * gain;
                case DS2:          return 9.f + 36.f * gain;
                case Rat:          return 6.f + 44.f * gain;
                case TurboRat:     return 6.f + 44.f * gain;
                case BigMuff:      return 4.f + 22.f * gain;
                case SansAmp:      return 3.f + 30.f * gain;
                case TubeScreamer: return 3.f + 38.f * gain;
                default:           return 0.f;
            }
        }();
        g1 = juce::Decibels::decibelsToGain (dB);
        g2 = model == BigMuff ? juce::Decibels::decibelsToGain (6.f + 20.f * gain)
           : model == SansAmp ? juce::Decibels::decibelsToGain (3.f + 6.f * gain) : 1.f;
        toneMix = tone;

        // wyrównanie głośności: każda kostka przy gain 0,5 mniej więcej tak głośno jak sygnał wejściowy
        switch (model)
        {
            case DS1:            trim = 0.39f; break;
            case DS2:            trim = 0.35f; break;
            case Rat:            trim = 0.18f; break;
            case TurboRat:       trim = 0.10f; break;
            case BigMuff:        trim = 0.25f; break;
            case SansAmp:        trim = 0.14f; break;
            case TubeScreamer:   trim = 0.42f; break;
            case Comp:           trim = 0.5f;  break;
            default:             trim = 1.f;   break;
        }

        if (model == Comp)
        {
            thrDb = -12.f - 28.f * gain;        // sustain: niżej próg
            makeup = 0.9f * (-thrDb) * 0.6f;
            cAtt = (float) std::exp (-1.0 / ((0.0005 + 0.02 * (1.0 - tone)) * sr));
        }
    }

    struct Ch { DBiquad inHp, pre, slew, inter, toneLp, toneHp, post1; };
    double sr = 192000.0;
    Ch ch[2];
    int applied = -1;
    float aGain = -1.f, aTone = -1.f, g1 = 1.f, g2 = 1.f, toneMix = 0.5f, trim = 1.f;
    float env = 0.f, att = 0.f, rel = 0.f, cAtt = 0.f, thrDb = -20.f, makeup = 0.f;
};
