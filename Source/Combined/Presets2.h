#pragma once

#include "Settings.h"
#include <initializer_list>
#include <utility>

namespace sxa
{
    struct Preset
    {
        const char* name;
        stonoga::Params pedal;
        Settings s;
    };

    using SP = stonoga::Params;
    using ST = Settings;

    inline stonoga::Params P (std::initializer_list<std::pair<float SP::*, float>> ch)
    {
        auto p = cleanPedal();
        for (const auto& c : ch) p.*(c.first) = c.second;
        return p;
    }

    inline Settings S (Source src, std::initializer_list<std::pair<float ST::*, float>> ch)
    {
        Settings s;
        s.source = (float) src;
        s.consoleModel = 1;   // brytyjska konsoleta z transformatorami - na niej powstała większość tych płyt
        if (src == Guitar) { s.cabA = 7; s.cabB = 2; s.cabBlend = 0.35f; }   // V30 średnia 2 + ciemna 2
        // rozsądne punkty wyjścia dla każdego źródła
        switch (src)
        {
            case Bass:   s.pedalOn = 0; s.ampDrive = 0.25f; s.srcComp = 0.5f; s.srcWidth = 0; s.srcSmear = 0; s.roomMix = 0.05f; s.width = 0; s.smear = 0; s.lowCut = 30; break;
            case Drums:  s.pedalOn = 0; s.mic = MicCondenser; s.compChar = 1; s.roomPredelay = 12; s.srcComp = 0.35f; s.srcWidth = 0; s.srcSmear = 0.1f; s.roomMix = 0.3f; s.roomSize = 0.5f; s.roomCrush = 0.5f; s.width = 0.2f; s.smear = 0.05f; break;
            case Vocals: s.deEss = 0.3f; s.pedalOn = 0; s.mic = MicDynamic; s.compChar = 2; s.srcComp = 0.6f; s.srcWidth = 0.15f; s.srcSmear = 0.25f; s.roomMix = 0.1f; s.roomSize = 0.3f; s.width = 0; s.smear = 0.1f; break;
            case Mix:    s.msMonoBass = 80.f; s.pedalOn = 0; s.busCompChar = 5; s.deHarsh = 0.3f; s.busScHpf = 100.f; s.srcComp = 0; s.srcWidth = 0; s.srcSmear = 0; s.roomMix = 0; s.busComp = 0.35f; s.width = 0.3f; s.smear = 0.05f; break;
            default: break;
        }
        for (const auto& c : ch) s.*(c.first) = c.second;
        return s;
    }

    inline const Preset kPresets[] = {
        // ---------------- GITARA ----------------
        { "Gitara: Garaż w Aberdeen",
          P ({ { &SP::drive, 0.62f }, { &SP::tone, 0.48f } }),
          S (Guitar, { { &ST::ampDrive, 0.15f }, { &ST::srcComp, 0.3f }, { &ST::srcWidth, 0.3f }, { &ST::srcSmear, 0.05f },
                       { &ST::roomMix, 0.25f }, { &ST::roomSize, 0.3f }, { &ST::tape, 0.35f } }) },

        { "Gitara: Cicho zwrotka, głośno refren",
          P ({ { &SP::drive, 0.85f }, { &SP::body, 0.25f }, { &SP::soft, 0.15f } }),
          S (Guitar, { { &ST::ampDrive, 0.2f }, { &ST::srcComp, 0.6f }, { &ST::srcWidth, 0.8f }, { &ST::srcLoose, 0.3f },
                       { &ST::srcSmear, 0.15f }, { &ST::roomMix, 0.15f }, { &ST::console, 0.35f }, { &ST::tape, 0.4f } }) },

        { "Gitara: Ściana flaneli",
          P ({ { &SP::drive, 0.80f }, { &SP::soft, 0.2f }, { &SP::body, 0.3f } }),
          S (Guitar, { { &ST::ampDrive, 0.15f }, { &ST::srcComp, 0.5f }, { &ST::srcWidth, 1.0f }, { &ST::srcLoose, 0.45f },
                       { &ST::srcSmear, 0.25f }, { &ST::srcSize, 0.5f }, { &ST::roomMix, 0.2f }, { &ST::tape, 0.4f }, { &ST::width, 0.5f } }) },

        { "Gitara: Mokry chorus z Seattle",
          P ({ { &SP::drive, 0.45f }, { &SP::soft, 0.7f }, { &SP::outDb, -8.f } }),
          S (Guitar, { { &ST::ampDrive, 0.2f }, { &ST::srcComp, 0.45f }, { &ST::chorus, 0.75f }, { &ST::chRate, 0.35f }, { &ST::chDepth, 0.55f },
                       { &ST::srcWidth, 0.4f }, { &ST::srcSmear, 0.3f }, { &ST::srcSize, 0.5f }, { &ST::roomMix, 0.15f }, { &ST::outputGain, 2.f } }) },

        { "Gitara: Smoła",
          P ({ { &SP::drive, 0.82f }, { &SP::tone, 0.38f }, { &SP::body, 0.35f } }),
          S (Guitar, { { &ST::consoleModel, 2.f }, { &ST::ampDrive, 0.15f }, { &ST::srcComp, 0.55f }, { &ST::chorus, 0.45f }, { &ST::chRate, 0.18f }, { &ST::chDepth, 0.7f },
                       { &ST::srcWidth, 0.85f }, { &ST::srcLoose, 0.4f }, { &ST::srcSmear, 0.3f }, { &ST::srcSize, 0.6f },
                       { &ST::tilt, -0.35f }, { &ST::tape, 0.45f } }) },

        { "Gitara: Czarne słońce",
          P ({ { &SP::drive, 0.88f }, { &SP::starve, 0.1f }, { &SP::body, 0.3f }, { &SP::tone, 0.44f } }),
          S (Guitar, { { &ST::ampDrive, 0.15f }, { &ST::srcComp, 0.55f }, { &ST::chorus, 0.15f }, { &ST::srcWidth, 1.0f }, { &ST::srcLoose, 0.35f },
                       { &ST::srcSmear, 0.25f }, { &ST::srcSize, 0.5f }, { &ST::roomMix, 0.15f }, { &ST::console, 0.4f }, { &ST::tape, 0.45f }, { &ST::outputGain, 3.f } }) },

        { "Gitara: Deszcz nad Puget Sound",
          P ({ { &SP::drive, 0.58f }, { &SP::soft, 0.45f }, { &SP::swell, 0.2f } }),
          S (Guitar, { { &ST::ampDrive, 0.2f }, { &ST::srcComp, 0.4f }, { &ST::chorus, 0.25f }, { &ST::srcWidth, 0.75f }, { &ST::srcLoose, 0.3f },
                       { &ST::srcSmear, 0.5f }, { &ST::srcSize, 0.7f }, { &ST::roomMix, 0.25f }, { &ST::roomSize, 0.6f }, { &ST::tilt, -0.1f } }) },

        { "Gitara: Prosto z taśmy, bez litości",
          P ({ { &SP::drive, 0.7f } }),
          S (Guitar, { { &ST::ampDrive, 0.2f }, { &ST::srcComp, 0.15f }, { &ST::srcWidth, 0.f }, { &ST::srcSmear, 0.f },
                       { &ST::roomMix, 0.35f }, { &ST::roomSize, 0.45f }, { &ST::tape, 0.55f }, { &ST::busComp, 0.1f }, { &ST::width, 0.f }, { &ST::smear, 0.f } }) },

        { "Gitara: Czysto, bez kostki",
          P ({}),
          S (Guitar, { { &ST::pedalOn, 0.f }, { &ST::ampInput, 6.f }, { &ST::ampDrive, 0.45f }, { &ST::srcComp, 0.3f }, { &ST::chorus, 0.3f },
                       { &ST::srcWidth, 0.4f }, { &ST::srcSmear, 0.2f }, { &ST::outputGain, 4.f } }) },

        { "Gitara: Brytyjski piec w Seattle",
          P ({ { &SP::drive, 0.35f }, { &SP::soft, 0.4f }, { &SP::outDb, -12.f } }),
          S (Guitar, { { &ST::ampModel, 1.f }, { &ST::ampDrive, 0.5f }, { &ST::ampPresence, 0.2f }, { &ST::ampSag, 0.35f },
                       { &ST::srcComp, 0.35f }, { &ST::srcWidth, 0.7f }, { &ST::srcLoose, 0.3f }, { &ST::srcSmear, 0.15f },
                       { &ST::roomMix, 0.2f }, { &ST::tape, 0.4f }, { &ST::micDist, 0.25f } }) },

        { "Gitara: Prostownik, ciężko i zwarcie",
          P ({}),
          S (Guitar, { { &ST::pedalOn, 0.f }, { &ST::ampModel, 2.f }, { &ST::ampInput, 4.f }, { &ST::ampDrive, 0.6f }, { &ST::ampResonance, 0.35f },
                       { &ST::ampPresence, -0.1f }, { &ST::ampSag, 0.5f }, { &ST::srcComp, 0.4f }, { &ST::srcWidth, 1.0f }, { &ST::srcLoose, 0.4f },
                       { &ST::srcSmear, 0.15f }, { &ST::roomMix, 0.12f }, { &ST::tape, 0.4f }, { &ST::cabBlend, 0.45f }, { &ST::outputGain, 2.f } }) },

        { "Gitara: Twin na krawędzi",
          P ({ { &SP::drive, 0.5f }, { &SP::soft, 0.6f }, { &SP::outDb, -10.f } }),
          S (Guitar, { { &ST::ampModel, 3.f }, { &ST::ampDrive, 0.55f }, { &ST::ampSag, 0.6f }, { &ST::cabMic, 4.f }, { &ST::micDist, 0.45f },
                       { &ST::srcComp, 0.4f }, { &ST::chorus, 0.3f }, { &ST::srcWidth, 0.6f }, { &ST::srcSmear, 0.25f }, { &ST::roomMix, 0.25f },
                       { &ST::outputGain, 2.f } }) },

        { "Gitara: Wstęgowy przy brzegu, ciemno",
          P ({ { &SP::drive, 0.78f }, { &SP::body, 0.3f } }),
          S (Guitar, { { &ST::ampModel, 1.f }, { &ST::ampDrive, 0.25f }, { &ST::cabMic, 3.f }, { &ST::micPos, 0.75f }, { &ST::micDist, 0.2f },
                       { &ST::srcComp, 0.5f }, { &ST::srcWidth, 0.9f }, { &ST::srcLoose, 0.4f }, { &ST::srcSmear, 0.2f }, { &ST::tape, 0.45f },
                       { &ST::outputGain, 2.f } }) },

        { "Gitara: Dwa podejścia, ściana",
          P ({ { &SP::drive, 0.82f }, { &SP::soft, 0.2f }, { &SP::body, 0.3f } }),
          S (Guitar, { { &ST::trueDouble, 1.f }, { &ST::ampModel, 1.f }, { &ST::ampDrive, 0.3f }, { &ST::srcComp, 0.5f }, { &ST::srcWidth, 1.0f },
                       { &ST::srcLoose, 0.4f }, { &ST::srcSmear, 0.2f }, { &ST::srcSize, 0.5f }, { &ST::roomMix, 0.15f }, { &ST::tape, 0.4f },
                       { &ST::outputGain, 1.f } }) },

        // ---------------- BAS ----------------
        { "Bas: Młot",
          P ({ { &SP::drive, 0.4f }, { &SP::soft, 0.8f } }),
          S (Bass, { { &ST::ampDrive, 0.35f }, { &ST::srcComp, 0.6f }, { &ST::console, 0.4f }, { &ST::tape, 0.45f } }) },

        { "Bas: Ciepły z taśmy",
          P ({}),
          S (Bass, { { &ST::ampDrive, 0.15f }, { &ST::srcComp, 0.45f }, { &ST::tape, 0.6f }, { &ST::tilt, -0.2f } }) },

        { "Bas: W smole (przester)",
          P ({ { &SP::drive, 0.7f }, { &SP::body, 0.3f }, { &SP::mix, 0.6f } }),
          S (Bass, { { &ST::pedalOn, 1.f }, { &ST::ampDrive, 0.3f }, { &ST::srcComp, 0.55f }, { &ST::tilt, -0.15f }, { &ST::tape, 0.45f } }) },

        { "Bas: Piec SVT w piwnicy",
          P ({}),
          S (Bass, { { &ST::ampModel, 4.f }, { &ST::ampDrive, 0.45f }, { &ST::ampSag, 0.4f }, { &ST::srcComp, 0.55f }, { &ST::tape, 0.45f }, { &ST::console, 0.35f } }) },

        // ---------------- PERKUSJA ----------------
        { "Perkusja: Bębny w drewnianej sali",
          P ({}),
          S (Drums, { { &ST::roomMix, 0.4f }, { &ST::roomSize, 0.55f }, { &ST::roomCrush, 0.4f }, { &ST::tape, 0.45f } }) },

        { "Perkusja: Zgnieciona sala",
          P ({}),
          S (Drums, { { &ST::roomMix, 0.5f }, { &ST::roomSize, 0.5f }, { &ST::roomCrush, 0.85f }, { &ST::srcComp, 0.5f }, { &ST::console, 0.4f }, { &ST::tape, 0.4f } }) },

        { "Perkusja: Sala opóźniona jak u Albiniego",
          P ({}),
          S (Drums, { { &ST::roomMix, 0.45f }, { &ST::roomSize, 0.5f }, { &ST::roomPredelay, 20.f }, { &ST::roomCrush, 0.25f },
                      { &ST::compChar, 3.f }, { &ST::srcComp, 0.3f }, { &ST::tape, 0.5f }, { &ST::busComp, 0.f } }) },

        { "Perkusja: Werbel przez 1176",
          P ({}),
          S (Drums, { { &ST::compMode, 1.f }, { &ST::compChar, 1.f }, { &ST::compThresh, -24.f }, { &ST::compRatio, 8.f }, { &ST::compAttack, 0.3f },
                      { &ST::compRelease, 120.f }, { &ST::compMakeup, 6.f }, { &ST::compMix, 0.6f }, { &ST::roomMix, 0.3f } }) },

        { "Perkusja: Atak i zgnieciona sala na taśmie",
          P ({}),
          S (Drums, { { &ST::trAttack, 0.45f }, { &ST::trSustain, -0.15f }, { &ST::compMode, 1.f }, { &ST::compChar, 1.f }, { &ST::compThresh, -26.f },
                      { &ST::compRatio, 12.f }, { &ST::compAttack, 0.5f }, { &ST::compRelease, 100.f }, { &ST::compMakeup, 8.f }, { &ST::compMix, 0.45f },
                      { &ST::roomMix, 0.4f }, { &ST::roomCrush, 0.8f }, { &ST::roomPredelay, 15.f }, { &ST::tapeModel, 2.f }, { &ST::tape, 0.5f } }) },

        { "Perkusja: Sucho i blisko",
          P ({}),
          S (Drums, { { &ST::roomMix, 0.08f }, { &ST::roomCrush, 0.f }, { &ST::srcComp, 0.45f }, { &ST::mic, MicDynamic }, { &ST::tape, 0.3f } }) },

        // ---------------- WOKAL ----------------
        { "Wokal: Krzyk przez dynamik",
          P ({}),
          S (Vocals, { { &ST::compChar, 1.f }, { &ST::srcComp, 0.7f }, { &ST::console, 0.4f }, { &ST::tape, 0.35f }, { &ST::roomMix, 0.12f } }) },

        { "Wokal: Dwa głosy w smole",
          P ({}),
          S (Vocals, { { &ST::mic, MicRibbon }, { &ST::srcComp, 0.6f }, { &ST::chorus, 0.25f }, { &ST::chRate, 0.25f }, { &ST::chDepth, 0.3f },
                       { &ST::srcWidth, 0.7f }, { &ST::srcLoose, 0.5f }, { &ST::srcSmear, 0.35f }, { &ST::tilt, -0.1f } }) },

        { "Wokal: Krótkie echo '91",
          P ({}),
          S (Vocals, { { &ST::compChar, 2.f }, { &ST::srcComp, 0.6f }, { &ST::slap, 0.35f }, { &ST::srcSmear, 0.1f }, { &ST::roomMix, 0.05f } }) },

        { "Wokal: Baryton w katedrze",
          P ({}),
          S (Vocals, { { &ST::mic, MicCondenser }, { &ST::srcComp, 0.55f }, { &ST::srcSmear, 0.45f }, { &ST::srcSize, 0.7f },
                       { &ST::roomMix, 0.2f }, { &ST::roomSize, 0.55f }, { &ST::tilt, 0.1f } }) },

        // ---------------- CAŁY MIKS ----------------
        { "Suma: Seattle '91",
          P ({}),
          S (Mix, { { &ST::busComp, 0.4f }, { &ST::console, 0.3f }, { &ST::tape, 0.3f }, { &ST::width, 0.3f }, { &ST::tilt, 0.15f } }) },

        { "Suma: Chicago na żywo",
          P ({}),
          S (Mix, { { &ST::busComp, 0.f }, { &ST::console, 0.25f }, { &ST::tape, 0.5f }, { &ST::width, 0.1f }, { &ST::smear, 0.f } }) },

        { "Suma: Ciężko i szeroko",
          P ({}),
          S (Mix, { { &ST::busComp, 0.45f }, { &ST::console, 0.4f }, { &ST::tape, 0.4f }, { &ST::width, 0.4f }, { &ST::smear, 0.1f } }) },

        { "Suma: Ciepła taśma 15 cali",
          P ({}),
          S (Mix, { { &ST::busComp, 0.3f }, { &ST::console, 0.3f }, { &ST::tapeModel, 3.f }, { &ST::tapeSpeed, 1.f }, { &ST::tape, 0.45f },
                    { &ST::width, 0.25f }, { &ST::deHarsh, 0.35f } }) },
    };

    inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}
