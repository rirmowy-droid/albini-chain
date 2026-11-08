#pragma once

#include "DSPUtils.h"
#include "../Stonoga/StonogaDSP.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

/** Dwa sloty IR kolumny (np. SM57 z bliska + wstęgowy) z blendem.

    - Oba IR: liniowe przenikanie A -> B (jak dwa suwaki mikrofonów).
      Z włączonym wyrównaniem fazy IR B jest przesuwany (do ±2 ms) i w razie
      potrzeby odwracany tak, żeby najlepiej pokrywał się z A: mniej „dziur”
      (filtra grzebieniowego) przy mieszaniu.
    - Jeden IR: tylko on.
    - Brak IR: prosta krzywa głośnika 4x12 (albo 8x10 dla basu).

    Za kolumną: model mikrofonu (różnica względem SM57, którym zwykle nagrane są IR),
    pozycja na membranie (środek = jasno, brzeg = ciemno) i odległość
    (z bliska = efekt zbliżeniowy, dalej = mniej dołu i odbicie od podłogi).

    loadIR()/clearIR()/setAlign() wołać poza wątkiem audio (UI / timer). */
class CabinetStage
{
public:
    enum Mic { MicAsIR = 0, Mic57, Mic421, MicRibbon, MicCondenser, NumMics };

    CabinetStage();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    bool loadIR (int slot, const juce::File& file);
    /** IR z pamięci (wbudowane), przycięte do maxSeconds - bez pogłosu pomieszczenia. */
    bool loadIRFromMemory (int slot, const void* data, size_t bytes, double maxSeconds = 0.2);
    void clearIR (int slot);
    bool hasIR (int slot) const { return loaded[slot & 1].load(); }

    /** Wyrównanie fazy B do A (wątek wiadomości). */
    void setAlign (bool on);
    /** Ostatnio zastosowane przesunięcie B w próbkach (+ = B był spóźniony) i odwrócenie. */
    int getAlignShift() const { return alignShift.load(); }
    bool getAlignFlipped() const { return alignFlip.load(); }

    void setBlend (float blend01) { blend = juce::jlimit (0.0f, 1.0f, blend01); }

    /** Wbudowana kolumna: gitarowa 4x12 albo basowa 8x10. */
    void setBassVoicing (bool bass) { bassVoicing = bass; }

    void setMic (int m, float position01, float distance01);

private:
    bool readIR (const juce::File* file, const void* data, size_t bytes, double maxSeconds, juce::AudioBuffer<float>& out, double& rate);
    void pushToConvolution (int slot);
    static void computeAlignment (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, double rate, int& shift, bool& flip);

    juce::dsp::Convolution conv[2];
    std::atomic<bool> loaded[2] { false, false };
    bool wasLoaded[2] { false, false };

    // surowe IR (wątek wiadomości) - potrzebne do wyrównania fazy
    juce::AudioBuffer<float> raw[2];
    double rawRate[2] { 48000.0, 48000.0 };
    bool hasRaw[2] { false, false };
    bool align = true;
    std::atomic<int> alignShift { 0 };
    std::atomic<bool> alignFlip { false };
    juce::AudioFormatManager formats;

    float blend = 0.5f;
    juce::SmoothedValue<float> gain[2];

    juce::AudioBuffer<float> scratch;

    // Fallback speaker curve
    albini::StereoIIR fbHighPass, fbPeak, fbLowPass1, fbLowPass2;

    // mikrofon za kolumną
    int micModel = MicAsIR;
    float micPos = 0.5f, micDist = 0.3f;
    int appliedMic = -1;
    float appliedPos = -1.f, appliedDist = -1.f;
    bool micActive = false;
    stonoga::Biquad micA[2], micB[2], micC[2], posF[2], proxF[2], bounceLp[2];
    stonoga::Delay bounce[2];
    float bounceDelay = 0.f, bounceGain = 0.f;
    void updateMicFilters();
    void processIRs (juce::dsp::AudioBlock<float>& block);

    double sampleRate = 44100.0;
    bool bassVoicing = false, appliedBassVoicing = false;
    void applyFallbackVoicing();
};
