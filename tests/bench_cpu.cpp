/*
 * Tinplate Reverb - CPU bench of the DSP (run()), native build:
 *   g++ -O3 -ffast-math -mno-daz-ftz -std=gnu++11 -Iplugin -Idpf/distrho -Idpf/dgl \
 *       -DDISTRHO_PLUGIN_TARGET_EXPORT -DDISTRHO_IS_STANDALONE=0 tests/bench_cpu.cpp -o build/bench_cpu
 *   taskset -c 2 build/bench_cpu
 * Same optimisation options as the plugin (DPF: -O3 -ffast-math), but -mno-daz-ftz: an
 * executable built with -ffast-math would otherwise switch FTZ / DAZ on at start-up (run()
 * switches them on itself, as in a host).
 *
 * Stereo, blocks of 512, preset Default, 10 s per case, at 44.1, 96 and 192 kHz:
 *   music    noise bursts with a sine;
 *   silence  10 s of silence after 1 s of music (only the silence is timed): denormals;
 *   clear    Clear on, attacks every 0.5 s after silences, fade 200 ms: two plates 40 % of
 *            the time;
 *   size     music, Size moved between 100 and 150 % every 0.25 s: the lengths glide (read
 *            with interpolation) all the time, the worst case;
 *   ducking  music, Duck 12 dB (detector and gain at every sample);
 *   options  music, stereo input on (two sets of input filters) and mono bass 150 Hz;
 *   two      music, Split 50 %: both plates in parallel.
 * Best of 3 runs.  Result: ns per stereo frame and load of one instance in % of one core in
 * real time.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "src/DistrhoPluginInternal.hpp"
#include "src/DistrhoPlugin.cpp"
#include "src/DistrhoUtils.cpp"
#include "../plugin/TinplatePlugin.cpp"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

USE_NAMESPACE_DISTRHO

static const uint32_t kBlock = 512;
static const double kSeconds = 10.0;

enum { kMusic = 0, kSilence, kClear, kSize, kDucking, kOptions, kTwo, kNumCases };
static const char* const kCaseNames[kNumCases] = { "music", "silence", "clear", "size", "ducking", "options", "two" };

static void fillMusic(std::vector<float>& L, std::vector<float>& R, const size_t n0, const size_t n1)
{
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (size_t n = n0; n < n1; ++n)
    {
        const float burst = (n / 2000) % 2 ? 0.5f : 0.05f;
        L[n] = burst * (0.6f * std::sin(0.031f * n) + 0.4f * u(rng));
        R[n] = burst * (0.5f * std::sin(0.017f * n + 1.0f) + 0.5f * u(rng));
    }
}

// seconds spent in run() on the timed frames
static double timeRun(const int c, const double rate)
{
    d_nextSampleRate = rate;
    const size_t warm = c == kSilence ? static_cast<size_t>(rate) : 0;
    const size_t n = warm + static_cast<size_t>(kSeconds * rate);
    std::vector<float> L(n, 0.0f), R(n, 0.0f), oL(kBlock), oR(kBlock);
    fillMusic(L, R, 0, c == kSilence ? warm : n);
    if (c == kClear)
    {
        // 0.3 s of music, 0.2 s of silence
        const size_t period = static_cast<size_t>(0.5 * rate), on = static_cast<size_t>(0.3 * rate);
        for (size_t i = 0; i < n; ++i)
            if (i % period >= on)
                L[i] = R[i] = 0.0f;
    }

    PluginExporter p(nullptr, nullptr, nullptr, nullptr);
    for (int i = 0; i < kPresetParamCount; ++i)
        p.setParameterValue(i, kTinplatePresets[0].values[i]);
    if (c == kClear)
    {
        p.setParameterValue(kParamClear, 1.0f);
        p.setParameterValue(kParamFade, 200.0f);
    }
    if (c == kDucking)
        p.setParameterValue(kParamDuck, 12.0f);
    if (c == kOptions)
    {
        p.setParameterValue(kParamStereoIn, 1.0f);
        p.setParameterValue(kParamMonoBass, 150.0f);
    }
    if (c == kTwo)
        p.setParameterValue(kParamSplit, 50.0f);   // both plates
    p.activate();

    const size_t sizeStep = static_cast<size_t>(0.25 * rate);
    double seconds = 0.0;
    for (size_t k = 0; k < n; k += kBlock)
    {
        if (c == kSize && k / sizeStep != (k + kBlock) / sizeStep)
            p.setParameterValue(kParamSize, ((k + kBlock) / sizeStep) % 2 ? 150.0f : 100.0f);
        const uint32_t f = static_cast<uint32_t>(std::min<size_t>(kBlock, n - k));
        const float* in[2] = { &L[k], &R[k] };
        float* out[2] = { oL.data(), oR.data() };
        const auto t0 = std::chrono::steady_clock::now();
        p.run(in, out, f);
        const auto t1 = std::chrono::steady_clock::now();
        if (k >= warm)
            seconds += std::chrono::duration<double>(t1 - t0).count();
    }
    return seconds;
}

int main()
{
    d_nextBufferSize = kBlock;
    std::printf("Tinplate CPU bench: stereo, blocks of %u, preset Default, %.0f s per case, best of 3\n", kBlock,
                kSeconds);
    std::printf("%-8s %-8s  %9s  %8s\n", "rate", "case", "ns/frame", "% core");
    for (const double rate : { 44100.0, 96000.0, 192000.0 })
        for (int c = 0; c < kNumCases; ++c)
        {
            double best = 1e30;
            for (int run = 0; run < 3; ++run)
                best = std::min(best, timeRun(c, rate));
            std::printf("%-8.1f %-8s  %9.1f  %8.2f\n", rate / 1000.0, kCaseNames[c], best / (kSeconds * rate) * 1e9,
                        best / kSeconds * 100.0);
        }
    return 0;
}
