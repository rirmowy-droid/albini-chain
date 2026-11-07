#include "CombinedEditor.h"
#include "Presets2.h"

namespace
{
    const juce::Colour paper   { 0xffebdfc4 };
    const juce::Colour ink     { 0xff1b1a1a };
    const juce::Colour seal    { 0xffc23b2a };
    const juce::Colour indigo  { 0xff22385e };
    const juce::Colour studio  { 0xff1a1a1a };
    const juce::Colour panelBg { 0xff242424 };
    const juce::Colour tapeCol { 0xffd9c9a3 };
    const juce::Colour flannel { 0xff8c3b2e };

    constexpr int headerH = 66;

    juce::String pl (const char* s) { return juce::String::fromUTF8 (s); }

    const char* shortNameFor (const char* id)
    {
        for (const auto& d : sxa::kParams)
            if (juce::String (d.id) == id) return d.shortName;
        return id;
    }
}

CombinedEditor::Knob* CombinedEditor::addKnob (juce::OwnedArray<Knob>& list, const char* paramId, const juce::String& label, bool onPaper)
{
    auto* k = list.add (new Knob());
    auto& s = k->slider;
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId, onPaper ? seal : tapeCol);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, onPaper ? indigo : juce::Colour (0xff3a3a3a));
    s.setColour (juce::Slider::thumbColourId, onPaper ? ink : tapeCol);
    s.setColour (juce::Slider::textBoxTextColourId, onPaper ? ink : juce::Colours::lightgrey);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setColour (juce::Label::textColourId, onPaper ? ink : tapeCol);
    k->label.setJustificationType (juce::Justification::centredBottom);
    k->label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    k->label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (s);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    return k;
}

CombinedEditor::CombinedEditor (CombinedProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    lnf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2c2c2c));
    lnf.setColour (juce::ComboBox::textColourId, tapeCol);
    lnf.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a4a));
    lnf.setColour (juce::ComboBox::arrowColourId, tapeCol);
    lnf.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2c2c2c));
    lnf.setColour (juce::PopupMenu::textColourId, tapeCol);
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, flannel);
    setLookAndFeel (&lnf);

    // --- nagłówek ---
    for (int i = 0; i < sxa::NumSources; ++i) sourceBox.addItem (pl (sxa::kSourceNames[i]), i + 1);
    for (int i = 0; i < sxa::kNumStyles; ++i) styleBox.addItem (pl (sxa::kStyleNames[i]), i + 1);
    for (int i = 0; i < sxa::NumMics; ++i)    micBox.addItem (pl (sxa::kMicNames[i]), i + 1);
    sourceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "source", sourceBox);
    styleAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "style", styleBox);
    micAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "mic", micBox);

    for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected (pl ("Presety…"));
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) proc.setCurrentProgram (id - 1); };

    autoMixButton.setColour (juce::TextButton::buttonColourId, flannel);
    autoMixButton.setColour (juce::TextButton::textColourOffId, paper);
    autoMixButton.setTooltip (pl ("Ustawia wszystkie kopie wtyczki w projekcie wg stylu"));
    autoMixButton.onClick = [this] { proc.requestAutoMix(); };

    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setFont (juce::FontOptions (12.5f));
    statusLabel.setJustificationType (juce::Justification::centredRight);
    micLabel.setText (pl ("Mikrofon"), juce::dontSendNotification);
    micLabel.setColour (juce::Label::textColourId, tapeCol);
    micLabel.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    micLabel.setJustificationType (juce::Justification::centredBottom);
    noPedalLabel.setText (pl ("Kostka i wzmacniacz działają przy gitarze i basie.\nDla tego źródła łańcuch zaczyna się od mikrofonu i studia."), juce::dontSendNotification);
    noPedalLabel.setColour (juce::Label::textColourId, juce::Colour (0xff6d5f4d));
    noPedalLabel.setJustificationType (juce::Justification::centred);
    noPedalLabel.setFont (juce::FontOptions (15.f));

    for (auto* c : std::initializer_list<juce::Component*> { &sourceBox, &presetBox, &styleBox, &autoMixButton, &statusLabel, &micBox, &micLabel, &noPedalLabel })
        addAndMakeVisible (c);

    // --- kostka + wzmacniacz ---
    footswitch.setClickingTogglesState (true);
    footswitch.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a5048));
    footswitch.setColour (juce::TextButton::buttonOnColourId, seal);
    footswitch.setColour (juce::TextButton::textColourOffId, paper);
    footswitch.setColour (juce::TextButton::textColourOnId, paper);
    footswitch.onStateChange = [this] { footswitch.setButtonText (footswitch.getToggleState() ? pl ("KOSTKA\nWŁ") : pl ("KOSTKA\nWYŁ")); };
    addAndMakeVisible (footswitch);
    footswitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "pedalOn", footswitch);
    footswitch.onStateChange();

    for (const auto& d : sxa::kPedal) addKnob (pedalKnobs, d.id, pl (d.name), true);
    for (const char* id : { "ampInput", "ampDrive", "cabBlend" }) addKnob (ampKnobs, id, pl (shortNameFor (id)), true);

    for (const char* id : { "distComp", "chorus", "chrate", "chdepth", "distWidth", "distLoose", "distSmear", "distSize" })
        addKnob (srcKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "roomMix", "roomSize", "roomCrush", "console", "tape", "tilt", "lowCut", "vocalSpace" })
        addKnob (studioKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "busComp", "width", "smear" }) addKnob (busKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "autoGain", "pan", "outputGain", "protect" }) addKnob (outKnobs, id, pl (shortNameFor (id)), false);

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

    pedalPanel  = { "KOSTKA STONOGA", {}, seal, true };
    ampPanel    = { "WZMACNIACZ I KOLUMNA", {}, seal, true };
    srcPanel    = { "NA PRZESTERZE", {}, seal.brighter (0.3f), false };
    studioPanel = { pl ("STUDIO ALBINIEGO: MIKROFON, SALA, KONSOLETA, TAŚMA"), {}, tapeCol, false };
    busPanel    = { pl ("NA CAŁOŚCI"), {}, tapeCol, false };
    outPanel    = { pl ("WYJŚCIE"), {}, tapeCol, false };
    irPanel     = { "IR KOLUMNY", {}, tapeCol, false };

    setResizable (true, true);
    setResizeLimits (960, 640, 1900, 1300);
    setSize (1160, 730);
    timerCallback();
    startTimerHz (5);
}

CombinedEditor::~CombinedEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CombinedEditor::chooseIR (int slot)
{
    chooser = std::make_unique<juce::FileChooser> (pl (slot == 0 ? "Wczytaj IR kolumny (A)" : "Wczytaj IR kolumny (B)"),
                                                   lastIRFolder, "*.wav;*.aif;*.aiff;*.flac");
    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<CombinedEditor> safe (this);
    chooser->launchAsync (chooserFlags, [safe, slot] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto file = fc.getResult();
        if (file.existsAsFile())
        {
            safe->lastIRFolder = file.getParentDirectory();
            safe->proc.loadCabIR (slot, file);
        }
        safe->timerCallback();
    });
}

void CombinedEditor::updateVisibility()
{
    const int src = proc.getSource();
    if (src == lastSource) return;
    lastSource = src;

    const bool pedal = sxa::hasPedal (src);
    for (auto* list : { &pedalKnobs, &ampKnobs })
        for (auto* k : *list) { k->slider.setVisible (pedal); k->label.setVisible (pedal); }
    footswitch.setVisible (pedal);
    for (auto& s : irSlots) { s.load.setEnabled (pedal); s.clear.setEnabled (pedal); }
    noPedalLabel.setVisible (! pedal);

    srcPanel.title = src == sxa::Guitar ? pl ("NA PRZESTERZE: KOMPRESJA, CHORUS, PANORAMA, POGŁOS")
                   : src == sxa::Mix    ? pl ("NA SUMIE (PRZED STUDIEM)")
                                        : pl ("NA ŹRÓDLE: KOMPRESJA, CHORUS / DUBEL, PANORAMA, POGŁOS");
    repaint();
}

void CombinedEditor::timerCallback()
{
    updateVisibility();

    for (int s = 0; s < 2; ++s)
    {
        const auto text = proc.getCabIRDisplayName (s);
        if (irSlots[(size_t) s].name.getText() != text) irSlots[(size_t) s].name.setText (text, juce::dontSendNotification);
    }
    const auto st = proc.getAutoMixStatus();
    if (statusLabel.getText() != st) statusLabel.setText (st, juce::dontSendNotification);

    const int cur = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != cur) presetBox.setSelectedId (cur, juce::dontSendNotification);
}

void CombinedEditor::drawPanel (juce::Graphics& g, const Panel& p)
{
    if (p.bounds.isEmpty()) return;
    g.setColour (p.paper ? paper : panelBg);
    g.fillRoundedRectangle (p.bounds.toFloat(), 5.f);
    g.setColour (p.paper ? ink : p.accent.withAlpha (0.6f));
    g.drawRoundedRectangle (p.bounds.toFloat().reduced (0.5f), 5.f, p.paper ? 1.5f : 1.f);
    g.setColour (p.paper ? ink : p.accent);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (p.title, p.bounds.reduced (10, 5).removeFromTop (16), juce::Justification::centredLeft);
}

void CombinedEditor::paint (juce::Graphics& g)
{
    g.fillAll (studio);

    // nagłówek: drzeworyt Stonogi + studio
    g.setColour (paper);
    g.fillRect (0, 0, getWidth(), headerH);
    g.setColour (ink);
    g.setFont (juce::FontOptions (28.f, juce::Font::bold));
    g.drawText ("STONOGA", 16, 6, 160, 34, juce::Justification::centredLeft);
    auto sealBox = juce::Rectangle<float> (168.f, 9.f, 28.f, 30.f);
    g.setColour (seal); g.fillRect (sealBox);
    g.setColour (paper); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText (pl ("百足"), sealBox.toNearestInt(), juce::Justification::centred);
    g.setColour (ink); g.setFont (juce::FontOptions (28.f, juce::Font::bold));
    g.drawText ("x ALBINI", 204, 6, 170, 34, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff6d5f4d)); g.setFont (juce::FontOptions (12.f));
    g.drawText (pl ("surowe studio lat 90. – gitara, bas, perkusja, wokal, cały miks"), 16, 42, 420, 16, juce::Justification::centredLeft);

    const bool pedal = sxa::hasPedal (proc.getSource());
    if (pedal) { drawPanel (g, pedalPanel); drawPanel (g, ampPanel); }
    else
    {
        Panel info { pl ("TO ŹRÓDŁO NIE MA KOSTKI"), pedalPanel.bounds.getUnion (ampPanel.bounds), seal, true };
        drawPanel (g, info);
    }
    for (const auto* p : { &srcPanel, &studioPanel, &busPanel, &outPanel, &irPanel }) drawPanel (g, *p);
}

void CombinedEditor::layoutKnobs (juce::OwnedArray<Knob>& list, juce::Rectangle<int> area, int firstCellOffset)
{
    area = area.withTrimmedTop (20);
    const int cells = list.size() + firstCellOffset;
    const int w = area.getWidth() / juce::jmax (1, cells);
    area.removeFromLeft (w * firstCellOffset);
    for (int i = 0; i < list.size(); ++i)
    {
        auto cell = (i == list.size() - 1) ? area : area.removeFromLeft (w);
        list[i]->label.setBounds (cell.removeFromTop (18).reduced (2, 0));
        list[i]->slider.setBounds (cell.reduced (4, 0).withTrimmedBottom (2));
    }
}

void CombinedEditor::resized()
{
    auto area = getLocalBounds();

    // nagłówek
    auto header = area.removeFromTop (headerH).reduced (10, 8);
    header.removeFromLeft (juce::jmin (380, getWidth() / 3));
    auto top = header.removeFromTop (28);
    autoMixButton.setBounds (top.removeFromRight (110));
    top.removeFromRight (6);
    const int flex = juce::jmax (300, top.getWidth() - 18);
    styleBox.setBounds (top.removeFromRight (flex * 42 / 100));
    top.removeFromRight (6);
    presetBox.setBounds (top.removeFromRight (flex * 38 / 100));
    top.removeFromRight (6);
    sourceBox.setBounds (top);
    statusLabel.setBounds (header.withTrimmedTop (4));

    area = area.reduced (8, 6);
    const int rowH = (area.getHeight() - 18) / 4;

    // rząd 1: kostka + wzmacniacz
    auto r1 = area.removeFromTop (rowH);
    pedalPanel.bounds = r1.removeFromLeft ((int) (r1.getWidth() * 0.72f)).reduced (3);
    ampPanel.bounds   = r1.reduced (3);
    {
        auto inner = pedalPanel.bounds.reduced (8, 4);
        auto fs = inner.withTrimmedTop (22);
        const int cellW = inner.getWidth() / (pedalKnobs.size() + 1);
        footswitch.setBounds (fs.removeFromLeft (cellW).reduced (6, 16));
        layoutKnobs (pedalKnobs, inner, 1);
        layoutKnobs (ampKnobs, ampPanel.bounds.reduced (8, 4));
        noPedalLabel.setBounds (pedalPanel.bounds.getUnion (ampPanel.bounds).reduced (20));
    }
    area.removeFromTop (6);

    // rząd 2: na przesterze / źródle
    srcPanel.bounds = area.removeFromTop (rowH).reduced (3);
    layoutKnobs (srcKnobs, srcPanel.bounds.reduced (8, 4));
    area.removeFromTop (6);

    // rząd 3: studio (mikrofon + 8 gałek)
    studioPanel.bounds = area.removeFromTop (rowH).reduced (3);
    {
        auto inner = studioPanel.bounds.reduced (8, 4);
        const int cellW = inner.getWidth() / (studioKnobs.size() + 1);
        auto micCell = inner.withTrimmedTop (20).removeFromLeft (cellW).reduced (4, 0);
        micLabel.setBounds (micCell.removeFromTop (18));
        micBox.setBounds (micCell.withSizeKeepingCentre (micCell.getWidth(), 26).translated (0, -10));
        layoutKnobs (studioKnobs, inner, 1);
    }
    area.removeFromTop (6);

    // rząd 4: na całości | wyjście | IR
    auto r4 = area;
    const int unit = r4.getWidth() / 11;
    busPanel.bounds = r4.removeFromLeft (unit * 3).reduced (3);
    outPanel.bounds = r4.removeFromLeft (unit * 4).reduced (3);
    irPanel.bounds  = r4.reduced (3);
    layoutKnobs (busKnobs, busPanel.bounds.reduced (8, 4));
    layoutKnobs (outKnobs, outPanel.bounds.reduced (8, 4));
    {
        auto inner = irPanel.bounds.reduced (10, 6).withTrimmedTop (22);
        const int h = juce::jmin (30, inner.getHeight() / 2 - 4);
        for (int s = 0; s < 2; ++s)
        {
            auto row = inner.removeFromTop (h);
            inner.removeFromTop (8);
            auto& slot = irSlots[(size_t) s];
            slot.load.setBounds (row.removeFromLeft (110));
            slot.clear.setBounds (row.removeFromRight (28));
            slot.name.setBounds (row.reduced (6, 0));
        }
    }
}
