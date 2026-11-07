#pragma once

#include "../Stonoga/StonogaDSP.h"

/** Wszystkie ustawienia Stonoga x Albini poza kostką, w jednym miejscu.
    Z tej struktury korzystają: parametry wtyczki, presety, auto-miks i silnik. */
namespace sxa
{
    enum Source { Guitar = 0, Bass, Drums, Vocals, Mix, NumSources };
    enum Mic    { MicNone = 0, MicDynamic, MicRibbon, MicCondenser, NumMics };

    inline bool hasPedal (int source) { return source == Guitar || source == Bass; }

    struct Settings
    {
        float source = Guitar, mic = MicNone, style = 0;

        // wzmacniacz i kolumna (gitara, bas)
        float pedalOn = 1, ampInput = 0, ampDrive = 0.15f, cabBlend = 0.5f;

        // na przesterze / na źródle
        float srcComp = 0.4f, chorus = 0.f, chRate = 0.3f, chDepth = 0.5f;
        float srcWidth = 0.5f, srcLoose = 0.3f, srcSmear = 0.15f, srcSize = 0.45f;

        // studio
        float roomMix = 0.18f, roomSize = 0.4f, roomCrush = 0.f, console = 0.3f, tape = 0.3f;
        float tilt = 0.f, lowCut = 20.f, vocalSpace = 0.f;

        // na całości
        float busComp = 0.25f, width = 0.35f, smear = 0.2f;

        // wyjście
        float autoGain = 0.f, pan = 0.f, outputGain = 0.f, protect = 0.7f;
    };

    enum class Kind { Percent, Decibel, Choice, Toggle, Hertz, Pan, Blend, Rate };

    struct ParamDef
    {
        const char* id;
        const char* name;       // pełna nazwa (DAW)
        const char* shortName;  // podpis pod gałką
        float Settings::* mem;
        float lo, hi;
        Kind kind;
    };

    inline const char* const kSourceNames[] = { "Gitara", "Bas", "Perkusja", "Wokal", "Cały miks" };
    inline const char* const kMicNames[]    = { "Brak (tylko kolumna)", "Dynamiczny", "Wstęgowy", "Pojemnościowy" };
    inline const char* const kStyleNames[]  = {
        "Seattle '91 – jasno i zbito",
        "Chicago bez litości – surowo, dużo sali",
        "Ciepła sala – przestrzeń i pogłos",
        "Smoła – ciemno, ciężko, podwójne głosy",
        "Czarne słońce – gęsto i szeroko",
    };
    inline constexpr int kNumStyles = 5;

    inline const ParamDef kParams[] = {
        { "source",     "Źródło",                 "Źródło",       &Settings::source,     0, NumSources - 1, Kind::Choice },
        { "mic",        "Mikrofon",               "Mikrofon",     &Settings::mic,        0, NumMics - 1,    Kind::Choice },
        { "style",      "Styl auto-miksu",        "Styl",         &Settings::style,      0, kNumStyles - 1, Kind::Choice },

        { "pedalOn",    "Kostka",                 "Kostka",       &Settings::pedalOn,    0, 1,     Kind::Toggle },
        { "ampInput",   "Wzmacniacz: wejście",    "Wejście",      &Settings::ampInput, -12, 24,    Kind::Decibel },
        { "ampDrive",   "Wzmacniacz: drive",      "Drive",        &Settings::ampDrive,   0, 1,     Kind::Percent },
        { "cabBlend",   "Kolumna A/B",            "Kolumna A/B",  &Settings::cabBlend,   0, 1,     Kind::Blend },

        { "distComp",   "Źródło: kompresja",      "Kompresja",    &Settings::srcComp,    0, 1,     Kind::Percent },
        { "chorus",     "Chorus",                 "Chorus",       &Settings::chorus,     0, 1,     Kind::Percent },
        { "chrate",     "Chorus: tempo",          "Tempo",        &Settings::chRate,     0, 1,     Kind::Rate },
        { "chdepth",    "Chorus: głębia",         "Głębia",       &Settings::chDepth,    0, 1,     Kind::Percent },
        { "distWidth",  "Źródło: panorama",       "Panorama",     &Settings::srcWidth,   0, 1,     Kind::Percent },
        { "distLoose",  "Źródło: rozjazd nagrań", "Rozjazd",      &Settings::srcLoose,   0, 1,     Kind::Percent },
        { "distSmear",  "Źródło: pogłos",         "Pogłos",       &Settings::srcSmear,   0, 1,     Kind::Percent },
        { "distSize",   "Źródło: długość pogłosu","Długość",      &Settings::srcSize,    0, 1,     Kind::Percent },

        { "roomMix",    "Sala",                   "Sala",         &Settings::roomMix,    0, 0.6f,  Kind::Percent },
        { "roomSize",   "Rozmiar sali",           "Rozmiar",      &Settings::roomSize,   0, 1,     Kind::Percent },
        { "roomCrush",  "Zgniecenie sali",        "Zgniecenie",   &Settings::roomCrush,  0, 1,     Kind::Percent },
        { "console",    "Konsoleta",              "Konsoleta",    &Settings::console,    0, 1,     Kind::Percent },
        { "tape",       "Taśma",                  "Taśma",        &Settings::tape,       0, 1,     Kind::Percent },
        { "tilt",       "Barwa (ciemno/jasno)",   "Barwa",        &Settings::tilt,      -1, 1,     Kind::Pan },
        { "lowCut",     "Obcięcie dołu",          "Obcięcie dołu",&Settings::lowCut,    20, 300,   Kind::Hertz },
        { "vocalSpace", "Miejsce na wokal",       "Miejsce na wokal", &Settings::vocalSpace, -6, 0, Kind::Decibel },

        { "busComp",    "Całość: kompresja",      "Kompresja",    &Settings::busComp,    0, 1,     Kind::Percent },
        { "width",      "Całość: panorama",       "Panorama",     &Settings::width,      0, 1,     Kind::Percent },
        { "smear",      "Całość: pogłos",         "Pogłos",       &Settings::smear,      0, 1,     Kind::Percent },

        { "autoGain",   "Głośność (auto-miks)",   "Głośność",     &Settings::autoGain, -24, 24,    Kind::Decibel },
        { "pan",        "Panorama ścieżki",       "Pan",          &Settings::pan,       -1, 1,     Kind::Pan },
        { "outputGain", "Wyjście",                "Wyjście",      &Settings::outputGain,-24, 12,   Kind::Decibel },
        { "protect",    "Ochrona uszu",           "Ochrona uszu", &Settings::protect,    0, 1,     Kind::Percent },
    };
    inline constexpr int kNumParams = (int) (sizeof (kParams) / sizeof (kParams[0]));

    // --- kostka Stonoga: tylko przester i barwa (szum i chór przechodzą do osobnego efektu) ---
    struct PedalDef { const char* id; const char* name; float stonoga::Params::* mem; float lo, hi; };
    inline const PedalDef kPedal[] = {
        { "drive",  "Przester",       &stonoga::Params::drive,  0.f, 1.f },
        { "soft",   "Miękkość",       &stonoga::Params::soft,   0.f, 1.f },
        { "starve", "Głód baterii",   &stonoga::Params::starve, 0.f, 1.f },
        { "tone",   "Barwa kostki",   &stonoga::Params::tone,   0.f, 1.f },
        { "body",   "Pełnia",         &stonoga::Params::body,   0.f, 1.f },
        { "swell",  "Mgła",           &stonoga::Params::swell,  0.f, 1.f },
        { "mix",    "Mix kostki",     &stonoga::Params::mix,    0.f, 1.f },
        { "output", "Do wzmacniacza", &stonoga::Params::outDb, -40.f, 6.f },
    };
    inline constexpr int kNumPedal = (int) (sizeof (kPedal) / sizeof (kPedal[0]));

    /** Czysta kostka: wszystkie efekty szumowe, pogłosy kostki i stary chór wyłączone. */
    inline stonoga::Params cleanPedal()
    {
        stonoga::Params p;
        p.drive = 0.75f; p.tone = 0.5f; p.mix = 1.f; p.outDb = -10.f;
        p.gate = p.octave = p.rust = p.crackle = p.feedback = p.howl = p.revfb = 0.f;
        p.mask = p.sing = p.choir = p.reverb = p.rev2 = p.rev3 = p.chorus = 0.f;
        return p;
    }
}
