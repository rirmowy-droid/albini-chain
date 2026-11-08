#pragma once

#include "DSPUtils.h"
#include <array>
#include <vector>

/** A live room, not a "reverb plate": a handful of early reflections plus a
    short, darkish 4-line FDN tail. Think room mics a few metres back in a
    wooden tracking room. Wet path is high-passed so the room never muddies
    the close sound. Parallel: out = dry + mix * room. */
class SimpleRoom
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    void setMix (float mix)   { mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mix)); }
    void setSize (float size) { sizeTarget = juce::jlimit (0.0f, 1.0f, size); sizeSmoothed.setTargetValue (sizeTarget); }

    /** Zgniecenie mikrofonów sali (mocna kompresja samego pogłosu sali, jak przy bębnach). */
    void setCrush (float c)   { crush = juce::jlimit (0.0f, 1.0f, c); }
    /** Dolna granica sygnału, który trafia do sali (bębny niżej, wokal wyżej). */
    void setInputHighpass (float hz) { inputHpHz = juce::jlimit (20.0f, 1000.0f, hz); }
    /** Opóźnienie mikrofonów sali (Albini: ok. 10-20 ms) - bębny brzmią „dalej” i większe. */
    void setPredelay (float ms) { predelayTarget = juce::jlimit (0.0f, maxPredelayMs, ms); }
    static constexpr float maxPredelayMs = 40.0f;

    static constexpr double maxTailSeconds = 2.0;

private:
    struct Delay
    {
        std::vector<float> buf;
        int w = 0;

        void init (int size) { buf.assign ((size_t) size, 0.0f); w = 0; }
        void clear()         { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x)  { buf[(size_t) w] = x; if (++w == (int) buf.size()) w = 0; }

        float read (float delaySamples) const // delay >= 1
        {
            const int size = (int) buf.size();
            float pos = (float) w - delaySamples;
            while (pos < 0.0f) pos += (float) size;
            const int i0 = (int) pos;
            const int i1 = (i0 + 1) % size;
            const float f = pos - (float) i0;
            return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
    };

    static constexpr int numLines = 4;
    static constexpr int numTaps  = 6;

    void updateDecay (float size);

    double sampleRate = 44100.0;

    Delay er;                                  // shared early-reflection line
    std::array<Delay, numLines> lines;
    std::array<albini::OnePole, numLines> damping;
    std::array<float, numLines> feedback {};

    albini::OnePole inputHP, inputLP;

    juce::SmoothedValue<float> mixSmoothed { 0.12f }, sizeSmoothed { 0.35f };
    float sizeTarget = 0.35f, lastDecaySize = -1.0f;
    float crush = 0.0f, crushEnv = 0.0f;
    float inputHpHz = 180.0f, lastInputHpHz = -1.0f;
    float predelayTarget = 0.0f;
    juce::SmoothedValue<float> predelaySmoothed { 0.0f };
};
