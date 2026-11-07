#pragma once

#include <atomic>
#include <mutex>
#include <array>
#include <cmath>
#include "Settings.h"

/** Auto-miks.

    Każda kopia wtyczki (na każdej ścieżce i na sumie) zapisuje się we wspólnym
    rejestrze w pamięci programu muzycznego. Kliknięcie AUTO-MIKS w dowolnej
    kopii uruchamia miks we wszystkich:

    1. ścieżki przez kilka sekund mierzą swoją głośność (gdy coś gra),
    2. każda ustawia studio wg stylu i swojego źródła, głośność względem
       pozostałych instrumentów, panoramę (gitary na boki), obcięcie dołu
       gitar pod bas i miejsce na wokal,
    3. kopia na sumie (źródło „Cały miks”) czeka, aż ścieżki skończą, mierzy
       całość i ustawia obróbkę sumy oraz głośność końcową.

    Działa, gdy program muzyczny trzyma wtyczki w jednym procesie (tak jest
    domyślnie w FL Studio, Reaperze, Abletonie, Cubase). */
namespace sxa
{
    class MixRegistry
    {
    public:
        static constexpr int maxSlots = 128;

        static MixRegistry& get() { static MixRegistry r; return r; }

        int add()
        {
            std::lock_guard<std::mutex> lock (mutex);
            for (int i = 0; i < maxSlots; ++i)
                if (! slots[(size_t) i].used.load())
                {
                    slots[(size_t) i].used = true;
                    slots[(size_t) i].order = ++orderCounter;
                    return i;
                }
            return -1;
        }

        void remove (int slot)
        {
            if (slot < 0) return;
            std::lock_guard<std::mutex> lock (mutex);
            slots[(size_t) slot].used = false;
        }

        void setSource (int slot, int source) { if (slot >= 0) slots[(size_t) slot].source = source; }

        /** Ile ścieżek danego źródła i które z kolei jest ta (wg kolejności dodania). */
        void count (int slot, int source, int& total, int& index) const
        {
            total = 0; index = 0;
            const int mine = slot >= 0 ? slots[(size_t) slot].order.load() : 0;
            for (const auto& s : slots)
                if (s.used.load() && s.source.load() == source)
                {
                    ++total;
                    if (s.order.load() < mine) ++index;
                }
        }

        int countSource (int source) const { int t = 0, i = 0; count (-1, source, t, i); return t; }

        void request (int style) { requestedStyle = style; generation.fetch_add (1); }
        int  currentGeneration() const { return generation.load(); }
        int  currentStyle() const { return requestedStyle.load(); }

        std::atomic<int> pendingTracks { 0 };

    private:
        struct Slot { std::atomic<bool> used { false }; std::atomic<int> source { 0 }; std::atomic<int> order { 0 }; };
        std::array<Slot, maxSlots> slots;
        std::mutex mutex;
        int orderCounter = 0;
        std::atomic<int> generation { 0 }, requestedStyle { 0 };
    };

    /** Ustawienia studia dla jednego źródła w danym stylu. */
    struct Template
    {
        float roomMix, roomSize, roomCrush, console, tape, srcComp;
        float chorus, chRate, chDepth, srcWidth, srcLoose, srcSmear, srcSize;
        float busComp, width, smear, mic, tilt, lowCut, vocalSpace;
        float relDb;   // głośność względem bębnów
    };

    //                         room  size  crush cons  tape  comp | chor  rate  dpth  wid   loose smear size | bus   wid   smear mic tilt   lowCut vSpace | rel dB
    inline const Template kTemplates[kNumStyles][NumSources] = {
        { // Seattle '91 - jasno i zbito
            { 0.12f, 0.30f, 0.0f, 0.30f, 0.30f, 0.45f,  0.0f, 0.3f, 0.4f, 0.90f, 0.35f, 0.15f, 0.35f,  0.20f, 0.0f, 0.0f, MicNone,      0.10f, 110, -2.0f,  -5.0f }, // gitara
            { 0.05f, 0.30f, 0.0f, 0.40f, 0.45f, 0.55f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.20f, 0.0f, 0.0f, MicNone,      0.00f,  30,  0.0f,  -3.0f }, // bas
            { 0.25f, 0.35f, 0.5f, 0.35f, 0.35f, 0.45f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.10f, 0.30f,  0.30f, 0.0f, 0.0f, MicCondenser, 0.30f,  25,  0.0f,   0.0f }, // perkusja
            { 0.10f, 0.25f, 0.0f, 0.30f, 0.30f, 0.65f,  0.0f, 0.3f, 0.3f, 0.20f, 0.20f, 0.20f, 0.35f,  0.30f, 0.0f, 0.0f, MicDynamic,   0.20f,  90,  0.0f,   1.0f }, // wokal
            { 0.00f, 0.30f, 0.0f, 0.30f, 0.30f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.40f, 0.3f, 0.05f, MicNone,     0.15f,  25,  0.0f, -13.0f }, // suma: docelowo dB RMS
        },
        { // Chicago bez litości - surowo, dużo sali
            { 0.25f, 0.45f, 0.0f, 0.30f, 0.45f, 0.20f,  0.0f, 0.3f, 0.4f, 0.60f, 0.45f, 0.00f, 0.30f,  0.10f, 0.0f, 0.0f, MicNone,      0.00f,  90, -1.0f,  -4.0f },
            { 0.10f, 0.40f, 0.0f, 0.35f, 0.55f, 0.30f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.10f, 0.0f, 0.0f, MicNone,      0.00f,  30,  0.0f,  -3.5f },
            { 0.45f, 0.55f, 0.7f, 0.30f, 0.50f, 0.20f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.10f, 0.0f, 0.0f, MicCondenser, 0.00f,  25,  0.0f,   0.0f },
            { 0.25f, 0.40f, 0.0f, 0.25f, 0.45f, 0.35f,  0.0f, 0.3f, 0.3f, 0.00f, 0.00f, 0.00f, 0.30f,  0.10f, 0.0f, 0.0f, MicDynamic,   0.00f,  90,  0.0f,  -1.0f },
            { 0.00f, 0.30f, 0.0f, 0.25f, 0.50f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.15f, 0.1f, 0.0f, MicNone,      0.00f,  25,  0.0f, -15.0f },
        },
        { // Ciepła sala - przestrzeń i pogłos
            { 0.20f, 0.55f, 0.0f, 0.30f, 0.35f, 0.35f,  0.2f, 0.3f, 0.4f, 0.75f, 0.30f, 0.40f, 0.60f,  0.20f, 0.0f, 0.0f, MicNone,     -0.10f, 100, -2.0f,  -5.0f },
            { 0.05f, 0.40f, 0.0f, 0.35f, 0.40f, 0.45f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.20f, 0.0f, 0.0f, MicNone,     -0.20f,  30,  0.0f,  -3.0f },
            { 0.30f, 0.60f, 0.35f,0.30f, 0.35f, 0.35f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.30f, 0.55f,  0.25f, 0.0f, 0.0f, MicCondenser,-0.10f,  25,  0.0f,   0.0f },
            { 0.20f, 0.55f, 0.0f, 0.30f, 0.30f, 0.55f,  0.0f, 0.3f, 0.3f, 0.15f, 0.20f, 0.45f, 0.65f,  0.25f, 0.0f, 0.0f, MicCondenser, 0.10f,  90,  0.0f,   1.5f },
            { 0.00f, 0.30f, 0.0f, 0.30f, 0.35f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.30f, 0.35f,0.15f, MicNone,    -0.10f,  25,  0.0f, -14.0f },
        },
        { // Smoła - ciemno, ciężko, podwójne głosy
            { 0.15f, 0.45f, 0.0f, 0.30f, 0.45f, 0.50f,  0.35f,0.2f, 0.6f, 0.85f, 0.40f, 0.30f, 0.55f,  0.25f, 0.0f, 0.0f, MicNone,     -0.35f,  90, -2.0f,  -4.5f },
            { 0.05f, 0.40f, 0.0f, 0.35f, 0.50f, 0.50f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.25f, 0.0f, 0.0f, MicNone,     -0.30f,  30,  0.0f,  -2.0f },
            { 0.30f, 0.50f, 0.5f, 0.30f, 0.45f, 0.40f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.15f, 0.45f,  0.25f, 0.0f, 0.0f, MicCondenser,-0.30f,  25,  0.0f,   0.0f },
            { 0.15f, 0.45f, 0.0f, 0.30f, 0.40f, 0.60f,  0.25f,0.25f,0.3f, 0.70f, 0.50f, 0.35f, 0.50f,  0.25f, 0.0f, 0.0f, MicRibbon,   -0.10f,  90,  0.0f,   0.5f },
            { 0.00f, 0.30f, 0.0f, 0.30f, 0.45f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.35f, 0.35f,0.10f, MicNone,    -0.25f,  25,  0.0f, -14.0f },
        },
        { // Czarne słońce - gęsto i szeroko
            { 0.15f, 0.45f, 0.0f, 0.35f, 0.40f, 0.55f,  0.1f, 0.3f, 0.4f, 1.00f, 0.35f, 0.25f, 0.50f,  0.25f, 0.0f, 0.0f, MicNone,     -0.15f,  80, -2.5f,  -4.0f },
            { 0.05f, 0.40f, 0.0f, 0.40f, 0.45f, 0.55f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.25f, 0.0f, 0.0f, MicNone,     -0.10f,  30,  0.0f,  -2.5f },
            { 0.35f, 0.60f, 0.6f, 0.35f, 0.40f, 0.45f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.15f, 0.50f,  0.30f, 0.0f, 0.0f, MicCondenser, 0.10f,  25,  0.0f,   0.0f },
            { 0.20f, 0.50f, 0.0f, 0.35f, 0.35f, 0.60f,  0.0f, 0.3f, 0.3f, 0.30f, 0.25f, 0.40f, 0.60f,  0.30f, 0.0f, 0.0f, MicDynamic,   0.15f,  90,  0.0f,   1.0f },
            { 0.00f, 0.30f, 0.0f, 0.40f, 0.40f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.45f, 0.4f, 0.10f, MicNone,     0.00f,  25,  0.0f, -12.5f },
        },
    };

    /** Bębny (punkt odniesienia) mają trafić na tyle dB RMS przed sumą. */
    inline constexpr float kDrumsTargetDb = -20.f;

    /** Kontekst miksu, który widzi dana ścieżka. */
    struct MixContext { int guitars = 0, guitarIndex = 0, basses = 0, vocals = 0; };

    /** Studio + panorama wg stylu (bez głośności - ta zależy od pomiaru). */
    inline void applyTemplate (Settings& s, int style, const MixContext& ctx)
    {
        const int src = (int) s.source;
        const Template& t = kTemplates[style][src];
        s.roomMix = t.roomMix; s.roomSize = t.roomSize; s.roomCrush = t.roomCrush;
        s.console = t.console; s.tape = t.tape; s.srcComp = t.srcComp;
        s.chorus = t.chorus; s.chRate = t.chRate; s.chDepth = t.chDepth;
        s.srcWidth = t.srcWidth; s.srcLoose = t.srcLoose; s.srcSmear = t.srcSmear; s.srcSize = t.srcSize;
        s.busComp = t.busComp; s.width = t.width; s.smear = t.smear;
        s.tilt = t.tilt; s.lowCut = t.lowCut; s.vocalSpace = t.vocalSpace;
        if (src != Guitar && src != Bass) s.mic = t.mic;   // przy gitarze/basie mikrofon to kolumna
        s.pan = 0.f;

        if (src == Guitar)
        {
            if (ctx.basses == 0) s.lowCut = 70.f;          // nie ma basu: gitara może mieć więcej dołu
            if (ctx.vocals == 0) s.vocalSpace = 0.f;
            if (ctx.guitars >= 2)
            {
                // kilka gitar: każda węższa, rozstawione na boki (jak dwa nagrania)
                s.srcWidth = std::min (s.srcWidth, 0.35f);
                const float hard = style == 1 ? 1.0f : 0.8f;
                static const float order[] = { -1.f, 1.f, 0.f, -0.5f, 0.5f, -0.75f, 0.75f };
                const int k = std::min (ctx.guitarIndex, 6);
                s.pan = order[k] * (k < 2 ? hard : 1.f);
            }
        }
    }

    /** Głośność względem bębnów (dla sumy: docelowy poziom całości). */
    inline float targetDb (int style, int source, const MixContext& ctx)
    {
        const Template& t = kTemplates[style][source];
        if (source == Mix) return t.relDb;
        float rel = t.relDb;
        if (source == Guitar && ctx.guitars >= 2) rel -= ctx.guitars >= 3 ? 3.f : 1.5f;
        return kDrumsTargetDb + rel;
    }
}
