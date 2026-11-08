#pragma once

#include <atomic>
#include <mutex>
#include <array>
#include <cmath>
#include <vector>
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

        // --- ustępowanie: obwiednie stopy i głosu (z wątków audio) --------------
        void publishEnvelopes (int slot, float kick, float vocal)
        {
            if (slot < 0) return;
            slots[(size_t) slot].kick.store (kick, std::memory_order_relaxed);
            slots[(size_t) slot].vocal.store (vocal, std::memory_order_relaxed);
        }
        void readEnvelopes (int self, float& kick, float& vocal) const
        {
            kick = vocal = 0.f;
            for (int i = 0; i < maxSlots; ++i)
            {
                const auto& s = slots[(size_t) i];
                if (i == self || ! s.used.load (std::memory_order_relaxed)) continue;
                kick  = std::max (kick,  s.kick.load (std::memory_order_relaxed));
                vocal = std::max (vocal, s.vocal.load (std::memory_order_relaxed));
            }
        }

        // --- korekta barwy: widma ścieżek po ustawieniu głośności ----------------
        void publishSpectrum (int slot, const float* bandsDb)
        {
            if (slot < 0) return;
            for (int b = 0; b < kNumEqBands; ++b) slots[(size_t) slot].bands[(size_t) b] = bandsDb[b];
            slots[(size_t) slot].published = true;
        }
        void clearSpectrum (int slot) { if (slot >= 0) slots[(size_t) slot].published = false; }

        struct Spectrum { int source; std::array<float, kNumEqBands> db; };
        std::vector<Spectrum> otherSpectra (int self) const
        {
            std::vector<Spectrum> v;
            for (int i = 0; i < maxSlots; ++i)
            {
                const auto& s = slots[(size_t) i];
                if (i == self || ! s.used.load() || ! s.published.load() || s.source.load() == Mix) continue;
                Spectrum sp { s.source.load(), {} };
                for (int b = 0; b < kNumEqBands; ++b) sp.db[(size_t) b] = s.bands[(size_t) b].load();
                v.push_back (sp);
            }
            return v;
        }

        std::atomic<int> pendingEq { 0 };

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
        struct Slot
        {
            std::atomic<bool> used { false }, published { false };
            std::atomic<int> source { 0 }, order { 0 };
            std::atomic<float> kick { 0.f }, vocal { 0.f };
            std::array<std::atomic<float>, kNumEqBands> bands {};
        };
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
            { 0.15f, 0.45f, 0.0f, 0.30f, 0.45f, 0.50f,  0.30f,0.2f, 0.5f, 0.45f, 0.40f, 0.30f, 0.55f,  0.25f, 0.0f, 0.0f, MicNone,     -0.35f,  90, -2.0f,  -4.5f },
            { 0.05f, 0.40f, 0.0f, 0.35f, 0.50f, 0.50f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.25f, 0.0f, 0.0f, MicNone,     -0.30f,  30,  0.0f,  -2.0f },
            { 0.30f, 0.50f, 0.5f, 0.30f, 0.45f, 0.40f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.15f, 0.45f,  0.25f, 0.0f, 0.0f, MicCondenser,-0.30f,  25,  0.0f,   0.0f },
            { 0.15f, 0.45f, 0.0f, 0.30f, 0.40f, 0.60f,  0.20f,0.25f,0.3f, 0.40f, 0.50f, 0.35f, 0.50f,  0.25f, 0.0f, 0.0f, MicRibbon,   -0.10f,  90,  0.0f,   0.5f },
            { 0.00f, 0.30f, 0.0f, 0.30f, 0.45f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.35f, 0.15f,0.10f, MicNone,    -0.25f,  25,  0.0f, -14.0f },
        },
        { // Czarne słońce - gęsto i szeroko
            { 0.15f, 0.45f, 0.0f, 0.35f, 0.40f, 0.55f,  0.1f, 0.3f, 0.4f, 1.00f, 0.35f, 0.25f, 0.50f,  0.25f, 0.0f, 0.0f, MicNone,     -0.15f,  80, -2.5f,  -4.0f },
            { 0.05f, 0.40f, 0.0f, 0.40f, 0.45f, 0.55f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.25f, 0.0f, 0.0f, MicNone,     -0.10f,  30,  0.0f,  -2.5f },
            { 0.35f, 0.60f, 0.6f, 0.35f, 0.40f, 0.45f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.15f, 0.50f,  0.30f, 0.0f, 0.0f, MicCondenser, 0.10f,  25,  0.0f,   0.0f },
            { 0.20f, 0.50f, 0.0f, 0.35f, 0.35f, 0.60f,  0.0f, 0.3f, 0.3f, 0.30f, 0.25f, 0.40f, 0.60f,  0.30f, 0.0f, 0.0f, MicDynamic,   0.15f,  90,  0.0f,   1.0f },
            { 0.00f, 0.30f, 0.0f, 0.40f, 0.40f, 0.00f,  0.0f, 0.3f, 0.4f, 0.00f, 0.00f, 0.00f, 0.30f,  0.45f, 0.4f, 0.10f, MicNone,     0.00f,  25,  0.0f, -12.5f },
        },
    };

    /** Sprzęt i techniki dla stylu (konsoleta, charakter kompresorów, opóźnienie sali, slap).
        Oparte na tym, co opowiadali realizatorzy tamtych płyt (szczegóły w README). */
    struct Gear { float console, compChar, busChar, predelayMs, slap; };
    inline const Gear kGear[kNumStyles][NumSources] = {
        //  konsoleta  komp. źródła  komp. sumy  opóźn. sali  slap
        { // Seattle '91: brytyjska konsoleta, 1176 na bębnach, LA-2A na wokalu, slap na wokalu
            { 1, 0, 5,  6, 0.00f }, { 1, 1, 5,  0, 0.00f }, { 1, 1, 5,  8, 0.00f }, { 1, 2, 5,  5, 0.25f }, { 1, 0, 5, 0, 0.f } },
        { // Chicago (Albini): brytyjska konsoleta, dbx 160 na bębnach, delikatnie na basie, sala opóźniona, bez kompresji sumy
            { 1, 0, 0, 12, 0.00f }, { 1, 2, 0,  0, 0.00f }, { 1, 3, 0, 18, 0.00f }, { 1, 2, 0, 10, 0.00f }, { 1, 0, 0, 0, 0.f } },
        { // Ciepła sala: brytyjska konsoleta, optyczne i lampowe
            { 1, 2, 4, 10, 0.00f }, { 1, 2, 4,  0, 0.00f }, { 1, 4, 4, 12, 0.00f }, { 1, 2, 4,  8, 0.10f }, { 1, 0, 4, 0, 0.f } },
        { // Smoła: amerykańska konsoleta (twarda średnica), 1176
            { 2, 1, 5,  8, 0.00f }, { 2, 1, 5,  0, 0.00f }, { 2, 1, 5, 10, 0.00f }, { 2, 2, 5,  6, 0.05f }, { 2, 0, 5, 0, 0.f } },
        { // Czarne słońce: brytyjskie preampy, 1176, szyna SSL na sumie
            { 1, 1, 5,  8, 0.00f }, { 1, 1, 5,  0, 0.00f }, { 1, 1, 5, 12, 0.00f }, { 1, 1, 5,  8, 0.10f }, { 1, 0, 5, 0, 0.f } },
    };

    /** Bębny (punkt odniesienia) mają trafić na tyle dB RMS przed sumą. */
    inline constexpr float kDrumsTargetDb = -20.f;

    /** Kontekst miksu, który widzi dana ścieżka. */
    struct MixContext { int guitars = 0, guitarIndex = 0, basses = 0, vocals = 0, drums = 0; };

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
        for (auto m : kEqMembers) s.*m = 0.f;              // korekta barwy liczona od zera

        // ustępowanie: bas stopie, gitary wokalowi (styl decyduje, jak mocno)
        static const float kickDuck[kNumStyles]  = { 4.f, 2.f, 3.f, 3.f, 4.5f };
        static const float vocalDuck[kNumStyles] = { 2.5f, 1.f, 2.f, 2.5f, 3.f };
        s.duckKick = (src == Bass && ctx.drums > 0) ? kickDuck[style] : 0.f;
        s.duckLowOnly = 1.f;
        s.duckRelease = style == 1 ? 90.f : 130.f;
        s.duckVocal = (src == Guitar && ctx.vocals > 0) ? vocalDuck[style] : 0.f;

        const Gear& g = kGear[style][src];
        s.consoleModel = g.console; s.compChar = g.compChar; s.busCompChar = g.busChar;
        s.roomPredelay = g.predelayMs; s.slap = g.slap;
        s.compMode = 0.f;   // auto-miks pracuje na „jednej gałce”: progi zależą od gałki, nie od poziomu sesji
        if (style == 1) s.busComp = 0.f;   // Albini: bez kompresji na sumie

        if (src == Guitar)
        {
            if (ctx.basses == 0) s.lowCut = 70.f;          // nie ma basu: gitara może mieć więcej dołu
            if (ctx.vocals == 0) s.vocalSpace = 0.f;
            if (ctx.guitars >= 2)
            {
                // kilka gitar: każda węższa, rozstawione na boki (jak dwa nagrania)
                s.srcWidth = std::min (s.srcWidth, 0.35f);
                const float hard = style == 1 ? 1.0f : (style == 3 ? 0.6f : 0.8f);   // In Utero bardzo szeroko, Dirt węziej (pomiar płyt)
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

    /** Kto „rządzi” w danym paśmie (1 = najważniejszy). Pasma: 60, 125, 250, 500, 1k, 2k, 4k, 8k. */
    inline const int kBandPriority[4][kNumEqBands] = {
        { 3, 2, 1, 1, 1, 2, 2, 3 },   // gitara: średnica
        { 1, 1, 2, 3, 3, 3, 3, 3 },   // bas: dół
        { 1, 2, 3, 3, 2, 2, 1, 1 },   // perkusja: stopa, werbel, talerze
        { 4, 3, 2, 2, 1, 1, 1, 2 },   // wokal: słowa (1-4 kHz)
    };

    /** Korekta barwy dla jednej ścieżki na podstawie widm wszystkich.
        Gdzie ważniejszy instrument jest głośny, a ten nie musi tam grać pierwszych
        skrzypiec - wycięcie (do -6 dB). Gdzie ta ścieżka rządzi, a jest
        przykryta - delikatne podbicie (do +2 dB). Pasma, w których ścieżka
        prawie nic nie ma, zostają w spokoju. */
    inline void computeAutoEq (int source, const float* myDb, const std::vector<MixRegistry::Spectrum>& others, float* outDb)
    {
        float myMax = -200.f;
        for (int b = 0; b < kNumEqBands; ++b) myMax = std::max (myMax, myDb[b]);

        const auto toPow = [] (float db) { return std::pow (10.f, db * 0.1f); };
        for (int b = 0; b < kNumEqBands; ++b)
        {
            outDb[b] = 0.f;
            if (source < 0 || source >= Mix || myDb[b] < myMax - 30.f) continue;

            const int mine = kBandPriority[source][b];
            float hi = 0.f, all = 0.f; int bestOther = 9;
            for (const auto& o : others)
            {
                if (o.source < 0 || o.source >= Mix) continue;
                const int p = kBandPriority[o.source][b];
                const float e = toPow (o.db[(size_t) b]);
                all += e;
                if (p < mine) { hi += e; bestOther = std::min (bestOther, p); }
            }

            if (hi > 0.f)
            {
                const float diff = 10.f * std::log10 (hi) - myDb[b];          // o ile ważniejsi są głośniejsi
                if (diff > -6.f)
                {
                    const float maxCut = std::min (6.f, 1.5f + 1.5f * (float) (mine - bestOther));
                    outDb[b] = -std::min (maxCut, (diff + 6.f) * 0.35f);
                }
            }
            else if (mine == 1 && all > 0.f)
            {
                const float buried = 10.f * std::log10 (all) - myDb[b];
                if (buried > 3.f) outDb[b] = std::min (2.f, (buried - 3.f) * 0.3f);
            }
            outDb[b] = std::round (outDb[b] * 10.f) / 10.f;
        }
    }

    /** Krzywe barwy płyt (8 pasm, dB względem średniej) i szerokość stereo (strona względem
        środka, dB), zmierzone z pełnych plików stereo tym samym miernikiem, którego używa wtyczka.
          Chicago: In Utero (Scentless Apprentice, Milk It - miksy Albiniego)
          Smoła:   Dirt (Sickman, Down in a Hole) */
    struct AlbumCurve { bool has; float db[kNumEqBands]; float sideToMidDb; const char* name; };
    inline const AlbumCurve kAlbumCurves[kNumStyles] = {
        { false, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0.f, "" },
        { true,  { 3.37f, 6.05f, 1.55f, -1.62f, -1.03f, 0.31f, -2.17f, -6.45f }, -5.2f, "In Utero" },
        { false, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0.f, "" },
        { true,  { 0.96f, 1.83f, 1.68f, 1.73f, 1.68f, 1.04f, -1.99f, -6.93f }, -8.8f, "Dirt" },
        { false, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0.f, "" },
    };

    /** Panorama sumy (tryb poszerzania M/S, tylko powyżej ~150 Hz) tak, żeby strona względem
        środka zbliżyła się do płyty. Poszerzać umiemy, zwężać nie: zbyt szeroki miks -> 0. */
    inline float computeAlbumWidth (int style, float measuredSideToMidDb, float currentWidth)
    {
        const auto& c = kAlbumCurves[style];
        if (! c.has) return currentWidth;
        const float needDb = c.sideToMidDb - measuredSideToMidDb;          // o ile podnieść stronę
        if (std::abs (needDb) < 1.f) return currentWidth;                  // wystarczająco blisko - nie ruszamy
        // obecna panorama już podnosi stronę o ~20*log10(1 + 1.2*w) (dla pasma > 150 Hz)
        const float haveDb = 20.f * std::log10 (1.f + 1.2f * currentWidth);
        const float totalDb = std::max (0.f, haveDb + needDb);
        return std::max (0.f, std::min (1.f, (std::pow (10.f, totalDb / 20.f) - 1.f) / 1.2f));
    }

    /** Korekta sumy w stronę krzywej płyty: 60% różnicy, najwyżej +-4 dB na pasmo.
        Średnia korekty = 0, więc głośność całości się nie zmienia. */
    inline bool computeAlbumMatch (int style, const float* measuredDb, float* outDb)
    {
        const auto& c = kAlbumCurves[style];
        if (! c.has) { for (int b = 0; b < kNumEqBands; ++b) outDb[b] = 0.f; return false; }
        float mean = 0.f;
        for (int b = 0; b < kNumEqBands; ++b) mean += measuredDb[b];
        mean /= (float) kNumEqBands;
        float diff[kNumEqBands], dmean = 0.f;
        for (int b = 0; b < kNumEqBands; ++b) { diff[b] = c.db[b] - (measuredDb[b] - mean); dmean += diff[b]; }
        dmean /= (float) kNumEqBands;
        for (int b = 0; b < kNumEqBands; ++b)
            outDb[b] = std::round (std::max (-4.f, std::min (4.f, 0.6f * (diff[b] - dmean))) * 10.f) / 10.f;
        return true;
    }
}
