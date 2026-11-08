#pragma once

/** Wbudowane IR kolumn (Jesters Brutal Pack 1.0, CC0 - zob. Resources/IR/LICENSE-IR.md).
    Kolejność: od najciemniejszej do najjaśniejszej (jasność zmierzona wcześniej). */
namespace sxa
{
    struct BundledIR { const char* file; const char* name; };

    inline const BundledIR kBundledIRs[] = {
        { "11-wumbo.wav",                 "4x12 V30/Rockdriver – ciemna 1" },
        { "13-cannibal-choir.wav",        "4x12 V30/Rockdriver – ciemna 2" },
        { "02-darth-genocider.wav",       "4x12 V30/Rockdriver – ciemna 3" },
        { "15-impaler-jim.wav",           "4x12 V30/Rockdriver – ciemna 4" },
        { "03-kitten-slayer.wav",         "4x12 V30/Rockdriver – ciemna 5" },
        { "04-kaiju-tamer.wav",           "4x12 V30/Rockdriver – średnia 1" },
        { "08-big-bubba.wav",             "4x12 V30/Rockdriver – średnia 2" },
        { "10-october-32th.wav",          "4x12 V30/Rockdriver – średnia 3" },
        { "06-vertical-lip-stabber.wav",  "4x12 V30/Rockdriver – średnia 4" },
        { "01-cookie-monster.wav",        "4x12 V30/Rockdriver – średnia 5" },
        { "05-iceburn-suicide.wav",       "4x12 V30/Rockdriver – jasna 1" },
        { "12-world-collider.wav",        "4x12 V30/Rockdriver – jasna 2" },
        { "07-manslaughter-joe.wav",      "4x12 V30/Rockdriver – jasna 3" },
        { "14-cathode-ray-fleshburn.wav", "4x12 V30/Rockdriver – jasna 4" },
        { "09-devils-cunnilingus.wav",    "4x12 V30/Rockdriver – jasna 5" },
    };
    inline constexpr int kNumBundledIRs = (int) (sizeof (kBundledIRs) / sizeof (kBundledIRs[0]));
}
