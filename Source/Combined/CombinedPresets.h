#pragma once

#include "../Stonoga/Presets.h"

namespace combined
{
    /** Ustawienia łańcucha nagraniowego (dół wtyczki). */
    struct Chain
    {
        bool  pedalOn;
        float ampInput, ampDrive, roomMix, roomSize, console, tape;
        float cabBlend   = 0.5f;
        float outputGain = 0.0f;
        float protect    = 0.7f;
    };

    /** Preset całości: preset kostki Stonoga (indeks w stonoga::kPresets) + łańcuch. */
    struct Preset
    {
        const char* name;
        int stonogaPreset;
        Chain chain;
    };

    //                                               pedal  input drive  room  size  cons  tape  [cab  out dB]
    inline const Preset kPresets[] = {
        { "Mięso w drewnianym pokoju",       0,  { true,  0.f, 0.12f, 0.18f, 0.35f, 0.30f, 0.25f } },
        { "Bateria na taśmie",               1,  { true,  0.f, 0.05f, 0.10f, 0.25f, 0.25f, 0.60f } },
        { "Maska nō w kamiennej sali",       2,  { true,  0.f, 0.10f, 0.30f, 0.75f, 0.20f, 0.30f } },
        { "Rdza przez konsoletę",            3,  { true,  0.f, 0.15f, 0.10f, 0.20f, 0.65f, 0.30f } },
        { "Sprzężenie na żywo",              4,  { true,  0.f, 0.10f, 0.25f, 0.50f, 0.30f, 0.35f } },
        { "Wycie w hali",                    5,  { true,  0.f, 0.10f, 0.35f, 0.90f, 0.25f, 0.30f } },
        { "Chór umarłych w piwnicy",         6,  { true,  0.f, 0.05f, 0.20f, 0.40f, 0.20f, 0.35f, 0.5f, 5.0f } },
        { "Ciepły crunch, czyste nagranie",  8,  { true,  0.f, 0.20f, 0.12f, 0.30f, 0.20f, 0.20f, 0.5f, 3.0f } },
        { "Shoegaze na taśmie",              9,  { true,  0.f, 0.05f, 0.15f, 0.50f, 0.20f, 0.45f } },
        { "Ściana mgły",                     10, { true,  0.f, 0.05f, 0.10f, 0.60f, 0.20f, 0.40f, 0.5f, 13.0f } },
        { "Totalny rozpad",                  12, { true,  0.f, 0.20f, 0.20f, 0.50f, 0.55f, 0.55f } },
        { "Albini bez kostki",               0,  { false, 6.f, 0.45f, 0.15f, 0.35f, 0.30f, 0.25f, 0.5f, 5.0f } },
    };

    inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}
