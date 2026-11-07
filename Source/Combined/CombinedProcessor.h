#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "../DSP/AmpStage.h"
#include "../DSP/CabinetStage.h"
#include "../DSP/SimpleRoom.h"
#include "../DSP/ConsoleSaturation.h"
#include "../DSP/TapeEmulation.h"
#include "../Stonoga/StonogaDSP.h"
#include "EarGuard.h"
#include "ChoirStage.h"
#include "SpaceSmear.h"
#include "Compressor.h"

/** Stonoga x Albini:
    Kostka Stonoga (4x) -> Amp -> Cab (2 IR)
    -> [na przesterze: kompresja -> panorama/pogłos]
    -> Chór -> Room -> Console -> Tape
    -> [na całości: kompresja -> panorama/pogłos] -> Output -> Ochrona uszu */
class CombinedProcessor : public juce::AudioProcessor
{
public:
    CombinedProcessor();
    ~CombinedProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Stonoga x Albini"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- IR kolumny (wątek wiadomości) ------------------------------------
    bool loadCabIR (int slot, const juce::File& file);
    void clearCabIR (int slot);
    juce::String getCabIRDisplayName (int slot) const;

    // --- opis parametrów dla edytora --------------------------------------
    static constexpr int kNumPedalParams = 28;
    static const char* pedalParamId (int i);
    static constexpr int kNumChainParams = 9;
    static const char* chainParamId (int i);
    static const char* chainParamShortName (int i);
    static constexpr int kNumBusParams = 6;      // 3 na przesterze + 3 na całości
    static const char* busParamId (int i);
    static const char* busParamShortName (int i);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::Identifier irProperty (int slot);
    void restoreCabIRsFromState();
    void processChunk (juce::dsp::AudioBlock<float>& block);
    void setParam (const char* id, float value);

    // kostka
    std::unique_ptr<juce::dsp::Oversampling<float>> pedalOS;
    stonoga::Engine pedal;
    juce::AudioBuffer<float> osDry;
    juce::SmoothedValue<float> pedalMix;

    // łańcuch
    AmpStage amp;
    CabinetStage cab;
    SimpleRoom room;
    ConsoleSaturation console;
    TapeEmulation tape;
    Compressor distComp, busComp;
    SpaceSmear distSpace, space;
    ChoirStage choir;
    juce::dsp::Gain<float> outputGain;
    EarGuard guard;

    int maxBlockSize = 512;
    int currentProgram = 0;

    std::array<std::atomic<float>*, kNumPedalParams> pedalParams {};
    std::atomic<float>* pPedalOn = nullptr;
    std::atomic<float>* pAmpInput = nullptr;
    std::atomic<float>* pAmpDrive = nullptr;
    std::atomic<float>* pCabBlend = nullptr;
    std::atomic<float>* pRoomMix = nullptr;
    std::atomic<float>* pRoomSize = nullptr;
    std::atomic<float>* pConsole = nullptr;
    std::atomic<float>* pTape = nullptr;
    std::atomic<float>* pOutputGain = nullptr;
    std::atomic<float>* pProtect = nullptr;
    std::atomic<float>* pSmear = nullptr;
    std::atomic<float>* pDistComp = nullptr;
    std::atomic<float>* pDistWidth = nullptr;
    std::atomic<float>* pDistSmear = nullptr;
    std::atomic<float>* pBusComp = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pChoir = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedProcessor)
};
