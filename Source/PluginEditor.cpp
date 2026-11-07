#include "PluginEditor.h"

namespace
{
    struct KnobDef { const char* paramId; const char* label; };
    constexpr KnobDef knobDefs[] = {
        { "inputGain",  "Input"   },
        { "drive",      "Drive"   },
        { "cabBlend",   "Cab A/B" },
        { "roomMix",    "Room"    },
        { "roomSize",   "Size"    },
        { "console",    "Console" },
        { "tape",       "Tape"    },
        { "outputGain", "Output"  },
    };

    const juce::Colour bg     { 0xff1a1a1a };
    const juce::Colour panel  { 0xff242424 };
    const juce::Colour accent { 0xffd9c9a3 };   // old masking-tape beige
}

AlbiniChainAudioProcessorEditor::AlbiniChainAudioProcessorEditor (AlbiniChainAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lnf.setColour (juce::Slider::rotarySliderFillColourId, accent);
    lnf.setColour (juce::Slider::thumbColourId, accent);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setLookAndFeel (&lnf);

    for (int i = 0; i < numKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
        addAndMakeVisible (k.slider);

        k.label.setText (knobDefs[i].label, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, accent);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, knobDefs[i].paramId, k.slider);
    }

    for (int s = 0; s < 2; ++s)
    {
        auto& slot = irSlots[(size_t) s];
        slot.load.setButtonText (s == 0 ? "Load IR A" : "Load IR B");
        slot.load.onClick  = [this, s] { chooseIR (s); };
        slot.clear.onClick = [this, s] { processor.clearCabIR (s); timerCallback(); };
        slot.name.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        slot.name.setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (slot.load);
        addAndMakeVisible (slot.clear);
        addAndMakeVisible (slot.name);
    }

    lastIRFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    setSize (780, 300);
    timerCallback();
    startTimerHz (4);
}

AlbiniChainAudioProcessorEditor::~AlbiniChainAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AlbiniChainAudioProcessorEditor::chooseIR (int slot)
{
    chooser = std::make_unique<juce::FileChooser> (slot == 0 ? "Load cabinet IR (A)" : "Load cabinet IR (B)",
                                                   lastIRFolder, "*.wav;*.aif;*.aiff;*.flac");

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    juce::Component::SafePointer<AlbiniChainAudioProcessorEditor> safe (this);
    chooser->launchAsync (chooserFlags, [safe, slot] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        const auto file = fc.getResult();
        if (file.existsAsFile())
        {
            safe->lastIRFolder = file.getParentDirectory();
            safe->processor.loadCabIR (slot, file);
        }
        safe->timerCallback();
    });
}

void AlbiniChainAudioProcessorEditor::timerCallback()
{
    for (int s = 0; s < 2; ++s)
    {
        const auto text = processor.getCabIRDisplayName (s);
        auto& label = irSlots[(size_t) s].name;
        if (label.getText() != text)
        {
            label.setText (text, juce::dontSendNotification);
        }
    }
}

void AlbiniChainAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    auto header = getLocalBounds().removeFromTop (40);
    g.setColour (accent);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("ALBINI CHAIN", header.removeFromLeft (220).withTrimmedLeft (16), juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("amp > cab > room > console > tape", header.withTrimmedRight (16), juce::Justification::centredRight);

    // IR strip background
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().removeFromBottom (48).reduced (10, 6).toFloat(), 4.0f);
}

void AlbiniChainAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (40);

    // --- IR strip ---
    auto strip = area.removeFromBottom (48).reduced (16, 12);
    const int half = strip.getWidth() / 2;
    for (int s = 0; s < 2; ++s)
    {
        auto r = (s == 0 ? strip.removeFromLeft (half) : strip).reduced (4, 0);
        auto& slot = irSlots[(size_t) s];
        slot.load.setBounds (r.removeFromLeft (84));
        slot.clear.setBounds (r.removeFromRight (24));
        slot.name.setBounds (r.reduced (6, 0));
    }

    // --- knobs ---
    area = area.reduced (10, 4);
    const int w = area.getWidth() / numKnobs;
    for (int i = 0; i < numKnobs; ++i)
    {
        auto col = (i == numKnobs - 1) ? area : area.removeFromLeft (w);
        auto& k = knobs[(size_t) i];
        k.label.setBounds (col.removeFromTop (20));
        k.slider.setBounds (col.reduced (4, 2));
    }
}
