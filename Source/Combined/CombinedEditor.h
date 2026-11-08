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

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void chooseIR (int slot);
    void updateVisibility (bool force = false);
    Knob* addKnob (juce::OwnedArray<Knob>& list, const char* paramId, const juce::String& label, bool onPaper, bool vertical = false);
    void layoutKnobs (juce::OwnedArray<Knob>& list, juce::Rectangle<int> area, int firstCellOffset = 0);
    void layoutCombo (juce::ComboBox& box, juce::Label& label, juce::Rectangle<int> cell);
    void drawPanel (juce::Graphics& g, const Panel& p);
    void drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float db, float rangeDb, const juce::String& caption, juce::Colour c);
    void setPage (int page);

    CombinedProcessor& proc;
    juce::LookAndFeel_V4 lnf;
    int page = 0;

    // --- strona 1: tor ---
    juce::OwnedArray<Knob> pedalKnobs, ampKnobs, srcKnobs, studioKnobs, busKnobs, outKnobs;
    juce::ComboBox micBox;
    std::unique_ptr<ComboAtt> micAtt;
    juce::Label micLabel, noPedalLabel;
    juce::TextButton footswitch;
    std::unique_ptr<ButtonAtt> footswitchAtt;
    struct IRSlot { juce::TextButton load, clear { "x" }; juce::Label name; };
    std::array<IRSlot, 2> irSlots;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastIRFolder;
    Panel pedalPanel, ampPanel, srcPanel, studioPanel, busPanel, outPanel, irPanel;

    // --- strona 2: kompresor i miks ---
    juce::OwnedArray<Knob> compKnobs, duckKnobs, eqKnobs;
    juce::ComboBox compModeBox, compCharBox;
    std::unique_ptr<ComboAtt> compModeAtt, compCharAtt;
    juce::Label compModeLabel, compCharLabel, compInfo, eqInfo;
    juce::ToggleButton lowOnlyButton;
    std::unique_ptr<ButtonAtt> lowOnlyAtt;
    Panel compPanel, duckPanel, eqPanel;
    juce::Rectangle<int> grMeter, kickMeter, vocalMeter;

    // --- nagłówek ---
    juce::ComboBox sourceBox, presetBox, styleBox;
    std::unique_ptr<ComboAtt> sourceAtt, styleAtt;
    juce::TextButton autoMixButton { "AUTO-MIKS" };
    juce::TextButton pageButtons[2];
    juce::Label statusLabel;

    std::vector<juce::Component*> page1, page2;
    int lastSource = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedEditor)
};
