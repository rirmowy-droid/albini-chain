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
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        juce::Colour accent;
        bool paper = false;
    };

    void timerCallback() override;
    void chooseIR (int slot);
    void updateVisibility();
    Knob* addKnob (juce::OwnedArray<Knob>& list, const char* paramId, const juce::String& label, bool onPaper);
    void layoutKnobs (juce::OwnedArray<Knob>& list, juce::Rectangle<int> area, int firstCellOffset = 0);
    void drawPanel (juce::Graphics& g, const Panel& p);

    CombinedProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    juce::OwnedArray<Knob> pedalKnobs, ampKnobs, srcKnobs, studioKnobs, busKnobs, outKnobs;

    juce::ComboBox sourceBox, presetBox, styleBox, micBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAtt, styleAtt, micAtt;
    juce::TextButton autoMixButton { "AUTO-MIKS" };
    juce::Label statusLabel, micLabel, noPedalLabel;

    juce::TextButton footswitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> footswitchAtt;

    struct IRSlot { juce::TextButton load, clear { "x" }; juce::Label name; };
    std::array<IRSlot, 2> irSlots;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastIRFolder;

    Panel pedalPanel, ampPanel, srcPanel, studioPanel, busPanel, outPanel, irPanel;
    int lastSource = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedEditor)
};
