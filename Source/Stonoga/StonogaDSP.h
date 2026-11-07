// Stonoga FX — rdzeń przetwarzania (bez zależności od JUCE).
// Kopia z github.com/rirmowy-droid/stonoga-fx (+ przełącznik builtInProtection).
// Przester do noise rocka: kaskadowy fuzz, zagłodzona bateria, rozpadająca się bramka,
// oktawa, nosowa "maska", rdza (bitcrush + decymacja), trzaski i sprzężenie zwrotne.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace stonoga {

constexpr float kPi = 3.14159265358979f;

inline float clampf(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }

struct Params {
    float drive    = 0.65f; // przester 0..1  (6..72 dB)
    float starve   = 0.0f;  // głód baterii: niższe szyny, asymetria, charczenie
    float gate     = 0.0f;  // bramka-rozpad: velcro/sputter przy wygasaniu
    float octave   = 0.0f;  // oktawa w górę (prostowanie)
    float mask     = 0.0f;  // nosowy filtr samogłosek
    float vowel    = 0.5f;  // u-o-a-e-i
    float rust     = 0.0f;  // bitcrush + decymacja
    float crackle  = 0.0f;  // trzaski zależne od sygnału
    float feedback = 0.0f;  // sprzężenie (może samo-oscylować)
    float fbTime   = 0.3f;  // czas sprzężenia 1..80 ms
    float tone     = 0.5f;  // barwa: ciemno <-> jasno
    float mix      = 1.0f;  // dry/wet
    float outDb    = -10.f; // wyjście
    float howl     = 0.0f;  // wycie: sprzężenie dostrojone do granej nuty, narasta przy wybrzmiewaniu
    float reverb   = 0.0f;  // ilość pogłosu
    float size     = 0.5f;  // czas pogłosu 0.5..20 s
    float sing     = 0.0f;  // śpiew: formanty + rezonans na wysokości nuty w ogonie
    float choir    = 0.0f;  // chór: oktawa i kwinta w pętli pogłosu (narasta bez końca)
    float revfb    = 0.0f;  // pętla: pogłos wraca do przesteru
    float chorus   = 0.0f;  // chorus (ilość)
    float chRate   = 0.3f;  // tempo chorusa 0.1..4 Hz
    float soft     = 0.0f;  // miękkość: ciepły overdrive zamiast kaskadowego fuzzu
    float swell    = 0.0f;  // mgła: nuty narastają (shoegaze)
    float body     = 0.0f;  // pełnia: dół + podoktawa
    float rev2     = 0.0f;  // pogłos 2 (jasny) — dostaje wyjście pogłosu 1
    float size2    = 0.5f;
    float rev3     = 0.0f;  // pogłos 3 (ciemna otchłań) — dostaje wyjście pogłosu 2
    float size3    = 0.7f;
    float protect  = 0.7f;  // ochrona uszu: przyciemnia bzyczenie i pisk, ścisza długo za głośny dźwięk
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void set(float nb0, float nb1, float nb2, float a0, float na1, float na2) {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
    static void wcs(float sr, float f, float q, float& c, float& al) {
        float w = 2 * kPi * clampf(f, 10.f, sr * 0.45f) / sr;
        c = std::cos(w); al = std::sin(w) / (2 * q);
    }
    void lowpass(float sr, float f, float q)  { float c, al; wcs(sr, f, q, c, al); set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass(float sr, float f, float q) { float c, al; wcs(sr, f, q, c, al); set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void bandpass(float sr, float f, float q) { float c, al; wcs(sr, f, q, c, al); set(al, 0, -al, 1 + al, -2 * c, 1 - al); }
    void peak(float sr, float f, float q, float db) {
        float c, al; wcs(sr, f, q, c, al); float A = std::pow(10.f, db / 40.f);
        set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf(float sr, float f, float db) {
        float c, al; wcs(sr, f, 0.7071f, c, al); float A = std::pow(10.f, db / 40.f), s = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
            (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf(float sr, float f, float db) {
        float c, al; wcs(sr, f, 0.7071f, c, al); float A = std::pow(10.f, db / 40.f), s = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
            (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
};

struct Delay {
    std::vector<float> b; size_t w = 0;
    void init(size_t n) { b.assign(n, 0.f); w = 0; }
    inline void push(float x) { b[w] = x; if (++w == b.size()) w = 0; }
    inline float tap(float d) const {
        float rp = (float)w - d; const float n = (float)b.size();
        while (rp < 0) rp += n;
        size_t i0 = (size_t)rp; if (i0 >= b.size()) i0 -= b.size();
        size_t i1 = i0 + 1; if (i1 == b.size()) i1 = 0;
        float fr = rp - (float)(size_t)rp;
        return b[i0] + (b[i1] - b[i0]) * fr;
    }
    void clear() { std::fill(b.begin(), b.end(), 0.f); }
};

inline const float kLen[8] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 79.3f}; // ms

struct Reverb {
    Delay line[8], pre, ap1, ap2, sh;
    float len[8] {}, g[8] {}, lp[8] {};
    float ap1M = 0, ap2M = 0, preM = 0, lfo = 0, ph1 = 0, ph2 = 0.5f, ph3 = 0, ph4 = 0.5f, shimFeed = 0, sr = 48000;
    void init(float rate) {
        sr = rate;
        for (auto& l : line) l.init((size_t)(0.0793f * 2.0f * sr + 0.012f * sr) + 16);
        preM = 0.018f * sr; ap1M = 0.0047f * sr; ap2M = 0.0113f * sr;
        pre.init((size_t)preM + 8); ap1.init((size_t)ap1M + 8); ap2.init((size_t)ap2M + 8);
        sh.init((size_t)(0.08f * sr) + 16);
    }
    void clear() { for (auto& l : line) l.clear(); pre.clear(); ap1.clear(); ap2.clear(); sh.clear(); for (float& v : lp) v = 0; shimFeed = 0; }
};

inline float allpass(Delay& d, float x, float M, float gg) {
    float dv = d.tap(M);
    float y = -gg * x + dv;
    d.push(x + gg * y);
    return y;
}

// jeden krok pogłosu FDN 8x8 (Householder) z chórem (oktawa + kwinta w pętli)
inline float reverbTick(Reverb& r, float x, float dampC, float choir, float win) {
    r.pre.push(x + r.shimFeed);
    float in = r.pre.tap(r.preM);
    in = allpass(r.ap1, in, r.ap1M, 0.62f);
    in = allpass(r.ap2, in, r.ap2M, 0.58f);
    r.lfo += 2.f * kPi * 0.4f / r.sr; if (r.lfo > 2.f * kPi) r.lfo -= 2.f * kPi;
    float v[8], sum = 0;
    const float md = 0.0012f * r.sr;
    for (int k = 0; k < 8; ++k) {
        float d = r.len[k] + (k < 4 ? md * (1.f + std::sin(r.lfo + 1.7f * (float)k)) : 0.f) + 2.f;
        float t = r.line[k].tap(d);
        r.lp[k] += dampC * (t - r.lp[k]);
        v[k] = r.lp[k] * r.g[k];
        sum += v[k];
    }
    sum *= 0.25f;
    static const float sg[8] = {1, -1, 1, -1, -1, 1, -1, 1};
    float out = 0;
    for (int k = 0; k < 8; ++k) {
        r.line[k].push(v[k] - sum + in * 0.35f * sg[k]);
        out += v[k] * ((k & 1) ? -0.35f : 0.35f);
    }
    // chór: przesuwanie wysokości ogona (oktawa i kwinta), zawracane do pętli
    float shOut = 0;
    if (choir > 0.001f) {
        r.sh.push(out);
        auto head = [&](float& ph, float ratio) {
            ph += (ratio - 1.f) / win; if (ph >= 1.f) ph -= 1.f;
            float d = win * (1.f - ph) + 2.f;
            return r.sh.tap(d) * std::sin(kPi * ph);
        };
        float oct = head(r.ph1, 2.f) + head(r.ph2, 2.f);
        float fif = head(r.ph3, 1.5f) + head(r.ph4, 1.5f);
        shOut = oct * 0.55f + fif * 0.45f;
        r.shimFeed = shOut * choir * 0.5f;
    } else r.shimFeed = 0;
    return out + shOut * choir * 0.6f;
}

class Engine {
public:
    // Stonoga x Albini: ochrona uszu działa na końcu całego łańcucha, więc tu ją wyłączamy.
    bool builtInProtection = true;

    void prepare(double sampleRate, int numChannels) {
        sr = (float)sampleRate;
        ch.assign((size_t)std::max(1, numChannels), Chan{});
        uint32_t seed = 0x9E3779B9u;
        for (auto& c : ch) {
            c.fb.assign((size_t)(sr * 0.1f) + 8, 0.f);
            c.rv.init(sr); c.rv2.init(sr); c.rv3.init(sr);
            c.hfHp.highpass(sr, 3500.f, 0.7f);
            c.guardShelf.highshelf(sr, 2800.f, 0.f);
            c.cd.init((size_t)(0.045f * sr) + 8);
            c.trk.lowpass(sr, 900.f, 0.7f);
            c.rng = (seed += 0x7F4A7C15u);
        }
        smooth = Params{};
        first = true;
    }

    // io: kanały (już nadpróbkowane), przetwarzanie w miejscu.
    void process(float* const* io, int numCh, int n, const Params& target) {
        if (ch.empty() || n <= 0) return;
        // wygładzanie parametrów raz na blok (bloki nadpróbkowane są krótkie)
        auto sm = [&](float& a, float b) { a = first ? b : a + (b - a) * 0.35f; };
        sm(smooth.drive, target.drive);   sm(smooth.starve, target.starve); sm(smooth.gate, target.gate);
        sm(smooth.octave, target.octave); sm(smooth.mask, target.mask);     sm(smooth.vowel, target.vowel);
        sm(smooth.rust, target.rust);     sm(smooth.crackle, target.crackle); sm(smooth.feedback, target.feedback);
        sm(smooth.fbTime, target.fbTime); sm(smooth.tone, target.tone);     sm(smooth.mix, target.mix);
        sm(smooth.outDb, target.outDb);   sm(smooth.howl, target.howl);     sm(smooth.reverb, target.reverb);
        sm(smooth.size, target.size);     sm(smooth.sing, target.sing);     sm(smooth.choir, target.choir);
        sm(smooth.revfb, target.revfb);   sm(smooth.chorus, target.chorus); sm(smooth.chRate, target.chRate);
        sm(smooth.soft, target.soft);     sm(smooth.swell, target.swell);   sm(smooth.body, target.body);
        sm(smooth.rev2, target.rev2);     sm(smooth.size2, target.size2);   sm(smooth.rev3, target.rev3);
        sm(smooth.size3, target.size3);   sm(smooth.protect, target.protect);
        first = false;
        const Params& p = smooth;

        // --- wartości na blok ---
        const float totalDb = 6.f + p.drive * 66.f;
        const float g = std::pow(10.f, totalDb / 20.f);
        const float g1 = std::pow(g, 0.5f), g2 = std::pow(g, 0.3f), g3 = std::pow(g, 0.2f);
        const float rail = 1.f - 0.82f * p.starve;
        const float bias = p.starve * 0.5f;
        const float makeup = 1.f / (rail * (1.f + 0.6f * p.starve));
        const float fold = clampf((p.drive - 0.7f) / 0.3f, 0.f, 1.f) * (1.f - p.soft);
        const float gs = std::pow(10.f, (3.f + p.drive * 38.f) / 20.f);
        const float swellK = 1.f / ((0.03f + p.swell * 0.75f) * sr);
        const float chHz = 0.1f + p.chRate * p.chRate * 3.9f;
        const float msS = 0.001f * sr;
        const float gateThr = std::max(p.gate * 0.06f, p.starve * p.starve * 0.025f);
        const bool gateOn = gateThr > 1e-5f;
        const int fbDelay = std::max(1, (int)(sr * (0.001f + p.fbTime * 0.079f)));
        const float fbAmt = p.feedback * 1.25f;
        const float bits = 12.f - p.rust * 10.5f;
        const float levels = std::pow(2.f, bits);
        const int holdN = 1 + (int)(p.rust * p.rust * 60.f);
        const float crackP = p.crackle * p.crackle * 0.0025f * (192000.f / sr);
        const float outG = std::pow(10.f, p.outDb / 20.f);
        const float atk = 1.f - std::exp(-1.f / (0.001f * sr));
        const float rel = 1.f - std::exp(-1.f / (0.09f * sr));
        const float rt60 = 0.5f * std::pow(40.f, p.size);
        const float lineScale = 0.7f + p.size * 0.9f;
        const float dampC = 1.f - std::exp(-2.f * kPi * (2500.f + 5000.f * (1.f - p.size * 0.6f)) / sr);
        const bool revOn = p.reverb > 0.001f || p.revfb > 0.001f;
        const bool rev2On = p.rev2 > 0.001f, rev3On = p.rev3 > 0.001f;
        const float rt2 = 0.4f * std::pow(30.f, p.size2), rt3 = 0.8f * std::pow(40.f, p.size3);
        const float damp2 = 1.f - std::exp(-2.f * kPi * 9000.f / sr), damp3 = 1.f - std::exp(-2.f * kPi * (1400.f + 1600.f * (1.f - p.size3)) / sr);
        const float shWin = 0.07f * sr;
        const float sixtyFour = 64.f / sr;
        const int chatterN = std::max(1, (int)(sr / (1500.f + 5000.f * (1.f - p.gate))));

        for (int c = 0; c < std::min(numCh, (int)ch.size()); ++c) {
            Chan& s = ch[(size_t)c];
            // filtry zależne od parametrów
            s.pre.peak(sr, 850.f, 0.8f, 3.f + 7.f * p.drive);
            s.inHp.highpass(sr, 35.f, 0.7f);
            s.inter1.lowpass(sr, 7000.f, 0.7f);
            s.inter2.lowpass(sr, 5200.f - 1500.f * p.starve, 0.7f);
            s.fbHp.highpass(sr, 280.f, 0.7f);
            s.crackBp.bandpass(sr, 3200.f, 1.3f);
            const float tilt = (p.tone - 0.5f) * 22.f;
            s.lowS.lowshelf(sr, 650.f, -tilt);
            s.highS.highshelf(sr, 900.f, tilt);
            s.post.lowpass(sr, 7000.f + 9000.f * p.tone, 0.7f);
            s.softLp.lowpass(sr, 3800.f + 3000.f * p.tone, 0.6f);
            s.bodyS.lowshelf(sr, 170.f, p.body * 9.f);
            s.subLp.lowpass(sr, 260.f, 0.7f);

            if (rev2On) for (int k = 0; k < 8; ++k) { s.rv2.len[k] = kLen[k] * 1.13f * (0.6f + p.size2 * 0.9f) * 0.001f * sr + (float)c * 0.0023f * sr; s.rv2.g[k] = std::pow(10.f, -3.f * s.rv2.len[k] / (rt2 * sr)); }
            if (rev3On) for (int k = 0; k < 8; ++k) { s.rv3.len[k] = kLen[k] * 0.87f * (0.9f + p.size3 * 0.8f) * 0.001f * sr + (float)c * 0.0031f * sr; s.rv3.g[k] = std::pow(10.f, -3.f * s.rv3.len[k] / (rt3 * sr)); }
            if (revOn) for (int k = 0; k < 8; ++k) { s.rv.len[k] = kLen[k] * lineScale * 0.001f * sr + (float)c * 0.0017f * sr; s.rv.g[k] = std::pow(10.f, -3.f * s.rv.len[k] / (rt60 * sr)); }
            float* x = io[c];
            const size_t N = s.fb.size();
            for (int i = 0; i < n; ++i) {
                const float dry = x[i];
                float v = s.inHp.process(dry);

                // śledzenie wysokości nuty (przejścia przez zero) i obwiednia wejścia
                {
                    float t = s.trk.process(v);
                    float ai = std::fabs(v); s.envIn += (ai > s.envIn ? atk : rel) * (ai - s.envIn);
                    s.sinceX++;
                    const float hy = 0.002f + s.envIn * 0.15f;
                    if (s.zc <= 0 && t > hy) {
                        s.zc = 1;
                        if (s.sinceX > (int)(sr / 1300.f) && s.sinceX < (int)(sr / 50.f)) s.trackF += (sr / (float)s.sinceX - s.trackF) * 0.25f;
                        s.sinceX = 0;
                        s.sub = -s.sub;
                    } else if (s.zc > 0 && t < -hy) s.zc = -1;
                    // nowa nuta -> mgła zaczyna narastać od zera
                    s.envF += (ai - s.envF) * (ai > s.envF ? 0.02f * 48000.f / sr : 0.0015f * 48000.f / sr);
                    s.envS += (ai - s.envS) * (0.00012f * 48000.f / sr);
                    s.sinceOn++;
                    if (s.envF > 0.008f && s.envF > s.envS * 1.9f && s.sinceOn > (int)(0.3f * sr)) { s.swellG = std::min(s.swellG, 0.12f + 0.6f * clampf(s.envS * 6.f, 0.f, 1.f)); s.sinceOn = 0; }
                    s.swellG += (1.f - s.swellG) * swellK;
                    s.bloom = clampf(s.bloom + (s.envIn > 0.006f ? 1.f / (1.3f * sr) : -1.f / (0.45f * sr)), 0.f, 1.f);
                }
                if ((s.slowCnt++ & 63) == 0) slowUpdate(s, p, sixtyFour);

                // wycie: pętla o długości okresu nuty / harmoniczna
                if (p.howl > 0.001f && s.bloom > 0.f) {
                    float d = clampf(sr / (s.trackF * s.H), 4.f, (float)N - 4.f);
                    float rp = (float)s.fbPos - d; while (rp < 0) rp += (float)N;
                    size_t i0 = (size_t)rp; float fr = rp - (float)i0; if (i0 >= N) i0 -= N;
                    float h = s.fb[i0] + (s.fb[(i0 + 1) % N] - s.fb[i0]) * fr;
                    h = s.howlBp.process(h);
                    v += p.howl * 1.6f * s.bloom * std::tanh(h * 2.5f);
                }
                // pętla pogłosu
                if (p.revfb > 0.001f) v += p.revfb * 1.3f * std::tanh(s.revLast * 1.5f);

                // sprzężenie zwrotne
                if (fbAmt > 0.001f) {
                    size_t rp = (s.fbPos + N - (size_t)fbDelay) % N;
                    float fb = s.fbHp.process(s.fb[rp]);
                    v += fbAmt * std::tanh(fb * 1.6f);
                }

                // obwiednia
                float a = std::fabs(v);
                s.env += (a > s.env ? atk : rel) * (a - s.env);

                // co 32 próbki: samogłoska maski podąża za kostką
                if ((s.vowelCnt++ & 31) == 0 && p.mask > 0.001f) setVowel(s, clampf(p.vowel + p.mask * 0.35f * clampf(s.env * 5.f, 0.f, 1.f), 0.f, 1.f));

                // stopnie przesteru
                v = s.pre.process(v);
                const float vPre = v;
                v = std::tanh(g1 * v + bias) - std::tanh(bias);
                v = s.inter1.process(v);
                v *= g2;
                v = v > 0 ? v / (1.f + v) : v / (1.f + 0.45f * std::fabs(v)) * 0.8f;
                v = s.inter2.process(v);
                v *= g3;
                float hard = clampf(v, -rail, rail * (1.f - 0.3f * p.starve));
                float folded = std::sin(v * kPi * 0.5f * (1.f + fold * 3.f)) * rail;
                v = hard + fold * (folded - hard);
                v *= makeup;
                if (p.soft > 0.001f) {
                    float w = gs * vPre + 0.25f * bias;
                    float sv = std::tanh(w) * 0.75f + 0.25f * w / (1.f + std::fabs(w));
                    sv = s.softLp.process(sv) * 1.15f;
                    v += p.soft * (sv - v);
                }

                // bramka-rozpad (velcro / zdychająca bateria)
                if (gateOn) {
                    float open = clampf((s.env - gateThr) / (gateThr * 0.7f + 1e-6f), 0.f, 1.f);
                    if (++s.chat >= chatterN) { s.chat = 0; s.gateT = (rnd(s) < open * open) ? 1.f : 0.f; }
                    s.gateG += (s.gateT - s.gateG) * 0.35f;
                    v *= s.gateG;
                }

                // oktawa
                if (p.octave > 0.001f) {
                    float oct = std::fabs(v) * 2.f - 1.f;
                    oct = oct - s.octX + 0.999f * s.octY; s.octX = std::fabs(v) * 2.f - 1.f; s.octY = oct;
                    v += p.octave * (oct * 1.4f - v);
                }

                // maska (nosowe formanty)
                if (p.mask > 0.001f) {
                    float m = s.f1.process(v) * 3.2f + s.f2.process(v) * 2.2f;
                    m = std::tanh(m * 2.6f);
                    v += p.mask * (m - v);
                }

                // rdza
                if (p.rust > 0.001f) {
                    float cr = std::round(v * levels) / levels;
                    if (++s.holdCnt >= holdN) { s.holdCnt = 0; s.holdV = cr; }
                    float rv = holdN > 1 ? s.holdV : cr;
                    v += std::min(1.f, p.rust * 3.f) * (rv - v);
                }

                // trzaski (gęstsze przy mocnym graniu)
                if (crackP > 0.f) {
                    float imp = 0.f;
                    if (rnd(s) < crackP * (0.15f + clampf(s.env * 8.f, 0.f, 3.f))) imp = (rnd(s) * 2.f - 1.f) * (0.7f + p.crackle);
                    v += s.crackBp.process(imp) * 3.5f;
                }

                // zapis do linii sprzężenia
                s.fb[s.fbPos] = v;
                s.fbPos = (s.fbPos + 1) % N;

                // barwa
                v = s.lowS.process(v);
                v = s.highS.process(v);
                v = s.post.process(v);
                // pełnia: dół + podoktawa śledząca grę
                if (p.body > 0.001f) {
                    v = s.bodyS.process(v);
                    float sb = s.subLp.process((float)s.sub) * clampf(s.envIn * 7.f, 0.f, 1.f);
                    v += p.body * 0.45f * sb;
                }
                // mgła (narastanie nut)
                if (p.swell > 0.001f) v *= 1.f - p.swell + p.swell * s.swellG * s.swellG;
                // chorus: trzy głosy, kanały w przeciwnych fazach
                if (p.chorus > 0.001f) {
                    s.cd.push(v);
                    s.cph += 2.f * kPi * chHz / sr; if (s.cph > 2.f * kPi) s.cph -= 2.f * kPi;
                    float acc = 0;
                    for (int k = 0; k < 3; ++k) {
                        float ph = s.cph + (float)k * 2.094f + (float)c * kPi * 0.5f;
                        acc += s.cd.tap(msS * (11.f + 3.f * (float)k + (2.f + 4.f * p.chorus) * (1.f + std::sin(ph))));
                    }
                    v = v * (1.f - 0.45f * p.chorus) + acc * (0.33f * p.chorus) * 1.1f;
                }
                // śpiewający pogłos
                if (revOn) {
                    float tail = reverbTick(s.rv, v * 0.5f, dampC, p.choir, shWin);
                    float sg = tail;
                    if (p.sing > 0.001f) {
                        float f = s.sf1.process(tail) * 3.f + s.sf2.process(tail) * 2.f + s.sf3.process(tail) * 0.8f;
                        f += (s.sr1.process(tail) + s.sr2.process(tail) * 0.7f + s.sr3.process(tail) * 0.5f) * 1.6f;
                        s.vib += 2.f * kPi * 5.4f / sr; if (s.vib > 2.f * kPi) s.vib -= 2.f * kPi;
                        f *= 1.f + 0.18f * std::sin(s.vib);
                        sg = tail + p.sing * (f - tail);
                    }
                    s.revLast = sg;
                    v = v * (1.f - 0.35f * p.reverb * p.reverb) + p.reverb * (1.f + 1.6f * p.reverb) * sg;
                }
                // pogłos 2 i 3 w szeregu: każdy dostaje to, co wyszło z poprzedniego
                if (rev2On) { float t2 = reverbTick(s.rv2, v * 0.5f, damp2, 0.f, shWin); v = v * (1.f - 0.3f * p.rev2 * p.rev2) + p.rev2 * (1.f + 1.4f * p.rev2) * t2; }
                if (rev3On) { float t3 = reverbTick(s.rv3, v * 0.5f, damp3, 0.f, shWin); v = v * (1.f - 0.3f * p.rev3 * p.rev3) + p.rev3 * (1.2f + 1.6f * p.rev3) * t3; }
                // DC
                float y = v - s.dcX + 0.9995f * s.dcY; s.dcX = v; s.dcY = y;
                y *= outG;
                if (!std::isfinite(y)) { y = 0.f; s.resetFilters(); }
                y = clampf(y, -4.f, 4.f);
                float o = dry + p.mix * (y - dry);
                x[i] = builtInProtection ? protectTick(s, o, p.protect) : o;
            }
        }
    }

private:
    struct Chan {
        Biquad inHp, pre, inter1, inter2, f1, f2, fbHp, crackBp, lowS, highS, post;
        Biquad trk, howlBp, sf1, sf2, sf3, sr1, sr2, sr3;
        Reverb rv, rv2, rv3;
        Biquad hfHp, guardShelf;
        float msAll = 0, msHf = 0, cutDb = 0, guardG = 1.f, limEnv = 0; uint32_t guardCnt = 0;
        Biquad softLp, bodyS, subLp; Delay cd;
        float sub = 1.f, envF = 0, envS = 0, swellG = 1.f, cph = 0; int sinceOn = 0;
        float envIn = 0, trackF = 110.f, bloom = 0, H = 2.f, revLast = 0, vib = 0, vph = 0;
        int zc = 0, sinceX = 0; uint32_t slowCnt = 0;
        std::vector<float> fb; size_t fbPos = 0;
        float env = 0, gateG = 1, gateT = 1, holdV = 0, octX = 0, octY = 0, dcX = 0, dcY = 0;
        int chat = 0, holdCnt = 0; uint32_t vowelCnt = 0, rng = 1;
        void resetFilters() {
            for (Biquad* b : {&inHp, &pre, &inter1, &inter2, &f1, &f2, &fbHp, &crackBp, &lowS, &highS, &post}) b->reset();
            std::fill(fb.begin(), fb.end(), 0.f); env = 0; dcX = dcY = 0; octX = octY = 0;
            for (Biquad* b : {&trk, &howlBp, &sf1, &sf2, &sf3, &sr1, &sr2, &sr3}) b->reset();
            hfHp.reset(); guardShelf.reset(); msAll = msHf = 0; cutDb = 0; guardG = 1.f; limEnv = 0;
            rv.clear(); rv2.clear(); rv3.clear(); revLast = 0; bloom = 0; cd.clear();
            for (Biquad* b : {&softLp, &bodyS, &subLp}) b->reset();
        }
    };

    static float rnd(Chan& s) {
        s.rng ^= s.rng << 13; s.rng ^= s.rng >> 17; s.rng ^= s.rng << 5;
        return (s.rng & 0xFFFFFF) / 16777216.f;
    }

    void setVowel(Chan& s, float v) {
        static const float F[5][2] = {{350, 700}, {500, 900}, {800, 1150}, {500, 1750}, {300, 2300}};
        float t = v * 4.f; int k = std::min(3, (int)t); float f = t - (float)k;
        float a = F[k][0] + (F[k + 1][0] - F[k][0]) * f, b = F[k][1] + (F[k + 1][1] - F[k][1]) * f;
        s.f1.bandpass(sr, a, 6.f);
        s.f2.bandpass(sr, b, 8.f);
    }

    // ochrona uszu: dynamiczne przyciemnianie góry + powolne ściszanie + limiter na wyjściu
    float protectTick(Chan& s, float o, float prot) {
        const float a300 = 1.f / (0.3f * sr), a50 = 1.f / (0.05f * sr);
        s.msAll += (o * o - s.msAll) * a300;
        float hf = s.hfHp.process(o);
        s.msHf += (hf * hf - s.msHf) * a50;
        if ((s.guardCnt++ & 63) == 0) {
            const float rms = std::sqrt(s.msAll) + 1e-6f, hfr = std::sqrt(s.msHf);
            const float ratio = hfr / rms;
            float tgt = prot * (clampf((ratio - 0.35f) / 0.3f, 0.f, 1.f) * 18.f + clampf((hfr - 0.07f) / 0.15f, 0.f, 1.f) * 12.f);
            tgt = std::min(tgt, 24.f);
            const float dt = 64.f / sr;
            s.cutDb += (tgt - s.cutDb) * (tgt > s.cutDb ? dt / 0.08f : dt / 0.8f);
            s.guardShelf.highshelf(sr, 2800.f, -s.cutDb);
            const float thr = 0.35f;
            float gT = rms > thr ? std::pow(thr / rms, 0.8f) : 1.f;
            gT = 1.f - prot * (1.f - gT);
            s.guardG += (gT - s.guardG) * (gT < s.guardG ? dt / 1.2f : dt / 2.f);
        }
        float y = s.guardShelf.process(o) * s.guardG;
        // limiter szczytowy: sufit -1 dBFS
        const float ceil = 0.89f, a = std::fabs(y);
        s.limEnv = a > s.limEnv ? a : s.limEnv + (a - s.limEnv) * (1.f / (0.12f * sr));
        if (s.limEnv > ceil) y *= ceil / s.limEnv;
        return ceil * std::tanh(y / ceil);
    }

    // co 64 próbki: formanty śpiewu, rezonatory na wysokości nuty, przeskoki harmonicznej wycia
    void slowUpdate(Chan& s, const Params& p, float dtSlow) {
        if (p.sing > 0.001f && (p.reverb > 0.001f || p.revfb > 0.001f)) {
            s.vph += 2.f * kPi * 0.09f * dtSlow; if (s.vph > 2.f * kPi) s.vph -= 2.f * kPi;
            float v = clampf(0.5f + 0.42f * std::sin(s.vph) + 0.12f * std::sin(s.vph * 2.7f + 1.f), 0.f, 1.f);
            static const float F[5][2] = {{350, 700}, {500, 900}, {800, 1150}, {500, 1750}, {300, 2300}};
            float t = v * 4.f; int k = std::min(3, (int)t); float f = t - (float)k;
            s.sf1.bandpass(sr, F[k][0] + (F[k + 1][0] - F[k][0]) * f, 7.f);
            s.sf2.bandpass(sr, F[k][1] + (F[k + 1][1] - F[k][1]) * f, 9.f);
            s.sf3.bandpass(sr, 2500.f + 400.f * v, 10.f);
            s.sr1.bandpass(sr, s.trackF, 25.f);
            s.sr2.bandpass(sr, s.trackF * 2.f, 25.f);
            s.sr3.bandpass(sr, s.trackF * 3.f, 25.f);
        }
        if (p.howl > 0.001f) {
            if (s.bloom > 0.95f && rnd(s) < dtSlow / 2.2f) {
                static const float Hs[5] = {1.5f, 2.f, 3.f, 4.f, 5.f};
                s.H = Hs[(int)(rnd(s) * 5.f) % 5];
            }
            s.howlBp.bandpass(sr, clampf(s.trackF * s.H, 80.f, 5000.f), 3.f);
        }
    }

    float sr = 192000.f;
    std::vector<Chan> ch;
    Params smooth;
    bool first = true;
};

} // namespace stonoga
