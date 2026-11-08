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

    // --- własne presety (pliki w Dokumentach) -----------------------------
    static juce::File getUserPresetFolder();
    static juce::Array<juce::File> getUserPresetFiles();
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getUserPresetName() const { return userPresetName; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- IR kolumny (wątek wiadomości) ------------------------------------
    bool loadCabIR (int slot, const juce::File& file);
    bool loadBundledIR (int slot, int index);
    void clearCabIR (int slot);
    juce::String getCabIRDisplayName (int slot) const;

    // --- auto-miks (wątek wiadomości) ---------------------------------------
    void requestAutoMix();
    void requestUndoAutoMix() { sxa::MixRegistry::get().requestUndo(); }
    bool canUndoAutoMix() const { return hasSnapshot; }
    juce::String getAutoMixStatus() const { return autoStatus; }
    int getSource() const { return (int) std::round (paramValue ("source")); }

    // --- wskaźniki dla UI ---------------------------------------------------
    float getCompReductionDb() const { return engine.getCompReductionDb(); }
    float getKickDuckDb() const      { return engine.getKickDuckDb(); }
    float getVocalDuckDb() const     { return engine.getVocalDuckDb(); }
    float getBusReductionDb() const  { return engine.getBusReductionDb(); }
    float getDeHarshDb() const       { return engine.getDeHarshDb(); }
    void getAlbumGains (float* out) const { const_cast<ChainEngine&> (engine).albumEq().getGains (out); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::Identifier irProperty (int slot);
    void restoreCabIRsFromState();
    void setAlbumGains (const float* gains);     // nullptr = wyzeruj
    void restoreAlbumFromState();
    void setParam (const char* id, float value);
    float paramValue (const char* id) const;
    void readSettings (sxa::Settings& s, stonoga::Params& p) const;
    void timerCallback() override;
    sxa::MixContext mixContext() const;

    ChainEngine engine;
    int maxBlockSize = 512;
    double preparedRate = 0.0;
    int currentProgram = 0;
    juce::String userPresetName;

    std::array<std::atomic<float>*, sxa::kNumParams> params {};
    std::array<std::atomic<float>*, sxa::kNumPedal> pedalParams {};

    // auto-miks
    enum class AM { Idle, Listening, WaitingForPeers, WaitingForTracks, ListeningMix, ListeningMix2 };
    AM amState = AM::Idle;
    int registrySlot = -1, seenGeneration = 0, amStyle = 0;
    double amStart = 0.0, amPeersStart = 0.0;
    std::array<float, sxa::kNumEqBands> myBands {};
    std::array<float, AlbumEq::numBands> albumTarget {};
    int trackTicks = 0;
    std::array<int, 2> loadedCab { -1, -1 };
    juce::ValueTree snapshot;
    bool hasSnapshot = false;
    int seenUndo = 0;
    juce::String lastGainText;
    static int eqParamIndex (int band);
    juce::String autoStatus { juce::String::fromUTF8 ("Auto-miks: wybierz styl i kliknij") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedProcessor)
};
