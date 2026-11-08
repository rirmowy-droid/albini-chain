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

        // pełny kompresor źródła
        float compMode = 0, compChar = 0, compThresh = -18.f, compRatio = 4.f;
        float compAttack = 10.f, compRelease = 150.f, compMakeup = 0.f, compMix = 1.f;

        // ustępowanie innym instrumentom
        float duckKick = 0.f, duckLowOnly = 1.f, duckRelease = 120.f, duckVocal = 0.f;

        // korekta barwy (ustawiana przez auto-miks)
        float autoEqAmt = 1.f;

        // modele sprzętu i techniki z lat 90.
        float consoleModel = 0, busCompChar = 0, compAllButtons = 0, roomPredelay = 0.f, slap = 0.f;
        float eq0 = 0, eq1 = 0, eq2 = 0, eq3 = 0, eq4 = 0, eq5 = 0, eq6 = 0, eq7 = 0;
    };

    inline constexpr int kNumEqBands = 8;
    inline const float kEqHz[kNumEqBands] = { 60.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f };
    inline float Settings::* const kEqMembers[kNumEqBands] = { &Settings::eq0, &Settings::eq1, &Settings::eq2, &Settings::eq3,
                                                              &Settings::eq4, &Settings::eq5, &Settings::eq6, &Settings::eq7 };

    enum class Kind { Percent, Decibel, Choice, Toggle, Hertz, Pan, Blend, Rate, Millis, Ratio };

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
    inline const char* const kCompModeNames[] = { "Jedna gałka (auto)", "Ręczny" };
    inline const char* const kCompCharNames[] = { "Czysty (VCA)", "W stylu 1176 (FET)", "W stylu LA-2A (optyczny)",
                                                  "W stylu dbx 160 (VCA)", "W stylu vari-mu (lampowy)", "W stylu szyny SSL" };
    inline constexpr int kNumCompChars = 6;
    inline const char* const kConsoleNames[] = { "Ogólna", "Brytyjska (w stylu Neve)", "Amerykańska (w stylu API)", "Czysta in-line (w stylu SSL/Neotek)" };
    inline constexpr int kNumConsoles = 4;

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

        { "compMode",   "Kompresor: tryb",        "Tryb",         &Settings::compMode,   0, 1,     Kind::Choice },
        { "compChar",   "Kompresor: charakter",   "Charakter",    &Settings::compChar,   0, 5,     Kind::Choice },
        { "compAllButtons", "Kompresor 1176: wszystkie przyciski", "Wszystkie przyciski", &Settings::compAllButtons, 0, 1, Kind::Toggle },
        { "busCompChar","Całość: charakter kompresora", "Charakter sumy", &Settings::busCompChar, 0, 5, Kind::Choice },
        { "consoleModel","Konsoleta: model",      "Model konsolety", &Settings::consoleModel, 0, 3, Kind::Choice },
        { "roomPredelay","Sala: opóźnienie",      "Opóźnienie sali", &Settings::roomPredelay, 0, 40, Kind::Millis },
        { "slap",       "Krótkie echo (slap)",    "Slap",         &Settings::slap,       0, 1,     Kind::Percent },
        { "compThresh", "Kompresor: próg",        "Próg",         &Settings::compThresh,-40, 0,    Kind::Decibel },
        { "compRatio",  "Kompresor: ratio",       "Ratio",        &Settings::compRatio,  1, 20,    Kind::Ratio },
        { "compAttack", "Kompresor: atak",        "Atak",         &Settings::compAttack, 0.1f, 100, Kind::Millis },
        { "compRelease","Kompresor: puszczanie",  "Puszczanie",   &Settings::compRelease,10, 1500, Kind::Millis },
        { "compMakeup", "Kompresor: wzmocnienie", "Wzmocnienie",  &Settings::compMakeup, 0, 24,    Kind::Decibel },
        { "compMix",    "Kompresor: mix (równoległa)", "Mix",     &Settings::compMix,    0, 1,     Kind::Percent },

        { "duckKick",   "Ustępuj stopie",         "Ustępuj stopie", &Settings::duckKick, 0, 12,    Kind::Decibel },
        { "duckLowOnly","Ustępuj stopie: tylko dół", "Tylko dół", &Settings::duckLowOnly, 0, 1,    Kind::Toggle },
        { "duckRelease","Ustępuj stopie: powrót", "Powrót",       &Settings::duckRelease,20, 400,  Kind::Millis },
        { "duckVocal",  "Ustępuj wokalowi",       "Ustępuj wokalowi", &Settings::duckVocal, 0, 8,  Kind::Decibel },

        { "autoEqAmt",  "Korekta barwy: siła",    "Siła",         &Settings::autoEqAmt,  0, 1,     Kind::Percent },
        { "eq0",        "Korekta 60 Hz",          "60",           &Settings::eq0,       -8, 4,     Kind::Decibel },
        { "eq1",        "Korekta 125 Hz",         "125",          &Settings::eq1,       -8, 4,     Kind::Decibel },
        { "eq2",        "Korekta 250 Hz",         "250",          &Settings::eq2,       -8, 4,     Kind::Decibel },
        { "eq3",        "Korekta 500 Hz",         "500",          &Settings::eq3,       -8, 4,     Kind::Decibel },
        { "eq4",        "Korekta 1 kHz",          "1k",           &Settings::eq4,       -8, 4,     Kind::Decibel },
        { "eq5",        "Korekta 2 kHz",          "2k",           &Settings::eq5,       -8, 4,     Kind::Decibel },
        { "eq6",        "Korekta 4 kHz",          "4k",           &Settings::eq6,       -8, 4,     Kind::Decibel },
        { "eq7",        "Korekta 8 kHz",          "8k",           &Settings::eq7,       -8, 4,     Kind::Decibel },
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
