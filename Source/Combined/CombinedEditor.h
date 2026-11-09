#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CombinedProcessor.h"
#include "MixerView.h"

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
    void rebuildPresetList();
    void syncPresetBox();
    void presetChosen (int id);
    juce::Array<juce::File> userPresets;

    CombinedProcessor& proc;
    juce::LookAndFeel_V4 lnf;
    int page = 0;

    // --- strona 1: tor ---
    juce::OwnedArray<Knob> cabKnobs;
    juce::ComboBox tapeBox, tapeSpeedBox;
    std::unique_ptr<ComboAtt> tapeAtt, tapeSpeedAtt;
    juce::Label tapeLabel;
    juce::ComboBox ampModelBox, cabMicBox;
    std::unique_ptr<ComboAtt> ampModelAtt, cabMicAtt;
    juce::Label ampModelLabel, cabMicLabel;
    juce::ToggleButton hqButton, alignButton, dblButton;
    std::unique_ptr<ButtonAtt> hqAtt, alignAtt, dblAtt;
    Panel drivePanel;
    juce::OwnedArray<Knob> driveKnobs;
    juce::ComboBox driveBox;
    std::unique_ptr<ComboAtt> driveAtt;
    juce::Label driveLabel;
    Panel cabPanel, bassPanel, drumPanel, vocalPanel, mixerPanel, chanPanel;
    juce::OwnedArray<Knob> bassKnobs, drumKnobs, vocalKnobs, chanKnobs;
    std::unique_ptr<MixerView> mixer;
    juce::ToggleButton chanMute, chanSolo;
    std::unique_ptr<ButtonAtt> chanMuteAtt, chanSoloAtt;
    juce::Label chanInfo;
    juce::OwnedArray<Knob> pedalKnobs, ampKnobs, srcKnobs, studioKnobs, busKnobs, outKnobs;
    juce::ComboBox micBox, consoleBox;
    std::unique_ptr<ComboAtt> micAtt, consoleAtt;
    juce::Label micLabel, consoleLabel, noPedalLabel;
    juce::TextButton footswitch;
    std::unique_ptr<ButtonAtt> footswitchAtt;
    struct IRSlot { juce::TextButton load, clear { "x" }; juce::Label name; juce::ComboBox choose; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att; };
    std::array<IRSlot, 2> irSlots;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastIRFolder;
    Panel pedalPanel, ampPanel, srcPanel, studioPanel, busPanel, outPanel, irPanel;

    // --- strona 2: kompresor i miks ---
    juce::OwnedArray<Knob> compKnobs, duckKnobs, eqKnobs;
    juce::ComboBox compModeBox, compCharBox, busCharBox;
    std::unique_ptr<ComboAtt> compModeAtt, compCharAtt, busCharAtt;
    juce::Label compModeLabel, compCharLabel, busCharLabel, compInfo, eqInfo;
    juce::ToggleButton lowOnlyButton, allButtonsButton;
    std::unique_ptr<ButtonAtt> lowOnlyAtt, allButtonsAtt;
    Panel compPanel, duckPanel, eqPanel;
    juce::Rectangle<int> grMeter, kickMeter, vocalMeter;

    // --- strona 2 przy źródle „Cały miks”: klej i auto-korektor sumy ---
    juce::OwnedArray<Knob> glueKnobs, masterKnobs;
    juce::ComboBox busModeBox;
    std::unique_ptr<ComboAtt> busModeAtt;
    juce::Label busModeLabel, masterInfo;
    juce::ToggleButton trackButton;
    std::unique_ptr<ButtonAtt> trackAtt;
    Panel gluePanel, masterEqPanel;
    juce::Rectangle<int> busGrMeter, harshMeter, curveArea;
    std::vector<juce::Component*> page2Track, page2Master;
    void drawCurve (juce::Graphics& g);

    // --- nagłówek ---
    juce::ComboBox sourceBox, presetBox, styleBox;
    std::unique_ptr<ComboAtt> sourceAtt, styleAtt;
    juce::TextButton autoMixButton { "AUTO-MIKS" };
    juce::TextButton undoButton;
    juce::TextButton pageButtons[2];
    juce::Label statusLabel;

    std::vector<juce::Component*> page1, page2;
    int lastSource = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombinedEditor)
};
