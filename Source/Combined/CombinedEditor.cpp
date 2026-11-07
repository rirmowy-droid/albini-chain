#include "CombinedEditor.h"

namespace
{
    // Stonoga: drzeworyt (papier, czerń, czerwona pieczęć)
    const juce::Colour paper   { 0xffebdfc4 };
    const juce::Colour ink     { 0xff1b1a1a };
    const juce::Colour seal    { 0xffc23b2a };
    const juce::Colour indigo  { 0xff22385e };
    // Albini: ciemne studio, beżowa taśma maskująca
    const juce::Colour studio  { 0xff1a1a1a };
    const juce::Colour panel   { 0xff242424 };
    const juce::Colour tapeCol { 0xffd9c9a3 };

    constexpr int pedalCols = 10;
    constexpr int headerH   = 64;

    juce::String pl (const char* s) { return juce::String::fromUTF8 (s); }

    void styleKnob (juce::Slider& s, juce::Label& l, juce::Colour fill, juce::Colour outline, juce::Colour text)
    {
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s.setColour (juce::Slider::rotarySliderFillColourId, fill);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, outline);
        s.setColour (juce::Slider::thumbColourId, text);
        s.setColour (juce::Slider::textBoxTextColourId, text);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        l.setColour (juce::Label::textColourId, text);
        l.setJustificationType (juce::Justification::centredBottom);
        l.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        l.setMinimumHorizontalScale (0.75f);
    }
}

CombinedEditor::CombinedEditor (CombinedProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    lnf.setColour (juce::ComboBox::backgroundColourId, paper);
    lnf.setColour (juce::ComboBox::textColourId, ink);
    lnf.setColour (juce::ComboBox::outlineColourId, ink);
    lnf.setColour (juce::ComboBox::arrowColourId, ink);
    lnf.setColour (juce::PopupMenu::backgroundColourId, paper);
    lnf.setColour (juce::PopupMenu::textColourId, ink);
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, seal);
    setLookAndFeel (&lnf);

    // --- kostka ---
    for (int i = 0; i < CombinedProcessor::kNumPedalParams; ++i)
    {
        auto* k = pedalKnobs.add (new Knob());
        const auto* id = CombinedProcessor::pedalParamId (i);
        if (auto* prm = p.apvts.getParameter (id))
            k->label.setText (prm->getName (40), juce::dontSendNotification);
        styleKnob (k->slider, k->label, seal, indigo, ink);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, k->slider);
    }

    // --- łańcuch ---
    for (int i = 0; i < CombinedProcessor::kNumChainParams; ++i)
    {
        auto* k = chainKnobs.add (new Knob());
        k->label.setText (pl (CombinedProcessor::chainParamShortName (i)), juce::dontSendNotification);
        styleKnob (k->slider, k->label, tapeCol, juce::Colour (0xff3a3a3a), tapeCol);
        k->slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, CombinedProcessor::chainParamId (i), k->slider);
    }

    // --- presety ---
    for (int i = 0; i < p.getNumPrograms(); ++i)
        presets.addItem (p.getProgramName (i), i + 1);
    presets.setTextWhenNothingSelected (pl ("Presety…"));
    presets.onChange = [this]
    {
        const int id = presets.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presets);

    // --- footswitch ---
    footswitch.setClickingTogglesState (true);
    footswitch.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a5048));
    footswitch.setColour (juce::TextButton::buttonOnColourId, seal);
    footswitch.setColour (juce::TextButton::textColourOffId, paper);
    footswitch.setColour (juce::TextButton::textColourOnId, paper);
    footswitch.onStateChange = [this] { updateFootswitchText(); };
    addAndMakeVisible (footswitch);
    footswitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "pedalOn", footswitch);
    updateFootswitchText();

    // --- IR ---
    for (int s = 0; s < 2; ++s)
    {
        auto& slot = irSlots[(size_t) s];
        slot.load.setButtonText (s == 0 ? "Wczytaj IR A" : "Wczytaj IR B");
        slot.load.onClick  = [this, s] { chooseIR (s); };
        slot.clear.onClick = [this, s] { proc.clearCabIR (s); timerCallback(); };
        slot.name.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        slot.name.setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (slot.load);
        addAndMakeVisible (slot.clear);
        addAndMakeVisible (slot.name);
    }
    lastIRFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    setSize (1000, 690);
    timerCallback();
    startTimerHz (4);
}

CombinedEditor::~CombinedEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CombinedEditor::updateFootswitchText()
{
    footswitch.setButtonText (footswitch.getToggleState() ? pl ("KOSTKA: WŁ") : pl ("KOSTKA: WYŁ"));
}

void CombinedEditor::chooseIR (int slot)
{
    chooser = std::make_unique<juce::FileChooser> (pl (slot == 0 ? "Wczytaj IR kolumny (A)" : "Wczytaj IR kolumny (B)"),
                                                   lastIRFolder, "*.wav;*.aif;*.aiff;*.flac");
    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    juce::Component::SafePointer<CombinedEditor> safe (this);
    chooser->launchAsync (chooserFlags, [safe, slot] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto file = fc.getResult();
        if (file.existsAsFile())
        {
            safe->lastIRFolder = file.getParentDirectory();
            safe->proc.loadCabIR (slot, file);
        }
        safe->timerCallback();
    });
}

void CombinedEditor::timerCallback()
{
    for (int s = 0; s < 2; ++s)
    {
        const auto text = proc.getCabIRDisplayName (s);
        auto& label = irSlots[(size_t) s].name;
        if (label.getText() != text)
            label.setText (text, juce::dontSendNotification);
    }

    const int cur = proc.getCurrentProgram() + 1;
    if (presets.getSelectedId() != cur)
        presets.setSelectedId (cur, juce::dontSendNotification);
}

void CombinedEditor::paint (juce::Graphics& g)
{
    g.fillAll (studio);

    // nagłówek
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (paper);
    g.fillRect (header);
    g.setColour (ink);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("STONOGA", 20, 10, 170, 36, juce::Justification::centredLeft);
    auto sealBox = juce::Rectangle<float> (186.f, 12.f, 30.f, 34.f);
    g.setColour (seal);
    g.fillRect (sealBox);
    g.setColour (paper);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (pl ("百足"), sealBox.toNearestInt(), juce::Justification::centred);
    g.setColour (ink);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("x ALBINI", 226, 10, 200, 36, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff6d5f4d));
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (pl ("kostka > wzmacniacz > kolumna > pokój > konsoleta > taśma > ochrona uszu"),
                20, 44, 520, 16, juce::Justification::centredLeft);

    // panel kostki
    g.setColour (paper);
    g.fillRect (pedalPanel);
    g.setColour (ink);
    g.drawRect (pedalPanel.reduced (6), 2);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("KOSTKA STONOGA", pedalPanel.reduced (16, 10).removeFromTop (16), juce::Justification::centredLeft);

    // panel łańcucha
    g.setColour (panel);
    g.fillRoundedRectangle (chainPanel.toFloat(), 6.0f);
    g.setColour (tapeCol);
    g.drawText (pl ("ŁAŃCUCH NAGRANIOWY ALBINI"), chainPanel.reduced (16, 10).removeFromTop (16), juce::Justification::centredLeft);
}

void CombinedEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (headerH).reduced (14, 14);
    footswitch.setBounds (header.removeFromRight (130));
    header.removeFromRight (10);
    presets.setBounds (header.removeFromRight (260));

    // --- kostka: 28 gałek w 3 rzędach ---
    const int pedalRows = (CombinedProcessor::kNumPedalParams + pedalCols - 1) / pedalCols;
    pedalPanel = area.removeFromTop (34 + pedalRows * 118 + 8);
    {
        auto r = pedalPanel.reduced (14, 0).withTrimmedTop (30);
        const int w = r.getWidth() / pedalCols;
        for (int i = 0; i < pedalKnobs.size(); ++i)
        {
            const int row = i / pedalCols, col = i % pedalCols;
            juce::Rectangle<int> cell (r.getX() + col * w, r.getY() + row * 118, w, 118);
            pedalKnobs[i]->label.setBounds (cell.removeFromTop (28).reduced (2, 0));
            pedalKnobs[i]->slider.setBounds (cell.reduced (8, 0).withTrimmedBottom (4));
        }
    }

    // --- łańcuch: 9 gałek + pasek IR ---
    chainPanel = area.reduced (10, 8);
    {
        auto r = chainPanel.reduced (12, 0).withTrimmedTop (28);
        auto strip = r.removeFromBottom (44).reduced (4, 8);
        const int half = strip.getWidth() / 2;
        for (int s = 0; s < 2; ++s)
        {
            auto sr = (s == 0 ? strip.removeFromLeft (half) : strip).reduced (4, 0);
            auto& slot = irSlots[(size_t) s];
            slot.load.setBounds (sr.removeFromLeft (110));
            slot.clear.setBounds (sr.removeFromRight (26));
            slot.name.setBounds (sr.reduced (6, 0));
        }

        const int n = chainKnobs.size();
        const int w = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto cell = (i == n - 1) ? r : r.removeFromLeft (w);
            chainKnobs[i]->label.setBounds (cell.removeFromTop (22));
            chainKnobs[i]->slider.setBounds (cell.reduced (6, 0));
        }
    }
}
