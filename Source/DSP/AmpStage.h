#pragma once

#include "DSPUtils.h"
#include "../Stonoga/StonogaDSP.h"
#include <vector>
#include <memory>
#include <array>

/** Wzmacniacz: przedwzmacniacz (stopnie lampowe), barwa (tone stack),
    końcówka mocy z „zapadaniem się” zasilania (sag), presence i resonance.

    Model 0 („Albini”) to pierwotny, prosty stopień: od czystego do crunchu.
    Modele 1-4 są wzorowane na typowych piecach z płyt grunge'owych:
      1 brytyjski 800 (w stylu Marshalla JCM800) – środek do przodu, chrupko,
      2 prostownik (w stylu Mesa Dual Rectifier) – dużo gainu, wycięty środek, zwarty dół,
      3 amerykański czysty (w stylu Fendera Twin) – dużo zapasu, szklista góra, rwie się końcówka,
      4 basowy SVT (w stylu Ampega) – gęsty dół, warkot w średnicy, ciemna góra.

    Nieliniowość działa na 4x (albo 8x w trybie wysokiej jakości). */
/** Szybki tanh (ułamek łańcuchowy Lamberta, błąd < 1e-4), ok. 5x szybszy niż std::tanh. */
inline float fastTanh (float x)
{
    if (x > 4.97f) return 1.f;
    if (x < -4.97f) return -1.f;
    const float x2 = x * x;
    return x * (135135.f + x2 * (17325.f + x2 * (378.f + x2))) / (135135.f + x2 * (62370.f + x2 * (3150.f + 28.f * x2)));
}

/** Biquad w podwójnej precyzji - przy 192/384 kHz niskie częstotliwości w float szumią. */
struct DBiquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float process (float xf)
    {
        const double x = xf, y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0; }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    { b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0; }
    static void wcs (double sr, double f, double q, double& c, double& al)
    { const double w = 2 * juce::MathConstants<double>::pi * f / sr; c = std::cos (w); al = std::sin (w) / (2 * q); }
    void lowpass (double sr, double f, double q)  { double c, al; wcs (sr, f, q, c, al); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass (double sr, double f, double q) { double c, al; wcs (sr, f, q, c, al); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void peak (double sr, double f, double q, double db)
    { double c, al; wcs (sr, f, q, c, al); const double A = std::pow (10.0, db / 40.0); set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
    void lowshelf (double sr, double f, double db)
    {
        double c, al; wcs (sr, f, 0.7071, c, al); const double A = std::pow (10.0, db / 40.0), s = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf (double sr, double f, double db)
    {
        double c, al; wcs (sr, f, 0.7071, c, al); const double A = std::pow (10.0, db / 40.0), s = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
};

class AmpStage
{
public:
    enum Model { Albini = 0, Brit800, Rectifier, CleanUS, BassSVT,
                 QuadBroken, BognerMarshall, AC30, Bassman, SuperReverb, Roadmaster, MesaStudioSS, NumModels };

    /** osFactorLog2: 2 = 4x, 3 = 8x (wysoka jakość). */
    void prepare (const juce::dsp::ProcessSpec& spec, int osFactorLog2 = 2);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    void setGain (float gainDb);
    void setDrive (float drive);          // 0..1
    void setModel (int m);
    void setPresence (float p)  { presence = juce::jlimit (-1.f, 1.f, p); }   // -1..1 → ±6 dB przy 3,5 kHz
    void setResonance (float r) { resonance = juce::jlimit (-1.f, 1.f, r); }  // -1..1 → ±6 dB przy 100 Hz
    void setSag (float s)       { sag = juce::jlimit (0.f, 1.f, s); }
    /** true: sygnał przechodzi tylko przez oversampling (ta sama latencja, bez wzmacniacza). */
    void setPassThrough (bool p) { passThrough = p; }

    float getLatencyInSamples() const;
    int getModel() const { return model; }

private:
    struct ModelDef
    {
        float inHpf, brightHz, brightDb, maxDb;
        int stages;
        float bias, interLp, interHpf;
        float lowHz, lowDb, midHz, midQ, midDb, hiHz, hiDb;
        float powerDb, trimDb;
        float powerAsym = 0.f;   // końcówka: 0 = przeciwsobna symetryczna, > 0 = jak z jedną lampą / klasa A
        float buzz = 0.f;        // przepalony głośnik: grzechotanie przy mocnym dole
        int solidState = 0;      // końcówka tranzystorowa: twarde obcinanie, bez sag
        float sagExtra = 0.f;    // ile mocniej „przysiada” zasilanie
    };
    static const ModelDef& def (int m);

    void updateBrightShelf (float drive);
    void updateModelFilters();
    void updateVoiceFilters();
    void processModelled (juce::dsp::AudioBlock<float>& up);

    double sampleRate = 44100.0;
    int osLog2 = 2;
    bool passThrough = false;
    int model = Albini, appliedModel = -1;
    float presence = 0.f, resonance = 0.f, sag = 0.3f;
    float appliedPresence = 99.f, appliedResonance = 99.f;

    juce::dsp::Gain<float> inputGain;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    albini::StereoIIR brightShelf;
    float lastShelfDrive = -1.0f;

    std::vector<albini::OnePole> couplingHP, postLP;
    std::vector<albini::DCBlocker> dc;

    // modele: filtry między stopniami i barwa w domenie oversamplingu
    static constexpr int maxCh = 2, maxStages = 3;
    DBiquad interLp[maxCh][maxStages], interHp[maxCh][maxStages];
    DBiquad toneLow[maxCh], toneMid[maxCh], toneHigh[maxCh];
    stonoga::Biquad modelHp[maxCh];               // sprzężenie wejścia (zależne od modelu)
    stonoga::Biquad presenceF[maxCh], resonanceF[maxCh];
    DBiquad buzzLp[maxCh], buzzHp[maxCh];
    float sagEnv = 0.f, sagAtt = 0.f, sagRel = 0.f;
    float cachedDrive = -1.f, cachedGs = 1.f, cachedComp = 1.f;

    juce::SmoothedValue<float> driveSmoothed { 0.4f };
    std::vector<float> gainRamp, biasRamp, compRamp;
};
