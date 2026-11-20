// CONSTELLATION — DSP building blocks (header-only, allocation-free in process()).
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include <array>

namespace cst
{
constexpr float kPi = 3.14159265358979f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }
inline float sat01 (float x)     noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// ───────────────────────────── Biquad (RBJ cookbook, TDF-II) ─────────────────────────────
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() noexcept { z1 = z2 = 0; }

    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2) noexcept
    {
        const float ia = 1.0f / na0;
        b0 = nb0 * ia; b1 = nb1 * ia; b2 = nb2 * ia; a1 = na1 * ia; a2 = na2 * ia;
    }
    void lowpass (double sr, float f, float q = 0.7071f)
    {
        f = juce::jlimit (10.0f, (float) (sr * 0.45), f);
        const float w = 2 * kPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double sr, float f, float q = 0.7071f)
    {
        f = juce::jlimit (10.0f, (float) (sr * 0.45), f);
        const float w = 2 * kPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double sr, float f, float q)
    {
        f = juce::jlimit (10.0f, (float) (sr * 0.45), f);
        const float w = 2 * kPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double sr, float f, float q, float db)
    {
        f = juce::jlimit (10.0f, (float) (sr * 0.45), f);
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / (float) sr;
        const float c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double sr, float f, float db)
    {
        f = juce::jlimit (10.0f, (float) (sr * 0.45), f);
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0f), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double sr, float f, float db)
    {
        f = juce::jlimit (10.0f, (float) (sr * 0.45), f);
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0f), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

struct OnePoleLP
{
    float a = 0, z = 0;
    void set (double sr, float f) noexcept { a = std::exp (-2.0f * kPi * juce::jlimit (5.0f, (float) (sr * 0.49), f) / (float) sr); }
    inline float process (float x) noexcept { z = x + a * (z - x); return z; }
    void clear() noexcept { z = 0; }
};

// ───────────────────────────── Fractional delay line ─────────────────────────────
struct DelayLine
{
    std::vector<float> buf; int w = 0, mask = 0;
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float tap (float d) const noexcept      // d in samples (>= 1)
    {
        const int   i = (int) d; const float fr = d - (float) i;
        const float a = buf[(size_t) ((w - 1 - i) & mask)], b = buf[(size_t) ((w - 2 - i) & mask)];
        return a + fr * (b - a);
    }
    inline float tapInt (int d) const noexcept { return buf[(size_t) ((w - 1 - d) & mask)]; }
};

// ───────────────────────────── Rooms ─────────────────────────────
struct RoomDef { const char* en; const char* es; float size, t60, damp, er, predelay; };
inline constexpr RoomDef kRooms[] = {
    { "Dry Booth",   "Cabina seca",  0.35f, 0.25f, 11000.f, 0.55f, 1.0f },
    { "Studio",      "Estudio",      0.60f, 0.55f,  9000.f, 0.60f, 4.0f },
    { "Club",        "Club",         1.00f, 1.20f,  6000.f, 0.45f, 8.0f },
    { "Plate",       "Placa",        0.80f, 1.80f, 12000.f, 0.10f, 2.0f },
    { "Concert Hall","Sala de conciertos", 1.60f, 2.40f, 7000.f, 0.40f, 14.0f },
    { "Cenote",      "Cenote",       2.00f, 3.80f,  4500.f, 0.35f, 18.0f },
    { "Cathedral",   "Catedral",     2.30f, 5.00f,  5000.f, 0.30f, 22.0f },
    { "Nebula",      "Nebulosa",     2.60f, 9.00f,  9500.f, 0.12f, 30.0f },
};
inline constexpr int kNumRooms = (int) (sizeof (kRooms) / sizeof (kRooms[0]));

// 4-line FDN + early reflections. Mono in, stereo out.
struct RoomReverb
{
    double sr = 48000;
    DelayLine pre, lines[4];
    OnePoleLP damp[4];
    float g[4] {}; int len[4] {};
    int   roomIdx = -1;
    float erGain = 0.5f; float predelayMs = 4.0f;
    static constexpr float kBase[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
    static constexpr float kEr[6]   = { 7.1f, 11.3f, 17.9f, 23.3f, 31.7f, 41.9f };
    static constexpr float kErG[6]  = { 0.8f, -0.7f, 0.6f, -0.5f, 0.4f, -0.33f };
    float erScale = 1.0f;

    void prepare (double s)
    {
        sr = s;
        pre.prepare ((int) (sr * 0.25));
        for (auto& l : lines) l.prepare ((int) (sr * 0.16));
        roomIdx = -1;
    }
    void clear() { pre.clear(); for (auto& l : lines) l.clear(); for (auto& d : damp) d.clear(); }

    void setRoom (int idx)
    {
        idx = juce::jlimit (0, kNumRooms - 1, idx);
        if (idx == roomIdx) return;
        roomIdx = idx;
        const auto& r = kRooms[idx];
        for (int i = 0; i < 4; ++i)
        {
            const float ms = kBase[i] * r.size;
            len[i] = juce::jmax (8, (int) (ms * 0.001 * sr));
            g[i] = std::pow (10.0f, -3.0f * (ms * 0.001f) / r.t60);
            damp[i].set (sr, r.damp);
        }
        erGain = r.er; erScale = r.size; predelayMs = r.predelay;
    }

    // closeness 0..1 (1 = near the listener → longer gap before the reverb)
    inline void process (float in, float closeness, float& outL, float& outR) noexcept
    {
        pre.push (in);
        const float pd = (float) ((predelayMs * (0.4f + 0.6f * closeness) + 1.0f) * 0.001 * sr);
        const float x  = pre.tap (pd);

        float erL = 0, erR = 0;
        for (int k = 0; k < 6; ++k)
        {
            const float t = pre.tap (pd + (float) (kEr[k] * erScale * 0.001 * sr));
            if (k & 1) erR += t * kErG[k]; else erL += t * kErG[k];
        }

        float o[4];
        for (int i = 0; i < 4; ++i) o[i] = damp[i].process (lines[i].tapInt (len[i] - 1)) * g[i];
        // Hadamard feedback
        const float h0 = 0.5f * ( o[0] + o[1] + o[2] + o[3]);
        const float h1 = 0.5f * ( o[0] - o[1] + o[2] - o[3]);
        const float h2 = 0.5f * ( o[0] + o[1] - o[2] - o[3]);
        const float h3 = 0.5f * ( o[0] - o[1] - o[2] + o[3]);
        lines[0].push (x + h0); lines[1].push (x + h1); lines[2].push (x + h2); lines[3].push (x + h3);

        outL = (o[0] + o[2]) * 0.6f + erL * erGain * 0.5f;
        outR = (o[1] + o[3]) * 0.6f + erR * erGain * 0.5f;
    }
};

// ───────────────────────────── Binaural (spherical-head model) ─────────────────────────────
// Woodworth ITD + head-shadow ILD + rear and elevation colouration. Cheap and phase-coherent.
struct BinauralSource
{
    double sr = 48000;
    DelayLine d;
    OnePoleLP shadowL, shadowR, backL, backR;
    float curDl = 0, curDr = 0, tgtDl = 0, tgtDr = 0;
    float nearL = 1, nearR = 1, back = 0;

    void prepare (double s) { sr = s; d.prepare ((int) (sr * 0.004)); curDl = curDr = tgtDl = tgtDr = 1; }
    void clear() { d.clear(); shadowL.clear(); shadowR.clear(); backL.clear(); backR.clear(); }

    void setDirection (float azDeg, float elDeg)
    {
        const float az  = azDeg * kPi / 180.0f;
        const float el  = elDeg * kPi / 180.0f;
        const float lat = std::sin (az) * std::cos (el);               // -1 left … +1 right
        const float th  = std::asin (juce::jlimit (-1.0f, 1.0f, lat));
        const float itd = (0.0875f / 343.0f) * (std::abs (std::sin (th)) + std::abs (th)); // seconds
        const float itdS = itd * (float) sr;
        tgtDl = 1.0f + (lat > 0 ? itdS : 0.0f);
        tgtDr = 1.0f + (lat < 0 ? itdS : 0.0f);
        const float shadowAmt = std::abs (lat);
        const float fFar = 20000.0f * std::pow (0.075f, std::pow (shadowAmt, 1.2f)); // ~1.5k at 90°
        shadowL.set (sr, lat > 0 ? fFar : 20000.0f);
        shadowR.set (sr, lat < 0 ? fFar : 20000.0f);
        nearL = lat < 0 ? 1.0f + 0.25f * shadowAmt : 1.0f - 0.25f * shadowAmt;
        nearR = lat > 0 ? 1.0f + 0.25f * shadowAmt : 1.0f - 0.25f * shadowAmt;
        back = sat01 (-std::cos (az)) * 0.55f;
        backL.set (sr, 3500.0f); backR.set (sr, 3500.0f);
    }

    inline void process (float x, float& l, float& r) noexcept
    {
        d.push (x);
        curDl += 0.002f * (tgtDl - curDl);
        curDr += 0.002f * (tgtDr - curDr);
        float el = shadowL.process (d.tap (curDl)) * nearL;
        float er = shadowR.process (d.tap (curDr)) * nearR;
        el = el * (1.0f - back) + backL.process (el) * back;
        er = er * (1.0f - back) + backR.process (er) * back;
        l += el; r += er;
    }
};

// ───────────────────────────── Speaker panner (pairwise, two layers) ─────────────────────────────
struct Speaker { int channel; float az; bool top; bool lfe; };

struct SpeakerLayout
{
    std::vector<Speaker> ear, top;
    std::vector<int> lfe;
    int numChannels = 2;

    void build (const juce::AudioChannelSet& set)
    {
        ear.clear(); top.clear(); lfe.clear();
        numChannels = set.size();
        for (int i = 0; i < set.size(); ++i)
        {
            using CS = juce::AudioChannelSet;
            const auto t = set.getTypeOfChannel (i);
            float az = 0; bool isTop = false, known = true;
            switch (t)
            {
                case CS::left:             az = -30;  break;
                case CS::right:            az =  30;  break;
                case CS::centre:           az =   0;  break;
                case CS::leftSurround:     az = set.size() >= 8 ? -90 : -110; break;
                case CS::rightSurround:    az = set.size() >= 8 ?  90 :  110; break;
                case CS::leftSurroundSide: az = -90;  break;
                case CS::rightSurroundSide:az =  90;  break;
                case CS::leftSurroundRear: az = -150; break;
                case CS::rightSurroundRear:az =  150; break;
                case CS::topFrontLeft:     az = -45; isTop = true; break;
                case CS::topFrontRight:    az =  45; isTop = true; break;
                case CS::topRearLeft:      az = -135; isTop = true; break;
                case CS::topRearRight:     az =  135; isTop = true; break;
                case CS::topMiddle:        az =   0; isTop = true; break;
                case CS::LFE: case CS::LFE2: lfe.push_back (i); known = false; break;
                default: known = false; break;
            }
            if (set == CS::quadraphonic())
                az = (t == CS::left ? -45.f : t == CS::right ? 45.f : t == CS::leftSurround ? -135.f : 135.f);
            if (known) (isTop ? top : ear).push_back ({ i, az, isTop, false });
        }
        auto byAz = [] (const Speaker& a, const Speaker& b) { return a.az < b.az; };
        std::sort (ear.begin(), ear.end(), byAz);
        std::sort (top.begin(), top.end(), byAz);
    }

    static void pairPan (const std::vector<Speaker>& ring, float az, float scale, float* gains)
    {
        if (ring.empty()) return;
        if (ring.size() == 1) { gains[ring[0].channel] += scale; return; }
        auto wrap = [] (float a) { while (a < -180) a += 360; while (a >= 180) a -= 360; return a; };
        az = wrap (az);
        const size_t n = ring.size();
        for (size_t i = 0; i < n; ++i)
        {
            const auto& a = ring[i]; const auto& b = ring[(i + 1) % n];
            float span = b.az - a.az; if (span <= 0) span += 360;
            float rel = az - a.az;    if (rel < 0)   rel += 360;
            if (rel <= span)
            {
                const float t = rel / span;
                gains[a.channel] += scale * std::cos (t * kPi * 0.5f);
                gains[b.channel] += scale * std::sin (t * kPi * 0.5f);
                return;
            }
        }
    }

    // fills gains[numChannels]; spread 0..1 blurs the source over the ring (wider/farther = more)
    void compute (float azDeg, float elDeg, float spread, float* gains) const
    {
        for (int i = 0; i < numChannels; ++i) gains[i] = 0;
        const float h = top.empty() ? 0.0f : sat01 (elDeg / 60.0f);
        pairPan (ear, azDeg, std::cos (h * kPi * 0.5f), gains);
        if (! top.empty()) pairPan (top, azDeg, std::sin (h * kPi * 0.5f), gains);
        if (spread > 0.001f)
        {
            const float per = spread / (float) juce::jmax<size_t> (1, ear.size() + top.size());
            for (auto& s : ear) gains[s.channel] += per;
            for (auto& s : top) gains[s.channel] += per * 0.7f;
        }
        float p = 0; for (int i = 0; i < numChannels; ++i) p += gains[i] * gains[i];
        const float nrm = p > 1e-9f ? 1.0f / std::sqrt (p) : 0.0f;
        for (int i = 0; i < numChannels; ++i) gains[i] *= nrm;
    }
};
} // namespace cst
