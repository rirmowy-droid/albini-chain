#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "../DSP/AmpStage.h"
#include "../DSP/CabinetStage.h"
#include "../DSP/SimpleRoom.h"
#include "../DSP/ConsoleSaturation.h"
#include "../DSP/TapeEmulation.h"
#include "../Stonoga/StonogaDSP.h"
#include "Settings.h"
#include "ToneStage.h"
#include "EnsembleChorus.h"
#include "SpaceSmear.h"
#include "Compressor.h"
#include "EarGuard.h"
#include "DuckStage.h"
#include "SlapEcho.h"

/** Cały tor Stonoga x Albini, niezależny od wtyczki (używany też w testach).

    [gitara/bas]  kostka (4x) -> wzmacniacz -> kolumna (2 IR)   (+ DI dołu dla basu)
    barwa (mikrofon, źródło, obcięcie dołu, miejsce na wokal, ciemno/jasno)
    na źródle:    kompresja -> chorus -> panorama (dwa podejścia) -> pogłos
    studio:       sala (+ zgniecenie) -> konsoleta -> taśma
    na całości:   kompresja -> panorama M/S -> pogłos
    [pomiar dla auto-miksu]
    wyjście:      głośność auto + wyjście -> pan -> ochrona uszu

    Opóźnienie jest stałe niezależnie od źródła (etapy z oversamplingiem
    zawsze przepuszczają sygnał). */
class ChainEngine
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels);
    float getLatencyInSamples() const;

    /** Przetwarza fragment nie dłuższy niż maxBlock. */
    void process (juce::dsp::AudioBlock<float>& block, const sxa::Settings& s, const stonoga::Params& pedalIn);

    // --- ustępowanie: wejście od innych kopii, wyjście dla innych -------------
    void setSidechain (float kickNorm, float vocalNorm) { scKick = kickNorm; scVocal = vocalNorm; }
    float getPublishedKick() const  { return pubKick.load(); }
    float getPublishedVocal() const { return pubVocal.load(); }

    // --- wskaźniki ---------------------------------------------------------------
    float getCompReductionDb() const   { return srcComp.getGainReductionDb(); }
    float getKickDuckDb() const        { return duck.getKickDuckDb(); }
    float getVocalDuckDb() const       { return duck.getVocalDuckDb(); }

    CabinetStage& cabinet() { return cab; }

    // --- pomiar głośności dla auto-miksu (przed suwakiem) -------------------
    void startMeasuring()  { measureReset = true; measuring = true; }
    void stopMeasuring()   { measuring = false; }
    float getMeasuredDb() const;
    float getMeasuredSeconds() const { return activeSeconds.load(); }
    /** Widmo z pomiaru: energia w 8 pasmach (dB), jak na wyjściu przed suwakiem. */
    void getMeasuredBandsDb (float* out8) const;

private:
    double sr = 48000.0;
    int maxBlockSize = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> pedalOS;
    stonoga::Engine pedal;
    juce::AudioBuffer<float> osDry, diBuf;
    juce::SmoothedValue<float> pedalMix;
    std::array<stonoga::Biquad, 2> diLp;

    AmpStage amp;
    CabinetStage cab;
    ToneStage tone;
    Compressor srcComp, busComp;
    EnsembleChorus chorus;
    SpaceSmear srcSpace, busSpace;
    SimpleRoom room;
    ConsoleSaturation console;
    TapeEmulation tape;
    EarGuard guard;
    DuckStage duck;
    SlapEcho slap;
    float scKick = 0.f, scVocal = 0.f;
    std::atomic<float> pubKick { 0.f }, pubVocal { 0.f };
    stonoga::Biquad bandBp[sxa::kNumEqBands];
    double bandSq[sxa::kNumEqBands] {};
    std::array<std::atomic<float>, sxa::kNumEqBands> bandMs {};

    juce::SmoothedValue<float> faderGain, panL, panR;

    // pomiar (K-ważenie w przybliżeniu)
    stonoga::Biquad kHp[2], kShelf[2];
    std::atomic<bool> measuring { false }, measureReset { false };
    double sumSq = 0.0; long long counted = 0;
    std::atomic<float> measuredMs { 0.f }, activeSeconds { 0.f };
};
