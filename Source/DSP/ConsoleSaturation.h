#pragma once

#include "DSPUtils.h"
#include <vector>
#include <memory>

/** Konsoleta: tor kanału + transformatory.

    Low shelf w górę -> nasycenie (2x OS) -> dokładnie odwrotny low shelf:
    dół „uderza w żelazo” mocniej niż środek. Do tego model konsolety:

      0 Ogólna        - jak w wersji 1.x
      1 Brytyjska     - w stylu dużych Neve z lat 70./80. (transformatory na wejściu
                        i wyjściu): nasycenie najmocniej w dole, dużo parzystych
                        harmonicznych, lekko pogrubiony niski środek, zaokrąglona góra
      2 Amerykańska   - w stylu dyskretnych API: mniej dołu w nasyceniu, twardsze
                        obcinanie (nieparzyste harmoniczne), wypchnięta średnica
      3 Czysta in-line- w stylu SSL / Neotek: mało koloru, szybka i przejrzysta

    Na 0 tor jest liniowy (nasycenie wchodzi płynnie w domenie OS, więc opóźnienie
    jest stałe). */
class ConsoleSaturation
{
public:
    enum Model { Generic = 0, British, American, CleanInline, NumModels };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    void setAmount (float amount01) { amount.setTargetValue (juce::jlimit (0.0f, 1.0f, amount01)); }
    void setModel (int m) { model = juce::jlimit (0, (int) NumModels - 1, m); }
    float getLatencyInSamples() const { return oversampling ? oversampling->getLatencyInSamples() : 0.0f; }

private:
    struct Voicing
    {
        float shelfHz, shelfDb;   // ile dołu „uderza w żelazo”
        float driveMax, biasMax;  // nasycenie i asymetria (parzyste)
        float hardness;           // 0 = tanh, 1 = twarde obcinanie
        float hfMinHz;            // utrata góry przy pełnej gałce
        float colourHz, colourQ, colourDb; // stały „charakter” w paśmie (skalowany gałką)
    };
    static const Voicing& voicing (int m);
    void applyModel();

    double sampleRate = 44100.0;
    int model = Generic, appliedModel = -1;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    albini::StereoIIR preShelf, postShelf, colour;
    float lastColourAmount = -1.0f;

    std::vector<albini::OnePole> hfSoften;
    std::vector<albini::DCBlocker> dc;
    float lastHfAmount = -1.0f;

    juce::SmoothedValue<float> amount { 0.25f };
    std::vector<float> driveRamp, biasRamp, normRamp, wetRamp;
};
