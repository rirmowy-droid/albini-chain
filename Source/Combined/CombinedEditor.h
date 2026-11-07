#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CombinedProcessor.h"

class CombinedEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit CombinedEditor (CombinedProcessor&);
    ~CombinedEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseIR (int slot);
    void updateFootswitchText();

    CombinedProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    juce::OwnedArray<Knob> pedalKnobs, chainKnobs, busKnobs;

    juce::ComboBox presets;
    juce::TextButton footswitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> footswitchAttachment;

    struct IRSlot
    {
        juce::TextButton load, clear { "x" };
        juce::Label name;
    };
    std::array<IRSlot, 2> irSlots;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastIRFolder;

    juce::Rectangle<int> pedalPanel, chainPanel, distGroup, busGroup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedEditor)
};
