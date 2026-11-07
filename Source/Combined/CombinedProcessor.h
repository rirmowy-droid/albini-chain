#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "ChainEngine.h"
#include "AutoMix.h"

/** Stonoga x Albini 2: kanał studyjny w stylu Albiniego dla gitary, basu,
    perkusji, wokalu i całego miksu, z kostką Stonoga i auto-miksem. */
class CombinedProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    CombinedProcessor();
    ~CombinedProcessor() override;

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
    double getTailLengthSeconds() const override { return 6.0; }

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

    // --- auto-miks (wątek wiadomości) ---------------------------------------
    void requestAutoMix();
    juce::String getAutoMixStatus() const { return autoStatus; }
    int getSource() const { return (int) std::round (paramValue ("source")); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::Identifier irProperty (int slot);
    void restoreCabIRsFromState();
    void setParam (const char* id, float value);
    float paramValue (const char* id) const;
    void readSettings (sxa::Settings& s, stonoga::Params& p) const;
    void timerCallback() override;
    sxa::MixContext mixContext() const;

    ChainEngine engine;
    int maxBlockSize = 512;
    int currentProgram = 0;

    std::array<std::atomic<float>*, sxa::kNumParams> params {};
    std::array<std::atomic<float>*, sxa::kNumPedal> pedalParams {};

    // auto-miks
    enum class AM { Idle, Listening, WaitingForTracks, ListeningMix };
    AM amState = AM::Idle;
    int registrySlot = -1, seenGeneration = 0, amStyle = 0;
    double amStart = 0.0;
    juce::String autoStatus { juce::String::fromUTF8 ("Auto-miks: wybierz styl i kliknij") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedProcessor)
};
