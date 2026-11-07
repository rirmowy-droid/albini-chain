#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"

class AlbiniChainAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit AlbiniChainAudioProcessorEditor (AlbiniChainAudioProcessor&);
    ~AlbiniChainAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseIR (int slot);

    AlbiniChainAudioProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    static constexpr int numKnobs = 8;
    std::array<Knob, numKnobs> knobs;

    struct IRSlot
    {
        juce::TextButton load, clear { "x" };
        juce::Label name;
    };
    std::array<IRSlot, 2> irSlots;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastIRFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlbiniChainAudioProcessorEditor)
};
