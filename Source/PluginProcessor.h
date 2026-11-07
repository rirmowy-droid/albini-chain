#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/AmpStage.h"
#include "DSP/CabinetStage.h"
#include "DSP/SimpleRoom.h"
#include "DSP/ConsoleSaturation.h"
#include "DSP/TapeEmulation.h"

/** Amp -> Cab (2 IRs, blend) -> Room -> Console -> Tape -> Output */
class AlbiniChainAudioProcessor : public juce::AudioProcessor
{
public:
    AlbiniChainAudioProcessor();
    ~AlbiniChainAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Albini Chain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return SimpleRoom::maxTailSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- cabinet IRs (message thread) ---------------------------------------
    bool loadCabIR (int slot, const juce::File& file);
    void clearCabIR (int slot);
    juce::String getCabIRDisplayName (int slot) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::Identifier irProperty (int slot);
    void restoreCabIRsFromState();
    void processChunk (juce::dsp::AudioBlock<float>& block);

    AmpStage amp;
    CabinetStage cab;
    SimpleRoom room;
    ConsoleSaturation console;
    TapeEmulation tape;
    juce::dsp::Gain<float> outputGain;

    int maxBlockSize = 512;

    std::atomic<float>* pInputGain  = nullptr;
    std::atomic<float>* pDrive      = nullptr;
    std::atomic<float>* pCabBlend   = nullptr;
    std::atomic<float>* pRoomMix    = nullptr;
    std::atomic<float>* pRoomSize   = nullptr;
    std::atomic<float>* pConsole    = nullptr;
    std::atomic<float>* pTape       = nullptr;
    std::atomic<float>* pOutputGain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlbiniChainAudioProcessor)
};
