#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoMix.h"

/** Mikser w kopii na sumie: pasek na każdą ścieżkę z wtyczką Stonoga x Albini.
    Suwak, wycisz, solo, panorama, 3-pasmowy EQ i wskaźnik poziomu. Zmiany idą
    przez rejestr do kopii na danej ścieżce i tam zapisują się w jej parametrach. */
class MixerView : public juce::Component, private juce::Timer
{
public:
    struct Colours { juce::Colour ink, paper, accent, text; };

    explicit MixerView (Colours c) : col (c)
    {
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);
        empty.setText (juce::String::fromUTF8 ("Brak ścieżek. Wstaw Stonoga x Albini na ścieżki w mikserze FL (gitara, bas, bębny, wokal),\n"
                                                 "a tutaj pojawi się pasek każdej z nich."), juce::dontSendNotification);
        empty.setJustificationType (juce::Justification::centred);
        empty.setColour (juce::Label::textColourId, col.text.withAlpha (0.7f));
        addAndMakeVisible (empty);
        startTimerHz (15);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        empty.setBounds (getLocalBounds().reduced (20));
        layoutStrips();
    }

private:
    struct Strip : public juce::Component
    {
        Strip (int s, Colours c) : slot (s), col (c)
        {
            name.setJustificationType (juce::Justification::centred);
            name.setColour (juce::Label::textColourId, col.ink);
            name.setFont (juce::FontOptions (13.f, juce::Font::bold));
            name.setMinimumHorizontalScale (0.6f);
            addAndMakeVisible (name);

            auto bar = [&] (juce::Slider& sl, const char* label, double lo, double hi, double def, int field, bool isPan)
            {
                sl.setSliderStyle (juce::Slider::LinearBar);
                sl.setRange (lo, hi, 0.1);
                sl.setDoubleClickReturnValue (true, def);
                sl.setColour (juce::Slider::trackColourId, col.accent.withAlpha (0.55f));
                sl.setColour (juce::Slider::textBoxTextColourId, col.ink);
                sl.setColour (juce::Slider::textBoxOutlineColourId, col.ink.withAlpha (0.3f));
                const juce::String lab = juce::String::fromUTF8 (label);
                sl.textFromValueFunction = [lab, isPan] (double v)
                {
                    if (isPan) return lab + " " + (std::abs (v) < 0.02 ? juce::String ("C") : (v < 0 ? "L" : "P") + juce::String (juce::roundToInt (std::abs (v) * 100)));
                    return lab + " " + (v > 0 ? "+" : "") + juce::String (v, 1);
                };
                sl.onValueChange = [this, &sl, field] { if (! updating) sxa::MixRegistry::get().remoteSet (slot, field, (float) sl.getValue()); };
                addAndMakeVisible (sl);
            };
            bar (high, "Góra", -12, 12, 0, sxa::MixRegistry::High, false);
            bar (mid,  "Środek", -12, 12, 0, sxa::MixRegistry::Mid, false);
            bar (low,  "Dół", -12, 12, 0, sxa::MixRegistry::Low, false);
            bar (pan,  "Pan", -1, 1, 0, sxa::MixRegistry::Pan, true);
            pan.setRange (-1, 1, 0.01);

            fader.setSliderStyle (juce::Slider::LinearVertical);
            fader.setRange (-60, 12, 0.1);
            fader.setSkewFactorFromMidPoint (-12.0);
            fader.setDoubleClickReturnValue (true, 0);
            fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
            fader.setColour (juce::Slider::trackColourId, col.accent);
            fader.setColour (juce::Slider::thumbColourId, col.ink);
            fader.setColour (juce::Slider::textBoxTextColourId, col.ink);
            fader.setColour (juce::Slider::textBoxOutlineColourId, col.ink.withAlpha (0.3f));
            fader.textFromValueFunction = [] (double v) { return v <= -59.9 ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };
            fader.onValueChange = [this] { if (! updating) sxa::MixRegistry::get().remoteSet (slot, sxa::MixRegistry::Fader, (float) fader.getValue()); };
            addAndMakeVisible (fader);

            for (auto* b : { &mute, &solo })
            {
                b->setClickingTogglesState (true);
                b->setColour (juce::TextButton::buttonColourId, col.paper.darker (0.15f));
                b->setColour (juce::TextButton::textColourOffId, col.ink);
                b->setColour (juce::TextButton::textColourOnId, col.paper);
                addAndMakeVisible (b);
            }
            mute.setButtonText ("M"); mute.setTooltip (juce::String::fromUTF8 ("Wycisz"));
            solo.setButtonText ("S"); solo.setTooltip (juce::String::fromUTF8 ("Solo"));
            mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb03a2e));
            solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc9a227));
            mute.onClick = [this] { if (! updating) sxa::MixRegistry::get().remoteSet (slot, sxa::MixRegistry::Mute, mute.getToggleState() ? 1.f : 0.f); };
            solo.onClick = [this] { if (! updating) sxa::MixRegistry::get().remoteSet (slot, sxa::MixRegistry::Solo, solo.getToggleState() ? 1.f : 0.f); };
        }

        void refresh (const juce::String& label)
        {
            auto& reg = sxa::MixRegistry::get();
            updating = true;
            if (name.getText() != label) name.setText (label, juce::dontSendNotification);
            auto upd = [&] (juce::Slider& sl, int f)
            {
                if (sl.isMouseButtonDown()) return;
                const double v = reg.channel (slot, f);
                if (std::abs (sl.getValue() - v) > 1e-3) sl.setValue (v, juce::dontSendNotification);
            };
            upd (fader, sxa::MixRegistry::Fader); upd (pan, sxa::MixRegistry::Pan);
            upd (low, sxa::MixRegistry::Low); upd (mid, sxa::MixRegistry::Mid); upd (high, sxa::MixRegistry::High);
            mute.setToggleState (reg.channel (slot, sxa::MixRegistry::Mute) > 0.5f, juce::dontSendNotification);
            solo.setToggleState (reg.channel (slot, sxa::MixRegistry::Solo) > 0.5f, juce::dontSendNotification);
            updating = false;
            const float m = reg.meter (slot);
            if (std::abs (m - meterDb) > 0.3f) { meterDb = m; repaint (meterArea); }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 4);
            name.setBounds (r.removeFromTop (18));
            r.removeFromTop (2);
            for (auto* sl : { &high, &mid, &low, &pan }) { sl->setBounds (r.removeFromTop (18)); r.removeFromTop (3); }
            auto btn = r.removeFromTop (22);
            mute.setBounds (btn.removeFromLeft (btn.getWidth() / 2).reduced (1, 0));
            solo.setBounds (btn.reduced (1, 0));
            r.removeFromTop (4);
            meterArea = r.removeFromRight (10).withTrimmedBottom (20);
            fader.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (col.paper);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 5.f);
            g.setColour (col.ink.withAlpha (0.25f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 5.f, 1.f);

            g.setColour (col.ink.withAlpha (0.15f));
            g.fillRect (meterArea);
            const float norm = juce::jlimit (0.f, 1.f, (meterDb + 60.f) / 60.f);
            auto fill = meterArea.toFloat();
            fill = fill.removeFromBottom (fill.getHeight() * norm);
            g.setColour (meterDb > -1.f ? juce::Colour (0xffd03a2e) : meterDb > -9.f ? juce::Colour (0xffd9a326) : juce::Colour (0xff5d8a3a));
            g.fillRect (fill);
        }

        const int slot;
        Colours col;
        bool updating = false;
        float meterDb = -100.f;
        juce::Rectangle<int> meterArea;
        juce::Label name;
        juce::Slider high, mid, low, pan, fader;
        juce::TextButton mute, solo;
    };

    void timerCallback() override
    {
        auto& reg = sxa::MixRegistry::get();
        const auto slots = reg.trackSlots();
        if (slots != shown)
        {
            shown = slots;
            strips.clear();
            for (int s : slots) { strips.push_back (std::make_unique<Strip> (s, col)); content.addAndMakeVisible (*strips.back()); }
            empty.setVisible (slots.empty());
            layoutStrips();
        }
        // podpisy: nazwa ścieżki z DAW, a bez niej źródło + numer
        int perSource[sxa::NumSources] {};
        for (auto& st : strips)
        {
            const int src = juce::jlimit (0, (int) sxa::NumSources - 1, reg.getSource (st->slot));
            ++perSource[src];
            auto label = reg.getName (st->slot);
            if (label.isEmpty()) label = juce::String::fromUTF8 (sxa::kSourceNames[src]) + " " + juce::String (perSource[src]);
            st->refresh (label);
        }
    }

    void layoutStrips()
    {
        const int h = juce::jmax (100, getHeight() - (strips.size() * 92 > (size_t) getWidth() ? 10 : 0));
        const int w = strips.empty() ? 0 : juce::jlimit (92, 140, getWidth() / (int) strips.size());
        content.setSize (juce::jmax (getWidth(), w * (int) strips.size()), h);
        for (size_t i = 0; i < strips.size(); ++i) strips[i]->setBounds ((int) i * w, 0, w, h);
    }

    Colours col;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label empty;
    std::vector<int> shown;
    std::vector<std::unique_ptr<Strip>> strips;
};
