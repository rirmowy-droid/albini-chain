#include "CombinedEditor.h"
#include "Presets2.h"
#include "BundledIRs.h"

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

CombinedEditor::Knob* CombinedEditor::addKnob (juce::OwnedArray<Knob>& list, const char* paramId, const juce::String& label, bool onPaper, bool vertical)
{
    auto* k = list.add (new Knob());
    auto& s = k->slider;
    if (vertical) s.setSliderStyle (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId, onPaper ? seal : tapeCol);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, onPaper ? indigo : juce::Colour (0xff3a3a3a));
    s.setColour (juce::Slider::thumbColourId, onPaper ? ink : tapeCol);
    s.setColour (juce::Slider::trackColourId, flannel);
    s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff3a3a3a));
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

static void styleLabel (juce::Label& l, const juce::String& text, juce::Colour c, float size = 12.5f, bool bold = true)
{
    l.setText (text, juce::dontSendNotification);
    l.setColour (juce::Label::textColourId, c);
    l.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
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
    lnf.setColour (juce::ToggleButton::textColourId, tapeCol);
    lnf.setColour (juce::ToggleButton::tickColourId, tapeCol);
    setLookAndFeel (&lnf);

    // ====================== nagłówek ======================
    for (int i = 0; i < sxa::NumSources; ++i) sourceBox.addItem (pl (sxa::kSourceNames[i]), i + 1);
    for (int i = 0; i < sxa::kNumStyles; ++i) styleBox.addItem (pl (sxa::kStyleNames[i]), i + 1);
    sourceAtt = std::make_unique<ComboAtt> (p.apvts, "source", sourceBox);
    styleAtt  = std::make_unique<ComboAtt> (p.apvts, "style", styleBox);
    presetBox.setTextWhenNothingSelected (pl ("Presety…"));
    rebuildPresetList();
    presetBox.onChange = [this] { presetChosen (presetBox.getSelectedId()); };

    autoMixButton.setColour (juce::TextButton::buttonColourId, flannel);
    autoMixButton.setColour (juce::TextButton::textColourOffId, paper);
    autoMixButton.onClick = [this] { proc.requestAutoMix(); };
    undoButton.setButtonText (pl ("COFNIJ"));
    undoButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a5048));
    undoButton.setColour (juce::TextButton::textColourOffId, paper);
    undoButton.setTooltip (pl ("Cofnij ostatni auto-miks we wszystkich kopiach wtyczki"));
    undoButton.onClick = [this] { proc.requestUndoAutoMix(); };

    const char* pageNames[2] = { "TOR", "KOMPRESOR I MIKS" };
    for (int i = 0; i < 2; ++i)
    {
        pageButtons[i].setButtonText (pl (pageNames[i]));
        pageButtons[i].setRadioGroupId (77);
        pageButtons[i].setClickingTogglesState (true);
        pageButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xffcdbf9f));
        pageButtons[i].setColour (juce::TextButton::buttonOnColourId, ink);
        pageButtons[i].setColour (juce::TextButton::textColourOffId, ink);
        pageButtons[i].setColour (juce::TextButton::textColourOnId, paper);
        pageButtons[i].onClick = [this, i] { setPage (i); };
        addAndMakeVisible (pageButtons[i]);
    }

    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setFont (juce::FontOptions (12.5f));
    statusLabel.setJustificationType (juce::Justification::centredRight);
    for (auto* c : std::initializer_list<juce::Component*> { &sourceBox, &presetBox, &styleBox, &autoMixButton, &undoButton, &statusLabel })
        addAndMakeVisible (c);

    // ====================== strona 1: tor ======================
    for (int i = 0; i < sxa::NumMics; ++i) micBox.addItem (pl (sxa::kMicNames[i]), i + 1);
    micAtt = std::make_unique<ComboAtt> (p.apvts, "mic", micBox);
    styleLabel (micLabel, pl ("Mikrofon"), tapeCol);
    micLabel.setJustificationType (juce::Justification::centredBottom);
    for (int i = 0; i < sxa::kNumTapes; ++i) tapeBox.addItem (pl (sxa::kTapeNames[i]), i + 1);
    for (int i = 0; i < 2; ++i) tapeSpeedBox.addItem (pl (sxa::kTapeSpeedNames[i]), i + 1);
    tapeAtt = std::make_unique<ComboAtt> (p.apvts, "tapeModel", tapeBox);
    tapeSpeedAtt = std::make_unique<ComboAtt> (p.apvts, "tapeSpeed", tapeSpeedBox);
    styleLabel (tapeLabel, pl ("Magnetofon"), tapeCol);
    tapeLabel.setJustificationType (juce::Justification::centredBottom);
    for (int i = 0; i < sxa::kNumConsoles; ++i) consoleBox.addItem (pl (sxa::kConsoleNames[i]), i + 1);
    consoleAtt = std::make_unique<ComboAtt> (p.apvts, "consoleModel", consoleBox);
    styleLabel (consoleLabel, pl ("Konsoleta"), tapeCol);
    consoleLabel.setJustificationType (juce::Justification::centredBottom);
    styleLabel (noPedalLabel, pl ("Kostka i wzmacniacz działają przy gitarze i basie.\nDla tego źródła tor zaczyna się od mikrofonu i studia."),
                juce::Colour (0xff6d5f4d), 15.f, false);
    noPedalLabel.setJustificationType (juce::Justification::centred);

    footswitch.setClickingTogglesState (true);
    footswitch.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a5048));
    footswitch.setColour (juce::TextButton::buttonOnColourId, seal);
    footswitch.setColour (juce::TextButton::textColourOffId, paper);
    footswitch.setColour (juce::TextButton::textColourOnId, paper);
    footswitch.onStateChange = [this] { footswitch.setButtonText (footswitch.getToggleState() ? pl ("KOSTKA\nWŁ") : pl ("KOSTKA\nWYŁ")); };
    footswitchAtt = std::make_unique<ButtonAtt> (p.apvts, "pedalOn", footswitch);
    footswitch.onStateChange();

    for (const auto& d : sxa::kPedal) addKnob (pedalKnobs, d.id, pl (d.name), true);
    for (const char* id : { "ampInput", "ampDrive", "ampPresence", "ampResonance", "ampSag" }) addKnob (ampKnobs, id, pl (shortNameFor (id)), true);
    for (const char* id : { "cabBlend", "micPos", "micDist" }) addKnob (cabKnobs, id, pl (shortNameFor (id)), true);
    for (int i = 0; i < sxa::kNumAmps; ++i) ampModelBox.addItem (pl (sxa::kAmpNames[i]), i + 1);
    ampModelAtt = std::make_unique<ComboAtt> (p.apvts, "ampModel", ampModelBox);
    styleLabel (ampModelLabel, pl ("Piec"), seal);
    ampModelLabel.setJustificationType (juce::Justification::centredBottom);
    for (int i = 0; i < sxa::kNumCabMics; ++i) cabMicBox.addItem (pl (sxa::kCabMicNames[i]), i + 1);
    cabMicAtt = std::make_unique<ComboAtt> (p.apvts, "cabMic", cabMicBox);
    styleLabel (cabMicLabel, pl ("Mikrofon przy kolumnie"), seal);
    cabMicLabel.setJustificationType (juce::Justification::centredBottom);
    hqButton.setButtonText (pl ("Jakość 8x"));
    hqButton.setTooltip (pl ("Kostka i piec liczone 8x zamiast 4x: gładsza góra przesteru, więcej pracy procesora"));
    hqAtt = std::make_unique<ButtonAtt> (p.apvts, "hq", hqButton);
    alignButton.setButtonText (pl ("Wyrównaj fazę A/B"));
    alignButton.setTooltip (pl ("Przesuwa IR B tak, żeby zgrał się w fazie z A (mniej dziur przy mieszaniu)"));
    alignAtt = std::make_unique<ButtonAtt> (p.apvts, "cabAlign", alignButton);
    for (auto* b : { &hqButton, &alignButton })
    {
        b->setColour (juce::ToggleButton::textColourId, ink);
        b->setColour (juce::ToggleButton::tickColourId, seal);
        b->setColour (juce::ToggleButton::tickDisabledColourId, ink.withAlpha (0.5f));
    }
    for (const char* id : { "trAttack", "trSustain", "distComp", "deEss", "chorus", "chrate", "chdepth", "distWidth", "distLoose", "distSmear", "distSize", "slap" })
        addKnob (srcKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "roomMix", "roomSize", "roomPredelay", "roomCrush", "console", "tape", "tilt", "lowCut", "vocalSpace" })
        addKnob (studioKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "busComp", "width", "smear" }) addKnob (busKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "autoGain", "pan", "outputGain", "protect" }) addKnob (outKnobs, id, pl (shortNameFor (id)), false);

    for (int s = 0; s < 2; ++s)
    {
        auto& slot = irSlots[(size_t) s];
        slot.load.setButtonText (pl ("Plik…"));
        slot.choose.addItem (pl ("Plik / wbudowana krzywa"), 1);
        for (int i = 0; i < sxa::kNumBundledIRs; ++i) slot.choose.addItem (pl (sxa::kBundledIRs[i].name), i + 2);
        slot.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, s == 0 ? "cabA" : "cabB", slot.choose);
        slot.load.onClick  = [this, s] { chooseIR (s); };
        slot.clear.onClick = [this, s] { proc.clearCabIR (s); timerCallback(); };
        slot.name.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        slot.name.setMinimumHorizontalScale (0.6f);
    }
    lastIRFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    for (auto* list : { &pedalKnobs, &ampKnobs, &cabKnobs, &srcKnobs, &studioKnobs, &busKnobs, &outKnobs })
        for (auto* k : *list) { page1.push_back (&k->slider); page1.push_back (&k->label); }
    for (auto* c : std::initializer_list<juce::Component*> { &micBox, &micLabel, &consoleBox, &consoleLabel, &noPedalLabel, &footswitch,
                                                             &ampModelBox, &ampModelLabel, &cabMicBox, &cabMicLabel, &hqButton, &alignButton,
                                                             &tapeBox, &tapeSpeedBox, &tapeLabel })
        page1.push_back (c);
    for (auto& s : irSlots) { page1.push_back (&s.load); page1.push_back (&s.clear); page1.push_back (&s.choose); }

    pedalPanel  = { "KOSTKA STONOGA", {}, seal, true };
    ampPanel    = { "WZMACNIACZ", {}, seal, true };
    cabPanel    = { "KOLUMNA I MIKROFON", {}, seal, true };
    srcPanel    = { "NA PRZESTERZE", {}, seal.brighter (0.3f), false };
    studioPanel = { pl ("STUDIO ALBINIEGO: MIKROFON, SALA, KONSOLETA, TAŚMA"), {}, tapeCol, false };
    busPanel    = { pl ("NA CAŁOŚCI"), {}, tapeCol, false };
    outPanel    = { pl ("WYJŚCIE"), {}, tapeCol, false };
    irPanel     = { "IR KOLUMNY", {}, tapeCol, false };

    // ====================== strona 2: kompresor i miks ======================
    for (int i = 0; i < 2; ++i) compModeBox.addItem (pl (sxa::kCompModeNames[i]), i + 1);
    for (int i = 0; i < sxa::kNumCompChars; ++i) { compCharBox.addItem (pl (sxa::kCompCharNames[i]), i + 1); busCharBox.addItem (pl (sxa::kCompCharNames[i]), i + 1); }
    compModeAtt = std::make_unique<ComboAtt> (p.apvts, "compMode", compModeBox);
    compCharAtt = std::make_unique<ComboAtt> (p.apvts, "compChar", compCharBox);
    busCharAtt  = std::make_unique<ComboAtt> (p.apvts, "busCompChar", busCharBox);
    styleLabel (busCharLabel, pl ("Kompresor sumy"), tapeCol);
    busCharLabel.setJustificationType (juce::Justification::centredBottom);
    allButtonsButton.setButtonText (pl ("1176: wszystkie przyciski"));
    allButtonsAtt = std::make_unique<ButtonAtt> (p.apvts, "compAllButtons", allButtonsButton);
    styleLabel (compModeLabel, pl ("Tryb"), tapeCol);
    styleLabel (compCharLabel, pl ("Charakter"), tapeCol);
    compModeLabel.setJustificationType (juce::Justification::centredBottom);
    compCharLabel.setJustificationType (juce::Justification::centredBottom);
    styleLabel (compInfo, pl ("„Jedna gałka”: działa gałka Kompresja ze strony TOR, a charakter ustala czasy i barwę. Ręcznie: te gałki, w granicach danego urządzenia (np. 1176: ratio 4/8/12/20)."),
                juce::Colours::grey, 12.f, false);

    for (const char* id : { "compThresh", "compRatio", "compAttack", "compRelease", "compMakeup", "compMix" })
        addKnob (compKnobs, id, pl (shortNameFor (id)), false);
    for (const char* id : { "duckKick", "duckRelease", "duckVocal" })
        addKnob (duckKnobs, id, pl (shortNameFor (id)), false);
    lowOnlyButton.setButtonText (pl ("tylko dół (< 150 Hz)"));
    lowOnlyAtt = std::make_unique<ButtonAtt> (p.apvts, "duckLowOnly", lowOnlyButton);

    addKnob (eqKnobs, "autoEqAmt", pl ("Siła korekty"), false);
    for (int b = 0; b < sxa::kNumEqBands; ++b)
    {
        const char* id = nullptr;
        for (const auto& d : sxa::kParams) if (d.mem == sxa::kEqMembers[b]) id = d.id;
        addKnob (eqKnobs, id, pl (shortNameFor (id)) + " Hz", false, true);
    }
    styleLabel (eqInfo, pl ("Ręczna / automatyczna korekta 8 pasm (na ścieżkach ustawia ją auto-miks, żeby robić miejsce ważniejszym instrumentom)."),
                juce::Colours::grey, 12.f, false);

    // --- klej i auto-korektor sumy (źródło „Cały miks”) ---
    for (int i = 0; i < 2; ++i) busModeBox.addItem (pl (sxa::kCompModeNames[i]), i + 1);
    busModeAtt = std::make_unique<ComboAtt> (p.apvts, "busMode", busModeBox);
    styleLabel (busModeLabel, pl ("Tryb"), tapeCol);
    busModeLabel.setJustificationType (juce::Justification::centredBottom);
    addKnob (glueKnobs, "busComp", pl ("Klej (gałka)"), false);
    for (const char* id : { "busThresh", "busRatio", "busAttack", "busRelease", "busScHpf", "busMix" })
        addKnob (glueKnobs, id, pl (shortNameFor (id)), false);
    addKnob (masterKnobs, "albumAmt", pl ("Barwa płyty"), false);
    addKnob (masterKnobs, "deHarsh", pl ("Wygładzanie góry"), false);
    trackButton.setButtonText (pl ("Ciągłe dopasowanie"));
    trackAtt = std::make_unique<ButtonAtt> (p.apvts, "albumTrack", trackButton);
    styleLabel (masterInfo, pl ("Krzywa: aktualna korekta barwy płyty (27 pasm, 40 Hz – 16 kHz) dla stylu z nagłówka. „Ciągłe dopasowanie” – korektor stale śledzi miks. „Wygładzanie góry” ścisza tylko wyskakujące syczenie i ostre szczyty."),
                juce::Colours::grey, 12.f, false);

    for (auto* list : { &compKnobs, &duckKnobs })
        for (auto* k : *list) { page2Track.push_back (&k->slider); page2Track.push_back (&k->label); }
    for (auto* c : std::initializer_list<juce::Component*> { &compModeBox, &compCharBox, &compModeLabel, &compCharLabel,
                                                             &allButtonsButton, &compInfo, &lowOnlyButton })
        page2Track.push_back (c);
    for (auto* list : { &glueKnobs, &masterKnobs })
        for (auto* k : *list) { page2Master.push_back (&k->slider); page2Master.push_back (&k->label); }
    for (auto* c : std::initializer_list<juce::Component*> { &busModeBox, &busModeLabel, &trackButton, &masterInfo })
        page2Master.push_back (c);
    for (auto* k : eqKnobs) { page2.push_back (&k->slider); page2.push_back (&k->label); }
    for (auto* c : std::initializer_list<juce::Component*> { &busCharBox, &busCharLabel, &eqInfo })
        page2.push_back (c);
    gluePanel     = { pl ("KLEJ SUMY: KOMPRESOR NA CAŁOŚĆ"), {}, flannel.brighter (0.4f), false };
    masterEqPanel = { pl ("AUTO-KOREKTOR SUMY I WYGŁADZANIE GÓRY"), {}, tapeCol, false };

    compPanel = { pl ("PEŁNY KOMPRESOR ŹRÓDŁA"), {}, flannel.brighter (0.4f), false };
    duckPanel = { pl ("USTĘPOWANIE: BAS STOPIE, GITARY WOKALOWI"), {}, tapeCol, false };
    eqPanel   = { pl ("KOREKTA BARWY (AUTO-MIKS)"), {}, tapeCol, false };

    for (auto* c : page1) addAndMakeVisible (c);
    for (auto* c : page2) addChildComponent (c);
    for (auto* c : page2Track) addChildComponent (c);
    for (auto* c : page2Master) addChildComponent (c);

    setResizable (true, true);
    setResizeLimits (960, 640, 1900, 1300);
    setSize (1160, 860);
    pageButtons[0].setToggleState (true, juce::dontSendNotification);
    setPage (0);
    timerCallback();
    startTimerHz (15);
}

CombinedEditor::~CombinedEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CombinedEditor::setPage (int newPage)
{
    page = newPage;
    const bool mix = proc.getSource() == sxa::Mix;
    for (auto* c : page2) c->setVisible (page == 1);
    for (auto* c : page2Track) c->setVisible (page == 1 && ! mix);
    for (auto* c : page2Master) c->setVisible (page == 1 && mix);
    for (auto* c : page1) c->setVisible (page == 0);
    updateVisibility (true);
    repaint();
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

void CombinedEditor::updateVisibility (bool force)
{
    const int src = proc.getSource();
    if (src == lastSource && ! force) return;
    const bool sourceChanged = src != lastSource;
    lastSource = src;
    if (sourceChanged && ! force) { setPage (page); resized(); return; }

    if (page == 0)
    {
        const bool pedal = sxa::hasPedal (src);
        for (auto* list : { &pedalKnobs, &ampKnobs, &cabKnobs })
            for (auto* k : *list) { k->slider.setVisible (pedal); k->label.setVisible (pedal); }
        footswitch.setVisible (pedal);
        for (auto* c : std::initializer_list<juce::Component*> { &ampModelBox, &ampModelLabel, &cabMicBox, &cabMicLabel, &hqButton, &alignButton })
            c->setVisible (pedal);
        for (auto& s : irSlots) { s.load.setEnabled (pedal); s.clear.setEnabled (pedal); s.choose.setEnabled (pedal); }
        noPedalLabel.setVisible (! pedal);
    }

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
        // pozycja 1 pokazuje nazwę własnego pliku (albo wbudowaną krzywą)
        auto& box = irSlots[(size_t) s].choose;
        if (box.getSelectedId() == 1)
        {
            const auto text = (s == 0 ? juce::String ("A: ") : juce::String ("B: ")) + proc.getCabIRDisplayName (s);
            if (box.getItemText (0) != text) { box.changeItemText (1, text); box.setText (text, juce::dontSendNotification); }
        }
        else if (box.getItemText (0) != pl ("Plik / wbudowana krzywa"))
            box.changeItemText (1, pl ("Plik / wbudowana krzywa"));
    }
    undoButton.setEnabled (proc.canUndoAutoMix());
    const auto st = proc.getAutoMixStatus();
    if (statusLabel.getText() != st) statusLabel.setText (st, juce::dontSendNotification);

    syncPresetBox();

    if (page == 1)
    {
        for (auto r : { grMeter, kickMeter, vocalMeter, busGrMeter, harshMeter })
            repaint (r.expanded (2, 20));
        repaint (curveArea);
    }
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

void CombinedEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float db, float rangeDb, const juce::String& caption, juce::Colour c)
{
    if (r.isEmpty()) return;
    auto bar = r.withTrimmedTop (18).withTrimmedBottom (18);
    g.setColour (juce::Colour (0xff111111));
    g.fillRect (bar);
    const float frac = juce::jlimit (0.f, 1.f, -db / rangeDb);
    g.setColour (c);
    g.fillRect (bar.withHeight (juce::roundToInt ((float) bar.getHeight() * frac)));   // ściskanie rośnie z góry w dół
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRect (bar);
    g.setColour (tapeCol);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (caption, r.removeFromTop (16), juce::Justification::centred);
    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (juce::String (db, 1) + " dB", r.removeFromBottom (16), juce::Justification::centred);
}

void CombinedEditor::paint (juce::Graphics& g)
{
    g.fillAll (studio);

    g.setColour (paper);
    g.fillRect (0, 0, getWidth(), headerH);
    g.setColour (ink);
    g.setFont (juce::FontOptions (26.f, juce::Font::bold));
    g.drawText ("STONOGA", 16, 4, 150, 30, juce::Justification::centredLeft);
    auto sealBox = juce::Rectangle<float> (156.f, 6.f, 26.f, 28.f);
    g.setColour (seal); g.fillRect (sealBox);
    g.setColour (paper); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
    g.drawText (pl ("百足"), sealBox.toNearestInt(), juce::Justification::centred);
    g.setColour (ink); g.setFont (juce::FontOptions (26.f, juce::Font::bold));
    g.drawText ("x ALBINI", 188, 4, 160, 30, juce::Justification::centredLeft);

    if (page == 0)
    {
        if (sxa::hasPedal (proc.getSource())) { drawPanel (g, pedalPanel); drawPanel (g, ampPanel); drawPanel (g, cabPanel); }
        else drawPanel (g, { pl ("TO ŹRÓDŁO NIE MA KOSTKI"), pedalPanel.bounds.getUnion (cabPanel.bounds), seal, true });
        for (const auto* p : { &srcPanel, &studioPanel, &busPanel, &outPanel, &irPanel }) drawPanel (g, *p);
    }
    else if (proc.getSource() == sxa::Mix)
    {
        for (const auto* p : { &gluePanel, &masterEqPanel, &eqPanel }) drawPanel (g, *p);
        drawMeter (g, busGrMeter, proc.getBusReductionDb(), 12.f, pl ("klej"), flannel.brighter (0.3f));
        drawMeter (g, harshMeter, proc.getDeHarshDb(), 10.f, pl ("góra"), tapeCol);
        drawCurve (g);
    }
    else
    {
        for (const auto* p : { &compPanel, &duckPanel, &eqPanel }) drawPanel (g, *p);
        drawMeter (g, grMeter, proc.getCompReductionDb(), 24.f, pl ("ściska"), flannel.brighter (0.3f));
        drawMeter (g, kickMeter, proc.getKickDuckDb(), 12.f, pl ("stopa"), tapeCol);
        drawMeter (g, vocalMeter, proc.getVocalDuckDb(), 8.f, pl ("wokal"), tapeCol);
    }
}

void CombinedEditor::drawCurve (juce::Graphics& g)
{
    if (curveArea.isEmpty()) return;
    auto r = curveArea.toFloat();
    g.setColour (juce::Colour (0xff111111));
    g.fillRoundedRectangle (r, 4.f);
    const float range = 8.f;
    auto yOf = [&] (float db) { return r.getCentreY() - db / range * (r.getHeight() * 0.45f); };
    auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * (std::log (hz / 30.f) / std::log (18000.f / 30.f)); };
    g.setFont (juce::FontOptions (10.5f));
    for (float db : { -8.f, -4.f, 0.f, 4.f, 8.f })
    {
        g.setColour (db == 0.f ? juce::Colour (0xff555555) : juce::Colour (0xff2c2c2c));
        g.drawHorizontalLine ((int) yOf (db), r.getX(), r.getRight());
        g.setColour (juce::Colours::grey);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getX() + 2, yOf (db) - 7, 24, 14), juce::Justification::left);
    }
    for (float hz : { 100.f, 1000.f, 10000.f })
    {
        g.setColour (juce::Colour (0xff2c2c2c));
        g.drawVerticalLine ((int) xOf (hz), r.getY(), r.getBottom());
        g.setColour (juce::Colours::grey);
        g.drawText (hz >= 1000.f ? juce::String ((int) (hz / 1000.f)) + "k" : juce::String ((int) hz), juce::Rectangle<float> (xOf (hz) + 2, r.getBottom() - 14, 30, 12), juce::Justification::left);
    }
    float gains[AlbumEq::numBands];
    proc.getAlbumGains (gains);
    float amt = 1.f;
    if (auto* v = proc.apvts.getRawParameterValue ("albumAmt")) amt = v->load();
    juce::Path path;
    // krzywa całej kaskady (to, co naprawdę słychać)
    for (int i = 0; i <= 120; ++i)
    {
        const float hz = 30.f * std::pow (18000.f / 30.f, (float) i / 120.f);
        float db = 0.f;
        for (int b = 0; b < AlbumEq::numBands; ++b) db += AlbumEq::peakResponseDb (48000.f, AlbumEq::bandHz (b), AlbumEq::q, gains[b] * amt, hz);
        const float y = yOf (juce::jlimit (-range, range, db));
        if (i == 0) path.startNewSubPath (xOf (hz), y); else path.lineTo (xOf (hz), y);
    }
    g.setColour (tapeCol);
    g.strokePath (path, juce::PathStrokeType (2.f));
}

// --- presety: wbudowane + własne pliki ---------------------------------------
namespace { constexpr int kSaveId = 1900, kFolderId = 1901, kUserBase = 2000; }

void CombinedEditor::rebuildPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    userPresets = CombinedProcessor::getUserPresetFiles();
    if (! userPresets.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading (pl ("Moje presety"));
        for (int i = 0; i < userPresets.size(); ++i) presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), kUserBase + i);
    }
    presetBox.addSeparator();
    presetBox.addItem (pl ("Zapisz obecne ustawienia jako preset…"), kSaveId);
    presetBox.addItem (pl ("Otwórz folder z presetami"), kFolderId);
    syncPresetBox();
}

void CombinedEditor::syncPresetBox()
{
    int want = proc.getCurrentProgram() + 1;
    const auto user = proc.getUserPresetName();
    if (user.isNotEmpty())
    {
        want = 0;
        for (int i = 0; i < userPresets.size(); ++i)
            if (userPresets[i].getFileNameWithoutExtension() == user) want = kUserBase + i;
    }
    if (presetBox.getSelectedId() != want) presetBox.setSelectedId (want, juce::dontSendNotification);
}

void CombinedEditor::presetChosen (int id)
{
    if (id <= 0) return;
    if (id <= proc.getNumPrograms()) { proc.setCurrentProgram (id - 1); return; }
    if (id >= kUserBase && id - kUserBase < userPresets.size())
    {
        proc.loadUserPreset (userPresets[id - kUserBase]);
        syncPresetBox();
        return;
    }
    if (id == kFolderId)
    {
        auto folder = CombinedProcessor::getUserPresetFolder();
        folder.createDirectory();
        folder.startAsProcess();
        syncPresetBox();
        return;
    }
    if (id == kSaveId)
    {
        syncPresetBox();
        auto* w = new juce::AlertWindow (pl ("Zapisz preset"), pl ("Nazwa presetu (zapisze się w Dokumentach, w folderze „Stonoga x Albini”):"),
                                         juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.getUserPresetName(), {});
        w->addButton (pl ("Zapisz"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (pl ("Anuluj"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<CombinedEditor> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            const auto name = w->getTextEditorContents ("name");
            if (safe->proc.saveUserPreset (name)) safe->rebuildPresetList();
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, pl ("Nie zapisano"),
                                                         pl ("Nie udało się zapisać presetu. Sprawdź nazwę."));
        }), true);
    }
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

void CombinedEditor::layoutCombo (juce::ComboBox& box, juce::Label& label, juce::Rectangle<int> cell)
{
    cell = cell.withTrimmedTop (20).reduced (4, 0);
    label.setBounds (cell.removeFromTop (18));
    box.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 26).translated (0, -10));
}

void CombinedEditor::resized()
{
    auto area = getLocalBounds();

    // --- nagłówek ---
    auto header = area.removeFromTop (headerH).reduced (10, 6);
    auto left = header.removeFromLeft (juce::jmin (360, getWidth() / 3));
    auto tabs = left.removeFromBottom (22).withTrimmedLeft (6);
    pageButtons[0].setBounds (tabs.removeFromLeft (70));
    tabs.removeFromLeft (4);
    pageButtons[1].setBounds (tabs.removeFromLeft (160));

    auto top = header.removeFromTop (28);
    undoButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (4);
    autoMixButton.setBounds (top.removeFromRight (100));
    top.removeFromRight (6);
    const int flex = juce::jmax (300, top.getWidth() - 18);
    styleBox.setBounds (top.removeFromRight (flex * 42 / 100));
    top.removeFromRight (6);
    presetBox.setBounds (top.removeFromRight (flex * 38 / 100));
    top.removeFromRight (6);
    sourceBox.setBounds (top);
    statusLabel.setBounds (header.withTrimmedTop (4));

    area = area.reduced (8, 6);
    const auto content = area;
    const int rowH = (area.getHeight() - 18) / 4;

    // ====================== strona 1 ======================
    {
        auto a = content;
        const int rowH1 = (content.getHeight() - 24) / 5;
        pedalPanel.bounds = a.removeFromTop (rowH1).reduced (3);
        auto inner = pedalPanel.bounds.reduced (8, 4);
        const int cellW = inner.getWidth() / (pedalKnobs.size() + 1);
        footswitch.setBounds (inner.withTrimmedTop (22).removeFromLeft (cellW).reduced (6, 16));
        layoutKnobs (pedalKnobs, inner, 1);
        a.removeFromTop (6);

        // piec | kolumna i mikrofon
        auto r2 = a.removeFromTop (rowH1);
        ampPanel.bounds = r2.removeFromLeft ((int) (r2.getWidth() * 0.58f)).reduced (3);
        cabPanel.bounds = r2.reduced (3);
        {
            auto ai = ampPanel.bounds.reduced (8, 4);
            const int w = ai.getWidth() / (ampKnobs.size() + 2);
            auto comboCell = ai.withWidth (w * 2);
            layoutCombo (ampModelBox, ampModelLabel, comboCell);
            hqButton.setBounds (comboCell.reduced (8, 0).removeFromBottom (28));
            layoutKnobs (ampKnobs, ai, 2);

            auto ci = cabPanel.bounds.reduced (8, 4);
            const int cw = ci.getWidth() / (cabKnobs.size() + 2);
            auto cabCell = ci.withWidth (cw * 2);
            layoutCombo (cabMicBox, cabMicLabel, cabCell);
            alignButton.setBounds (cabCell.reduced (8, 0).removeFromBottom (28));
            layoutKnobs (cabKnobs, ci, 2);
        }
        noPedalLabel.setBounds (pedalPanel.bounds.getUnion (cabPanel.bounds).reduced (20));
        a.removeFromTop (6);

        srcPanel.bounds = a.removeFromTop (rowH1).reduced (3);
        layoutKnobs (srcKnobs, srcPanel.bounds.reduced (8, 4));
        a.removeFromTop (6);

        studioPanel.bounds = a.removeFromTop (rowH1).reduced (3);
        auto sInner = studioPanel.bounds.reduced (8, 4);
        const int scw = sInner.getWidth() / (studioKnobs.size() + 3);
        layoutCombo (micBox, micLabel, sInner.withWidth (scw));
        layoutCombo (consoleBox, consoleLabel, sInner.withWidth (scw).translated (scw, 0));
        {
            auto tc = sInner.withWidth (scw).translated (scw * 2, 0);
            layoutCombo (tapeBox, tapeLabel, tc);
            tapeSpeedBox.setBounds (tc.reduced (4, 0).removeFromBottom (30).withSizeKeepingCentre (tc.getWidth() - 8, 24));
        }
        layoutKnobs (studioKnobs, sInner, 3);
        a.removeFromTop (6);

        auto r4 = a;
        const int unit = r4.getWidth() / 11;
        busPanel.bounds = r4.removeFromLeft (unit * 3).reduced (3);
        outPanel.bounds = r4.removeFromLeft (unit * 4).reduced (3);
        irPanel.bounds  = r4.reduced (3);
        layoutKnobs (busKnobs, busPanel.bounds.reduced (8, 4));
        layoutKnobs (outKnobs, outPanel.bounds.reduced (8, 4));
        auto irInner = irPanel.bounds.reduced (10, 6).withTrimmedTop (22);
        const int h = juce::jmin (30, irInner.getHeight() / 2 - 4);
        for (int s = 0; s < 2; ++s)
        {
            auto row = irInner.removeFromTop (h);
            irInner.removeFromTop (8);
            auto& slot = irSlots[(size_t) s];
            slot.clear.setBounds (row.removeFromRight (28));
            row.removeFromRight (4);
            slot.load.setBounds (row.removeFromRight (64));
            row.removeFromRight (4);
            slot.choose.setBounds (row);
        }
    }

    // ====================== strona 2 ======================
    {
        auto a = content;
        // kompresor: tryb, charakter, 6 gałek, wskaźnik
        compPanel.bounds = a.removeFromTop (rowH + rowH / 3).reduced (3);
        auto c = compPanel.bounds.reduced (8, 4);
        compInfo.setBounds (c.removeFromBottom (18).reduced (4, 0));
        grMeter = c.removeFromRight (60).reduced (8, 4).withTrimmedTop (16);
        auto extra = c.removeFromRight (c.getWidth() / 6);
        const int cw = c.getWidth() / 8;
        layoutCombo (compModeBox, compModeLabel, c.withWidth (cw));
        layoutCombo (compCharBox, compCharLabel, c.withWidth (cw).translated (cw, 0));
        layoutKnobs (compKnobs, c, 2);
        auto ex = extra.withTrimmedTop (20).reduced (4, 0);
        allButtonsButton.setBounds (ex.removeFromTop (26));
        ex.removeFromTop (6);
        busCharLabel.setBounds (ex.removeFromTop (18));
        busCharBox.setBounds (ex.removeFromTop (26));
        a.removeFromTop (6);

        // ustępowanie: 3 gałki + przełącznik + 2 wskaźniki
        duckPanel.bounds = a.removeFromTop (rowH).reduced (3);
        auto d = duckPanel.bounds.reduced (8, 4);
        vocalMeter = d.removeFromRight (60).reduced (8, 0).withTrimmedTop (16);
        kickMeter  = d.removeFromRight (60).reduced (8, 0).withTrimmedTop (16);
        const int dw = d.getWidth() / 4;
        layoutKnobs (duckKnobs, d.withTrimmedRight (dw));
        lowOnlyButton.setBounds (d.removeFromRight (dw).withSizeKeepingCentre (dw - 10, 26));
        a.removeFromTop (6);

        // --- źródło „Cały miks”: klej + auto-korektor zamiast kompresora i ustępowania ---
        {
            auto m = content;
            gluePanel.bounds = m.removeFromTop (rowH + rowH / 3).reduced (3);
            auto gp = gluePanel.bounds.reduced (8, 4);
            busGrMeter = gp.removeFromRight (60).reduced (8, 4).withTrimmedTop (16);
            const int gw = gp.getWidth() / (glueKnobs.size() + 2);
            layoutCombo (busModeBox, busModeLabel, gp.withWidth (gw));
            if (proc.getSource() == sxa::Mix)
                layoutCombo (busCharBox, busCharLabel, gp.withWidth (gw).translated (gw, 0));
            layoutKnobs (glueKnobs, gp, 2);
            m.removeFromTop (6);

            masterEqPanel.bounds = m.removeFromTop (rowH).reduced (3);
            auto mp = masterEqPanel.bounds.reduced (8, 4);
            masterInfo.setBounds (mp.removeFromBottom (18).reduced (4, 0));
            harshMeter = mp.removeFromRight (60).reduced (8, 0).withTrimmedTop (16);
            auto controls = mp.removeFromLeft (mp.getWidth() * 38 / 100);
            curveArea = mp.withTrimmedTop (22).reduced (6, 2);
            auto toggleRow = controls.removeFromBottom (28);
            trackButton.setBounds (toggleRow.reduced (8, 0));
            layoutKnobs (masterKnobs, controls);
        }

        // korekta barwy: siła + 8 suwaków
        eqPanel.bounds = a.reduced (3);
        auto e = eqPanel.bounds.reduced (8, 4);
        eqInfo.setBounds (e.removeFromBottom (18).reduced (4, 0));
        layoutKnobs (eqKnobs, e);
    }
}
