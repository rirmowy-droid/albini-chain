#pragma once

#include "../Stonoga/Presets.h"
#include <initializer_list>
#include <utility>

namespace combined
{
    /** Ustawienia łańcucha nagraniowego (dół wtyczki). */
    struct Chain
    {
        bool  pedalOn;
        float ampInput, ampDrive, roomMix, roomSize, console, tape;
        float smear, width;
        float outputGain = 0.0f;
        float cabBlend   = 0.5f;
        float protect    = 0.7f;
    };

    struct Preset
    {
        const char* name;
        stonoga::Params pedal;
        Chain chain;
    };

    /** Kostka: wychodzimy od czystych ustawień (bez sprzężenia, wycia, pętli,
        trzasków, rdzy, bramki i starego chóru) i zmieniamy tylko to, co trzeba. */
    inline stonoga::Params pedal (std::initializer_list<std::pair<float stonoga::Params::*, float>> changes)
    {
        stonoga::Params p;
        p.drive = 0.75f; p.tone = 0.5f; p.mix = 1.f; p.outDb = -10.f;
        p.reverb = 0.f; p.size = 0.5f;
        for (const auto& c : changes) p.*(c.first) = c.second;
        return p;
    }

    using P = stonoga::Params;

    inline const Preset kPresets[] = {
        { "Ściana",
          pedal ({ { &P::drive, 0.80f }, { &P::soft, 0.20f }, { &P::body, 0.25f }, { &P::reverb, 0.20f }, { &P::size, 0.45f } }),
          //  pedal  in    drive  room   size   cons   tape   smear  width  out
          { true,  0.f, 0.15f, 0.20f, 0.50f, 0.35f, 0.35f, 0.45f, 0.70f } },

        { "Mięso w drewnianym pokoju",
          pedal ({ { &P::drive, 0.62f }, { &P::tone, 0.45f }, { &P::reverb, 0.15f }, { &P::size, 0.35f } }),
          { true,  0.f, 0.12f, 0.22f, 0.35f, 0.30f, 0.25f, 0.20f, 0.50f } },

        { "Gęsty fuzz na taśmie",
          pedal ({ { &P::drive, 0.90f }, { &P::starve, 0.12f }, { &P::body, 0.20f }, { &P::tone, 0.42f } }),
          { true,  0.f, 0.10f, 0.15f, 0.40f, 0.30f, 0.60f, 0.35f, 0.60f, 6.0f } },

        { "Ciepły crunch",
          pedal ({ { &P::drive, 0.40f }, { &P::soft, 0.90f }, { &P::reverb, 0.15f }, { &P::size, 0.40f }, { &P::outDb, -8.f } }),
          { true,  0.f, 0.22f, 0.15f, 0.30f, 0.25f, 0.25f, 0.25f, 0.45f, 2.0f } },

        { "Shoegaze",
          pedal ({ { &P::drive, 0.55f }, { &P::soft, 0.40f }, { &P::reverb, 0.45f }, { &P::size, 0.70f },
                   { &P::chorus, 0.35f }, { &P::chRate, 0.25f }, { &P::swell, 0.40f }, { &P::outDb, -9.f } }),
          { true,  0.f, 0.08f, 0.15f, 0.50f, 0.20f, 0.40f, 0.70f, 0.75f } },

        { "Rozlana katedra",
          pedal ({ { &P::drive, 0.78f }, { &P::soft, 0.25f }, { &P::reverb, 0.40f }, { &P::size, 0.70f },
                   { &P::rev2, 0.30f }, { &P::size2, 0.60f } }),
          { true,  0.f, 0.10f, 0.25f, 0.80f, 0.25f, 0.30f, 0.80f, 0.75f } },

        { "Chór nad ścianą",
          pedal ({ { &P::drive, 0.82f }, { &P::soft, 0.20f }, { &P::choir, 0.55f }, { &P::reverb, 0.20f }, { &P::size, 0.55f } }),
          { true,  0.f, 0.12f, 0.18f, 0.55f, 0.30f, 0.35f, 0.55f, 0.80f } },

        { "Anielski chór",
          pedal ({ { &P::drive, 0.55f }, { &P::soft, 0.70f }, { &P::choir, 0.85f }, { &P::reverb, 0.25f }, { &P::size, 0.65f },
                   { &P::chorus, 0.20f } }),
          { true,  0.f, 0.08f, 0.15f, 0.60f, 0.20f, 0.30f, 0.70f, 0.75f } },

        { "Maska nō",
          pedal ({ { &P::drive, 0.72f }, { &P::mask, 0.55f }, { &P::vowel, 0.35f }, { &P::reverb, 0.25f }, { &P::size, 0.50f } }),
          { true,  0.f, 0.12f, 0.20f, 0.60f, 0.25f, 0.30f, 0.40f, 0.70f } },

        { "Albini bez kostki",
          pedal ({}),
          { false, 6.f, 0.50f, 0.15f, 0.35f, 0.30f, 0.25f, 0.15f, 0.35f, 5.0f } },
    };

    inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}
