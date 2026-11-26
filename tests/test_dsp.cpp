/*
 * Tinplate Reverb - DSP tests (native build, see ../build.sh; built twice: strict options, and
 * the options DPF gives the plugin, -O3 -ffast-math).
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * 1. Original: with the additions neutral, output == the original CAPS PlateX2 (tests/caps/,
 *    driven through LADSPA), for 7 settings of the four controls at 44.1, 48, 96 and 192 kHz,
 *    odd block sizes.  Strict build: identical to the bit; -ffast-math build: both codes are
 *    optimised differently, the gap is reported and must stay at rounding level.
 * 2. Bypass: output == input, exactly.
 * 3. Pre-delay: the wet response starts exactly that many samples later (not identical
 *    afterwards: the modulation of the tank runs from the start).
 * 4. Low cut: a 50 Hz tone in the wet signal loses >= 30 dB with the low cut at 500 Hz.
 * 5. Width: 0 % gives L == R (wet only); 200 % doubles the side signal.
 * 6. Freeze: the tail keeps its level for 10 s, ignores the input, then decays again.
 * 7. Decay time: RT60 measured on the impulse response (tools/rt60_measure.hpp) between the
 *    points of the measured table == the value shown by the interface (plugin/Rt60.hpp),
 *    within 10 %, at 44.1 and 96 kHz, also with Tail beyond 1 (up to 1.16: 10 s at 500 Hz).
 * 8. Smoothing: Blend jumping from 0 to 1 moves the output smoothly (the original jumps);
 *    a change of pre-delay gives no click.
 * 9. Presets: finite output, sensible peak.
 * 10. Meters; state of the interface kept; FPU state of the caller restored after run().
 * 11. Ducking: steady cut (depth, threshold), attack time, meter of the cut; once the cut is
 *     back to 0, output identical to the bit to a run without ducking.
 */
#include "src/DistrhoPluginInternal.hpp"
#include "src/DistrhoPlugin.cpp"
#include "src/DistrhoUtils.cpp"
#include "../plugin/TinplatePlugin.cpp"
#include "caps_reference.hpp"
#include "../plugin/Rt60.hpp"
#include "../tools/rt60_measure.hpp"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

USE_NAMESPACE_DISTRHO

static int gFailures = 0;

static void check(const bool ok, const char* what)
{
    gFailures += !ok;
    std::printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
}

struct Settings {
    float values[kParamCount];   // by parameter index; those of kPresetParams are used
    float freeze = 0.0f;
    float bypass = 0.0f;
};

static Settings originalSettings(const float bw, const float tail, const float damp, const float blend)
{
    Settings s;
    for (int i = 0; i < kPresetParamCount; ++i)
        s.values[kPresetParams[i]] = kTinplatePresets[1].values[i];   // "Original (CAPS)"
    s.values[kParamBandwidth] = bw;
    s.values[kParamTail] = tail;
    s.values[kParamDamping] = damp;
    s.values[kParamBlend] = blend;
    return s;
}

static const uint32_t kSizes[] = { 1, 7, 64, 333, 512 };

// a change of the settings at a given sample (applied at the start of the block holding it)
struct Change {
    size_t at;
    int param;
    float value;
};

// Runs the plugin with odd block sizes (blocks split at the changes).
static void process(const Settings& s, const double sr, const std::vector<float>& L, const std::vector<float>& R,
                    std::vector<float>& oL, std::vector<float>& oR, const std::vector<Change>& changes = {},
                    float* outs = nullptr)
{
    d_nextSampleRate = sr;
    PluginExporter p(nullptr, nullptr, nullptr, nullptr);
    for (int i = 0; i < kPresetParamCount; ++i)
        p.setParameterValue(kPresetParams[i], s.values[kPresetParams[i]]);
    p.setParameterValue(kParamFreeze, s.freeze);
    p.setParameterValue(kParamBypass, s.bypass);
    p.activate();

    const size_t N = L.size();
    oL.assign(N, 0.0f);
    oR.assign(N, 0.0f);
    size_t n = 0, k = 0, c = 0;
    while (n < N)
    {
        while (c < changes.size() && changes[c].at <= n)
        {
            p.setParameterValue(changes[c].param, changes[c].value);
            ++c;
        }
        size_t f = std::min<size_t>(kSizes[k++ % 5], N - n);
        if (c < changes.size())
            f = std::min(f, changes[c].at - n);
        const float* in[2] = { &L[n], &R[n] };
        float* out[2] = { &oL[n], &oR[n] };
        p.run(in, out, static_cast<uint32_t>(f));
        n += f;
    }
    if (outs)
        for (int i = kParamFirstOutput; i < kParamCount; ++i)
            outs[i] = p.getParameterValue(i);
}

// The original, same blocks
static void processCaps(const float controls[4], const unsigned long sr, const std::vector<float>& L,
                        const std::vector<float>& R, std::vector<float>& oL, std::vector<float>& oR)
{
    CapsPlate* const p = capsPlateNew(sr);
    const size_t N = L.size();
    oL.assign(N, 0.0f);
    oR.assign(N, 0.0f);
    size_t n = 0, k = 0;
    while (n < N)
    {
        const size_t f = std::min<size_t>(kSizes[k++ % 5], N - n);
        capsPlateRun(p, &L[n], &R[n], &oL[n], &oR[n], f, controls);
        n += f;
    }
    capsPlateDelete(p);
}

static float peakOf(const std::vector<float>& x, const size_t a = 0, size_t b = 0)
{
    if (b == 0)
        b = x.size();
    float m = 0.0f;
    for (size_t i = a; i < b; ++i)
        m = std::max(m, std::fabs(x[i]));
    return m;
}

static double rms(const std::vector<float>& x, const size_t a, const size_t b)
{
    double e = 0.0;
    for (size_t i = a; i < b; ++i)
        e += static_cast<double>(x[i]) * x[i];
    return std::sqrt(e / static_cast<double>(b - a));
}

static double db(const double g) { return 20.0 * std::log10(std::max(g, 1e-30)); }

// test signal: noise bursts, tones, silences (the tail decays into the silences)
static void testSignal(const double sr, const double seconds, std::vector<float>& L, std::vector<float>& R)
{
    const size_t N = static_cast<size_t>(seconds * sr);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    L.assign(N, 0.0f);
    R.assign(N, 0.0f);
    for (size_t n = 0; n < N; ++n)
    {
        const double t = n / sr;
        const int seg = static_cast<int>(t / 0.25) % 4;
        if (seg == 0)
        {
            L[n] = 0.5f * u(rng);
            R[n] = 0.3f * u(rng);
        }
        else if (seg == 1)
        {
            L[n] = 0.4f * static_cast<float>(std::sin(2.0 * M_PI * 440.0 * t));
            R[n] = 0.6f * static_cast<float>(std::sin(2.0 * M_PI * 3100.0 * t + 1.0));
        }
    }
}

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 44100.0;
#ifdef __FAST_MATH__
    std::printf("build: -ffast-math (options of DPF)\n");
#else
    std::printf("build: strict floating point\n");
#endif

    // 1. original -------------------------------------------------------------------------------
    std::printf("1. original CAPS PlateX2 (additions neutral)\n");
    {
        const float sets[][4] = {
            { 0.5f, 0.5f, 0.75f, 0.25f },   // LADSPA defaults
            { 1.0f, 1.0f, 0.0f, 1.0f },     // brightest, longest, wet only
            { 0.0f, 0.0f, 1.0f, 0.5f },
            { 0.8f, 0.55f, 0.4f, 0.3f },
            { 0.33f, 0.91f, 0.12f, 0.77f },
            { 0.9f, 0.2f, 0.6f, 0.0f },     // dry only
            { 0.65f, 0.75f, 0.25f, 0.6f },
        };
        const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        double worst = 0.0;
        int cases = 0;
        for (const double sr : rates)
        {
            std::vector<float> L, R, oL, oR, cL, cR;
            testSignal(sr, sr > 100000.0 ? 1.1 : 2.1, L, R);
            for (const auto& c : sets)
            {
                process(originalSettings(c[0], c[1], c[2], c[3]), sr, L, R, oL, oR);
                processCaps(c, static_cast<unsigned long>(sr), L, R, cL, cR);
                const double scale = std::max(1e-3f, std::max(peakOf(cL), peakOf(cR)));
                double err = 0.0;
                for (size_t i = 0; i < L.size(); ++i)
                    err = std::max(err, std::max<double>(std::fabs(oL[i] - cL[i]), std::fabs(oR[i] - cR[i])) / scale);
                worst = std::max(worst, err);
                ++cases;
            }
        }
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%d cases (7 settings x 4 rates): max error / peak %.2e", cases, worst);
#ifdef __FAST_MATH__
        check(worst < 1e-5, buf);
#else
        check(worst == 0.0, buf);
#endif
    }

    const double sr = 44100.0;
    std::vector<float> L, R, oL, oR, rL, rR;
    testSignal(sr, 2.0, L, R);
    const size_t N = L.size();

    // 2. bypass -----------------------------------------------------------------------------------
    std::printf("2. bypass\n");
    {
        Settings s = originalSettings(0.8f, 0.7f, 0.3f, 0.5f);
        s.values[kParamPredelay] = 40.0f;
        s.values[kParamLowCut] = 300.0f;
        s.values[kParamWidth] = 150.0f;
        s.values[kParamOutput] = -6.0f;
        s.bypass = 1.0f;
        process(s, sr, L, R, oL, oR);
        float err = 0.0f;
        for (size_t i = 0; i < N; ++i)
            err = std::max(err, std::max(std::fabs(oL[i] - L[i]), std::fabs(oR[i] - R[i])));
        char buf[96];
        std::snprintf(buf, sizeof(buf), "max error %.1e", err);
        check(err == 0.0f, buf);
    }

    // impulse, wet only
    std::vector<float> imp(static_cast<size_t>(3.0 * sr), 0.0f);
    imp[100] = 1.0f;

    // 3. pre-delay --------------------------------------------------------------------------------
    std::printf("3. pre-delay\n");
    {
        Settings s = originalSettings(0.8f, 0.6f, 0.4f, 1.0f);
        process(s, sr, imp, imp, rL, rR);
        s.values[kParamPredelay] = 50.0f;
        process(s, sr, imp, imp, oL, oR);
        const size_t d = static_cast<size_t>(std::lround(0.05 * sr));
        const auto onset = [](const std::vector<float>& x) {
            for (size_t i = 0; i < x.size(); ++i)
                if (std::fabs(x[i]) > 1e-9f)
                    return i;
            return x.size();
        };
        char buf[128];
        std::snprintf(buf, sizeof(buf), "50 ms: wet response from sample %zu, %zu without (%zu samples)",
                      onset(oL), onset(rL), d);
        check(onset(oL) == onset(rL) + d && onset(oR) == onset(rR) + d, buf);
    }

    // 4. low cut ----------------------------------------------------------------------------------
    std::printf("4. low cut\n");
    {
        std::vector<float> tone(static_cast<size_t>(2.0 * sr));
        for (size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 50.0 * i / sr));
        Settings s = originalSettings(0.8f, 0.6f, 0.4f, 1.0f);
        process(s, sr, tone, tone, rL, rR);
        s.values[kParamLowCut] = 500.0f;
        process(s, sr, tone, tone, oL, oR);
        const size_t a = tone.size() / 2;
        const double loss = db(rms(oL, a, tone.size()) / rms(rL, a, tone.size()));
        char buf[96];
        std::snprintf(buf, sizeof(buf), "50 Hz with low cut 500 Hz: %.1f dB", loss);
        check(loss < -30.0, buf);
    }

    // 5. width ------------------------------------------------------------------------------------
    std::printf("5. width\n");
    {
        Settings s = originalSettings(0.8f, 0.6f, 0.4f, 1.0f);
        process(s, sr, L, R, rL, rR);
        s.values[kParamWidth] = 0.0f;
        process(s, sr, L, R, oL, oR);
        bool mono = true;
        for (size_t i = 0; i < N; ++i)
            mono = mono && oL[i] == oR[i];
        check(mono, "0 %: L == R");
        s.values[kParamWidth] = 200.0f;
        process(s, sr, L, R, oL, oR);
        double err = 0.0, side = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            const double s1 = (rL[i] - rR[i]) * 0.5, s2 = (oL[i] - oR[i]) * 0.5;
            const double m1 = (rL[i] + rR[i]) * 0.5, m2 = (oL[i] + oR[i]) * 0.5;
            err = std::max(err, std::max(std::fabs(s2 - 2.0 * s1), std::fabs(m2 - m1)));
            side = std::max(side, std::fabs(s1));
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "200 %%: side x 2, mid unchanged (error %.1e / %.2f)", err, side);
        check(err < 1e-5 * side, buf);
    }

    // 6. freeze -----------------------------------------------------------------------------------
    std::printf("6. freeze\n");
    {
        const size_t n = static_cast<size_t>(18.0 * sr), on = static_cast<size_t>(0.5 * sr);
        const size_t off = static_cast<size_t>(11.0 * sr);
        std::vector<float> x(n, 0.0f), y(n, 0.0f);
        std::mt19937 rng(2);
        std::uniform_real_distribution<float> u(-0.5f, 0.5f);
        for (size_t i = 0; i < on; ++i)
            x[i] = y[i] = u(rng);
        for (size_t i = on + static_cast<size_t>(0.1 * sr); i < off; ++i)
            y[i] = u(rng);   // input during the freeze, once the input has faded out
        Settings s = originalSettings(0.8f, 0.5f, 0.4f, 1.0f);
        const std::vector<Change> ch = { { on, kParamFreeze, 1.0f }, { off, kParamFreeze, 0.0f } };
        process(s, sr, x, x, rL, rR, ch);
        process(s, sr, y, y, oL, oR, ch);
        const size_t s1 = on + static_cast<size_t>(0.5 * sr);
        const double first = rms(rL, s1, s1 + static_cast<size_t>(sr));
        const double last = rms(rL, off - static_cast<size_t>(sr), off);
        const double end = rms(rL, n - static_cast<size_t>(sr), n);
        bool same = true;
        for (size_t i = 0; i < off; ++i)
            same = same && oL[i] == rL[i] && oR[i] == rR[i];
        char buf[160];
        std::snprintf(buf, sizeof(buf), "level after 10 s of freeze: %+.2f dB", db(last / first));
        check(std::fabs(db(last / first)) < 1.5, buf);
        check(same, "input ignored while frozen (output identical)");
        std::snprintf(buf, sizeof(buf), "7 s after the freeze: %.1f dB", db(end / first));
        check(db(end / first) < -60.0, buf);
    }

    // 7. decay time ------------------------------------------------------------------------------
    std::printf("7. decay time shown == measured, between the points of the table\n");
    {
        struct { double sr; float tail, damping; double hz; } cases[] = {
            { 44100.0, 0.35f, 0.30f, 700.0 }, { 44100.0, 0.55f, 0.60f, 3000.0 },
            { 44100.0, 0.85f, 0.10f, 300.0 }, { 44100.0, 0.15f, 0.90f, 1500.0 },
            { 44100.0, 0.95f, 0.45f, 6000.0 }, { 96000.0, 0.55f, 0.60f, 3000.0 },
            { 96000.0, 0.75f, 0.20f, 180.0 },
            // beyond the range of the original (tail up to 1.16): 10 s at 500 Hz
            { 44100.0, 1.16f, 0.00f, 500.0 }, { 44100.0, 1.12f, 0.35f, 900.0 },
            { 96000.0, 1.07f, 0.55f, 2500.0 },
        };
        for (const auto& c : cases)
        {
            std::vector<float> longImp(static_cast<size_t>(24.0 * c.sr), 0.0f);
            longImp[0] = 1.0f;
            Settings s = originalSettings(1.0f, c.tail, c.damping, 1.0f);
            process(s, c.sr, longImp, longImp, oL, oR);
            const double measured = measureRt60(oL, oR, c.hz, c.sr);
            const double shown = TinplateRt60::seconds(c.tail, c.damping, c.hz, c.sr);
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%.0f kHz, tail %.2f damping %.2f, %5.0f Hz: measured %.2f s, shown %.2f s",
                          c.sr / 1000.0, c.tail, c.damping, c.hz, measured, shown);
            check(measured > 0.0 && std::fabs(shown / measured - 1.0) < 0.10, buf);
        }
    }

    // 8. smoothing --------------------------------------------------------------------------------
    std::printf("8. smoothing\n");
    {
        std::vector<float> tone(static_cast<size_t>(1.0 * sr));
        for (size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 220.0 * i / sr));
        const size_t at = static_cast<size_t>(0.5 * sr);
        const auto maxStep = [&](const std::vector<float>& y, const size_t a, const size_t b) {
            float m = 0.0f;
            for (size_t i = a; i < b; ++i)
                m = std::max(m, std::fabs(y[i] - y[i - 1]));
            return m;
        };
        // steady 220 Hz at 0.5: steps up to 0.5 x 2 pi 220 / 44100 = 0.016
        Settings s = originalSettings(0.8f, 0.6f, 0.4f, 0.0f);
        process(s, sr, tone, tone, oL, oR, { { at, kParamBlend, 1.0f } });
        const float ours = maxStep(oL, at - 10, at + static_cast<size_t>(0.05 * sr));
        // the original: blend changed between two blocks
        std::vector<float> cL(tone.size()), cR(tone.size());
        CapsPlate* const p = capsPlateNew(44100);
        const float c0[4] = { 0.8f, 0.6f, 0.4f, 0.0f }, c1[4] = { 0.8f, 0.6f, 0.4f, 1.0f };
        capsPlateRun(p, &tone[0], &tone[0], &cL[0], &cR[0], at, c0);
        capsPlateRun(p, &tone[at], &tone[at], &cL[at], &cR[at], tone.size() - at, c1);
        capsPlateDelete(p);
        const float theirs = maxStep(cL, at - 10, at + static_cast<size_t>(0.05 * sr));
        char buf[128];
        std::snprintf(buf, sizeof(buf), "blend 0 -> 1: largest step %.3f (original: %.3f)", ours, theirs);
        check(ours < 0.03f && theirs > 0.2f, buf);

        s = originalSettings(0.8f, 0.6f, 0.4f, 1.0f);
        process(s, sr, tone, tone, rL, rR);
        process(s, sr, tone, tone, oL, oR, { { at, kParamPredelay, 120.0f } });
        const float steady = maxStep(rL, at, at + static_cast<size_t>(0.2 * sr));
        const float moved = maxStep(oL, at, at + static_cast<size_t>(0.2 * sr));
        std::snprintf(buf, sizeof(buf), "pre-delay 0 -> 120 ms: largest step %.4f (steady %.4f)", moved, steady);
        check(moved < 1.5f * steady, buf);
    }

    // 9. presets ----------------------------------------------------------------------------------
    std::printf("9. presets\n");
    for (int k = 0; k < kTinplatePresetCount; ++k)
    {
        Settings s;
        for (int i = 0; i < kPresetParamCount; ++i)
            s.values[kPresetParams[i]] = kTinplatePresets[k].values[i];
        process(s, sr, L, R, oL, oR);
        bool finite = true;
        for (size_t i = 0; i < N; ++i)
            finite = finite && std::isfinite(oL[i]) && std::isfinite(oR[i]);
        const float peak = std::max(peakOf(oL), peakOf(oR));
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%-18s peak %.2f", kTinplatePresets[k].name, peak);
        check(finite && peak > 0.05f && peak < 2.0f, buf);
    }

    // 10. meters, state, FPU ----------------------------------------------------------------------
    std::printf("10. meters, state, FPU\n");
    {
        std::vector<float> tone(static_cast<size_t>(0.5 * sr));
        for (size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * i / sr));
        float outs[kParamCount];
        Settings s = originalSettings(0.8f, 0.6f, 0.4f, 0.0f);
        process(s, sr, tone, tone, oL, oR, {}, outs);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "1 kHz at -6 dBFS, dry: in %.2f, out %.2f dB, reverb %.1f dB",
                      outs[kParamMeterIn], outs[kParamMeterOut], outs[kParamMeterWet]);
        check(std::fabs(outs[kParamMeterIn] + 6.02f) < 0.05f && std::fabs(outs[kParamMeterOut] + 6.02f) < 0.05f
              && outs[kParamMeterWet] > -40.0f, buf);

        d_nextSampleRate = sr;
        PluginExporter p(nullptr, nullptr, nullptr, nullptr);
        p.setState("ab", "slot=1;other=0.5");
        check(std::strcmp(p.getStateValue("ab"), "slot=1;other=0.5") == 0 && p.getStateCount() == 1,
              "A/B state kept and given back");
#if defined(__SSE__) || defined(_M_X64)
        const unsigned before = _mm_getcsr();
        p.activate();
        const float* in[2] = { &tone[0], &tone[0] };
        float* out[2] = { &oL[0], &oR[0] };
        p.run(in, out, 256);
        check(_mm_getcsr() == before, "FPU state of the caller restored (FTZ / DAZ only inside run)");
#endif
    }

    // 11. ducking -------------------------------------------------------------------------------
    std::printf("11. ducking\n");
    {
        // 1 kHz at -6 dBFS for 1 s, then silence; wet only
        const size_t on = static_cast<size_t>(1.0 * sr), n = static_cast<size_t>(5.0 * sr);
        std::vector<float> tone(n, 0.0f);
        for (size_t i = 0; i < on; ++i)
            tone[i] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * i / sr));
        Settings s = originalSettings(0.8f, 0.6f, 0.4f, 1.0f);
        process(s, sr, tone, tone, rL, rR);
        const size_t a = static_cast<size_t>(0.5 * sr);
        char buf[160];

        // threshold -30: 24 dB over it, cut = the whole depth
        s.values[kParamDuck] = 12.0f;
        s.values[kParamThreshold] = -30.0f;
        float outs[kParamCount];
        std::vector<float> half(tone.begin(), tone.begin() + static_cast<long>(on));
        std::vector<float> hL, hR;
        process(s, sr, half, half, hL, hR, {}, outs);
        process(s, sr, tone, tone, oL, oR);
        double cut = db(rms(oL, a, on) / rms(rL, a, on));
        std::snprintf(buf, sizeof(buf), "depth 12 dB, input 24 dB over the threshold: reverb %.2f dB, meter %.2f dB",
                      cut, outs[kParamMeterDuck]);
        check(std::fabs(cut + 12.0) < 0.3 && std::fabs(outs[kParamMeterDuck] - 12.0f) < 0.3f, buf);

        // once the cut is back to 0, the same output as without ducking (the plate is not touched)
        size_t back = n;
        for (size_t i = n; i-- > on;)
            if (oL[i] != rL[i] || oR[i] != rR[i])
            {
                back = i + 1;
                break;
            }
        std::snprintf(buf, sizeof(buf), "identical to no ducking %.2f s after the input stops (release 300 ms)",
                      (back - on) / sr);
        check(back < on + static_cast<size_t>(3.2 * sr), buf);   // 12 dB -> 0.001 dB: 9.4 x 300 ms

        // threshold -12: the input peak goes 6 dB over it, cut 6 dB (depth 30 not reached)
        s.values[kParamDuck] = 30.0f;
        s.values[kParamThreshold] = -12.0f;
        process(s, sr, tone, tone, oL, oR);
        cut = db(rms(oL, a, on) / rms(rL, a, on));
        std::snprintf(buf, sizeof(buf), "input 6 dB over the threshold, depth 30 dB: reverb %.2f dB", cut);
        check(cut < -5.0 && cut > -6.3, buf);

        // attack 10 ms: 63 % of the cut after 10 ms
        s.values[kParamDuck] = 12.0f;
        s.values[kParamThreshold] = -30.0f;
        std::vector<float> start(tone.begin(), tone.begin() + static_cast<long>(0.01 * sr));
        process(s, sr, start, start, hL, hR, {}, outs);
        std::snprintf(buf, sizeof(buf), "attack 10 ms: cut %.2f dB after 10 ms (12 x 0.63 = 7.6)", outs[kParamMeterDuck]);
        check(outs[kParamMeterDuck] > 6.8f && outs[kParamMeterDuck] < 8.2f, buf);

        // depth 0: no cut whatever the other settings
        s.values[kParamDuck] = 0.0f;
        s.values[kParamThreshold] = -60.0f;
        s.values[kParamAttack] = 1.0f;
        process(s, sr, tone, tone, oL, oR, {}, outs);
        bool same = outs[kParamMeterDuck] == 0.0f;
        for (size_t i = 0; i < n; ++i)
            same = same && oL[i] == rL[i] && oR[i] == rR[i];
        check(same, "depth 0: output identical to no ducking");
    }

    std::printf("%s\n", gFailures == 0 ? "all DSP tests passed" : "DSP TESTS FAILED");
    return gFailures == 0 ? 0 : 1;
}
