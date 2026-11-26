/*
 * Tinplate Reverb - writes plugin/Rt60Table.hpp: the decay time of the plate measured on its
 * impulse response (tools/rt60_measure.hpp) for a grid of Size x Tail x Damping x frequency,
 * at 44.1 kHz (one thread per size).  The interface interpolates it (plugin/Rt60.hpp); tests/test_dsp.cpp checks the
 * interpolated values against new measurements between the points of the grid.
 *
 *   g++ -O2 -std=gnu++17 -Iplugin tools/rt60_table.cpp -o build/rt60_table -lpthread
 *   ./build/rt60_table > plugin/Rt60Table.hpp
 *
 * The bandwidth is at its maximum (it filters the input, not the decay).  Delays scale with the
 * sample rate but the filter coefficients do not: at another rate fs, the decay at f is the one
 * measured here at f x 44100 / fs.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "PlateCore.hpp"
#include "TinplateParams.hpp"
#include "rt60_measure.hpp"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

// size 0.5 to 2 in steps of 0.5; tail from 0 to 1.2 (beyond the maximum of the parameter, 1.16)
// in steps of 0.05
static const int kSizes = 4, kTails = 25, kDampings = 9, kFreqs = 9;
static const double kFirstSize = 0.5, kSizeStep = 0.5, kTailStep = 0.05;
static const double kFreq[kFreqs] = { 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

// the block of one size, as text
static void measureSize(const int z, std::string* const out)
{
    const double sr = 44100.0;
    const size_t n = static_cast<size_t>(45.0 * sr);   // RT60 up to 30 s (size 2, tail 1.2, 125 Hz)
    const float size = static_cast<float>(kFirstSize + z * kSizeStep);
    std::vector<float> l(n), r(n);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "  {   // size %.1f\n", size);
    *out = buf;
    for (int t = 0; t < kTails; ++t)
    {
        std::snprintf(buf, sizeof(buf), "    {   // tail %.2f\n", t * kTailStep);
        *out += buf;
        for (int d = 0; d < kDampings; ++d)
        {
            const float tail = static_cast<float>(t * kTailStep);
            const float damping = static_cast<float>(d) / (kDampings - 1);
            PlateCore::Plate p;
            p.init(static_cast<float>(sr));
            p.setSize(size, 0);
            p.activate();
            p.setBandwidth(static_cast<float>(TinplateLaw::bandwidthCoef(1.0f)));
            p.setDamping(static_cast<float>(TinplateLaw::dampingCoef(damping)));
            const float decay = TinplateLaw::decay(tail);
            for (size_t i = 0; i < n; ++i)
                p.process(i == 0 ? 1.0f : 0.0f, decay, &l[i], &r[i]);
            *out += "        {";
            for (int f = 0; f < kFreqs; ++f)
            {
                std::snprintf(buf, sizeof(buf), "%s%.3ff", f ? ", " : " ", measureRt60(l, r, kFreq[f], sr));
                *out += buf;
            }
            *out += " },\n";
        }
        *out += "    },\n";
    }
    *out += "  },\n";
}

int main()
{
    std::printf("/*\n * Tinplate Reverb - decay time (s) measured on the impulse response at 44.1 kHz,\n"
                " * written by tools/rt60_table.cpp: do not edit.  [size][tail][damping][frequency];\n"
                " * size from %.1f in steps of %.1f, tail and damping in steps of %.2f and 1/%d,\n"
                " * frequencies in octaves from 62.5 Hz.\n"
                " *\n * Copyright (C) 2026 Tinplate authors\n * SPDX-License-Identifier: GPL-3.0-or-later\n */\n",
                kFirstSize, kSizeStep, kTailStep, kDampings - 1);
    std::printf("#ifndef RT60_TABLE_HPP_INCLUDED\n#define RT60_TABLE_HPP_INCLUDED\n\n");
    std::printf("namespace Rt60Table {\n\nstatic const int kSizes = %d, kTails = %d, kDampings = %d, kFreqs = %d;\n"
                "static const double kFirstSize = %.1f, kSizeStep = %.1f, kTailStep = %.2f, kFirstHz = 62.5;\n\n"
                "static const float kSeconds[kSizes][kTails][kDampings][kFreqs] = {\n",
                kSizes, kTails, kDampings, kFreqs, kFirstSize, kSizeStep, kTailStep);
    std::string blocks[kSizes];
    std::vector<std::thread> threads;
    for (int z = 0; z < kSizes; ++z)
        threads.emplace_back(measureSize, z, &blocks[z]);
    for (std::thread& t : threads)
        t.join();
    for (const std::string& b : blocks)
        std::fputs(b.c_str(), stdout);
    std::printf("};\n\n}   // namespace Rt60Table\n\n#endif\n");
    return 0;
}
