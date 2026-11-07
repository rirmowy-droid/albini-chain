#pragma once
#include "StonogaDSP.h"

namespace stonoga {

struct Preset { const char* name; Params p; };

// drive, starve, gate, octave, mask, vowel, rust, crackle, feedback, fbTime, tone, mix, outDb,
// howl, reverb, size, sing, choir, revfb, chorus, chRate, soft, swell, body, rev2, size2, rev3, size3
inline const Preset kPresets[] = {
    {"Mięso",              {0.62f, 0.00f, 0.00f, 0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f, 0.30f, 0.45f, 1.f, -10.f,  0.00f, 0.15f, 0.35f, 0.00f, 0.00f, 0.00f}},
    {"Zdychająca bateria", {0.78f, 0.75f, 0.30f, 0.00f, 0.00f, 0.50f, 0.10f, 0.15f, 0.00f, 0.30f, 0.40f, 1.f, -7.f,   0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f}},
    {"Maska nō",           {0.70f, 0.10f, 0.00f, 0.15f, 0.85f, 0.35f, 0.00f, 0.00f, 0.00f, 0.30f, 0.55f, 1.f, -8.f,   0.20f, 0.30f, 0.45f, 0.60f, 0.00f, 0.00f}},
    {"Rdza",               {0.85f, 0.20f, 0.10f, 0.30f, 0.00f, 0.50f, 0.60f, 0.45f, 0.00f, 0.30f, 0.60f, 1.f, -8.f,   0.00f, 0.10f, 0.30f, 0.00f, 0.00f, 0.00f}},
    {"Sprzężenie",         {0.90f, 0.10f, 0.00f, 0.20f, 0.20f, 0.60f, 0.10f, 0.10f, 0.55f, 0.25f, 0.55f, 1.f, -7.f,   0.60f, 0.25f, 0.50f, 0.20f, 0.00f, 0.00f}},
    {"Wycie",              {0.85f, 0.05f, 0.00f, 0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f, 0.30f, 0.55f, 1.f, -9.f,   0.90f, 0.30f, 0.55f, 0.30f, 0.00f, 0.00f}},
    {"Chór umarłych",      {0.72f, 0.10f, 0.00f, 0.10f, 0.30f, 0.40f, 0.00f, 0.05f, 0.00f, 0.30f, 0.45f, 1.f, -16.f,  0.30f, 0.80f, 0.80f, 0.85f, 0.55f, 0.00f}},
    {"Utonięcie",          {0.88f, 0.15f, 0.10f, 0.25f, 0.40f, 0.60f, 0.25f, 0.30f, 0.20f, 0.50f, 0.50f, 1.f, -11.f,   0.50f, 0.70f, 0.90f, 0.60f, 0.40f, 0.45f}},
    {"Ciepły crunch",      {0.38f, 0.00f, 0.00f, 0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f, 0.30f, 0.50f, 1.f, -8.f,   0.00f, 0.22f, 0.40f, 0.00f, 0.00f, 0.00f,  0.20f, 0.25f, 0.90f, 0.00f, 0.45f}},
    {"Shoegaze",           {0.50f, 0.00f, 0.00f, 0.10f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f, 0.30f, 0.55f, 1.f, -9.f,   0.10f, 0.70f, 0.75f, 0.25f, 0.30f, 0.00f,  0.55f, 0.30f, 0.75f, 0.45f, 0.60f,  0.40f, 0.50f, 0.00f, 0.70f}},
    {"Ściana mgły",        {0.72f, 0.05f, 0.00f, 0.20f, 0.10f, 0.40f, 0.00f, 0.00f, 0.00f, 0.30f, 0.50f, 1.f, -18.f,  0.30f, 0.85f, 0.90f, 0.35f, 0.50f, 0.10f,  0.65f, 0.20f, 0.45f, 0.80f, 0.85f,  0.50f, 0.60f, 0.60f, 0.85f}},
    {"Trzy katedry",       {0.55f, 0.00f, 0.00f, 0.10f, 0.15f, 0.40f, 0.00f, 0.00f, 0.00f, 0.30f, 0.50f, 1.f, -11.f,  0.15f, 0.60f, 0.60f, 0.45f, 0.30f, 0.00f,  0.30f, 0.25f, 0.55f, 0.30f, 0.55f,  0.60f, 0.55f, 0.70f, 0.85f}},
    {"Totalny rozpad",     {1.00f, 0.60f, 0.55f, 0.50f, 0.50f, 0.70f, 0.75f, 0.80f, 0.45f, 0.65f, 0.65f, 1.f, -3.f,   0.40f, 0.40f, 0.70f, 0.40f, 0.30f, 0.30f}},
};
inline constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace stonoga
