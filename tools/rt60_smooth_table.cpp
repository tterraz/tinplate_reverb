/*
 * Tinplate Reverb - writes plugin/Rt60SmoothTable.hpp: the decay time of the plate at 500 Hz
 * measured on its impulse response (tools/rt60_measure.hpp) for a grid of Smooth x Size x Tail x
 * Damping, at 44.1 kHz (threads by Smooth and Size, at most 7).  The DSP inverts it in Tail
 * (plugin/Smooth.hpp): with Smooth, the tail it uses gives the decay time of Smooth 0.
 *
 *   g++ -O2 -std=gnu++17 -Iplugin tools/rt60_smooth_table.cpp -o build/rt60_smooth_table -lpthread
 *   taskset -c 0-6 ./build/rt60_smooth_table > plugin/Rt60SmoothTable.hpp
 *
 * Tail goes beyond the maximum of the parameter (1.16), up to 1.3: with Smooth, the same decay
 * time needs a larger tail.  Bandwidth at its maximum, as tools/rt60_table.cpp.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "PlateCore.hpp"
#include "TinplateParams.hpp"
#include "rt60_measure.hpp"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

static const int kSmooths = 5, kSizes = 4, kTails = 27, kDampings = 9;
static const double kSmoothStep = 0.25, kFirstSize = 0.5, kSizeStep = 0.5, kTailStep = 0.05;

static float gSeconds[kSmooths][kSizes][kTails][kDampings];

static void measure(const int s, const int z)
{
    const double sr = 44100.0;
    const size_t n = static_cast<size_t>(45.0 * sr);   // RT60 up to 30 s
    std::vector<float> l(n), r(n);
    for (int t = 0; t < kTails; ++t)
        for (int d = 0; d < kDampings; ++d)
        {
            PlateCore::Plate p;
            p.init(static_cast<float>(sr));
            p.setSize(static_cast<float>(kFirstSize + z * kSizeStep), 0);
            p.setSmooth(static_cast<float>(s * kSmoothStep));
            p.activate();
            p.setBandwidth(static_cast<float>(TinplateLaw::bandwidthCoef(1.0f)));
            p.setDamping(static_cast<float>(TinplateLaw::dampingCoef(static_cast<float>(d) / (kDampings - 1))));
            const float decay = TinplateLaw::decay(static_cast<float>(t * kTailStep));
            for (size_t i = 0; i < n; ++i)
                p.process(i == 0 ? 1.0f : 0.0f, decay, &l[i], &r[i]);
            gSeconds[s][z][t][d] = static_cast<float>(measureRt60(l, r, 500.0, sr));
        }
}

int main()
{
    std::atomic<int> next(0);
    std::vector<std::thread> threads;
    for (int k = 0; k < 7; ++k)
        threads.emplace_back([&next]() {
            for (int j; (j = next++) < kSmooths * kSizes;)
                measure(j / kSizes, j % kSizes);
        });
    for (std::thread& t : threads)
        t.join();

    std::printf("/*\n * Tinplate Reverb - decay time (s) at 500 Hz measured on the impulse response at 44.1 kHz,\n"
                " * written by tools/rt60_smooth_table.cpp: do not edit.  [smooth][size][tail][damping];\n"
                " * smooth from 0 in steps of %.2f, size from %.1f in steps of %.1f, tail and damping in steps\n"
                " * of %.2f and 1/%d.\n"
                " *\n * Copyright (C) 2026 Tinplate authors\n * SPDX-License-Identifier: GPL-3.0-or-later\n */\n",
                kSmoothStep, kFirstSize, kSizeStep, kTailStep, kDampings - 1);
    std::printf("#ifndef RT60_SMOOTH_TABLE_HPP_INCLUDED\n#define RT60_SMOOTH_TABLE_HPP_INCLUDED\n\n");
    std::printf("namespace Rt60SmoothTable {\n\nstatic const int kSmooths = %d, kSizes = %d, kTails = %d, kDampings = %d;\n"
                "static const double kSmoothStep = %.2f, kFirstSize = %.1f, kSizeStep = %.1f, kTailStep = %.2f;\n\n"
                "static const float kSeconds[kSmooths][kSizes][kTails][kDampings] = {\n",
                kSmooths, kSizes, kTails, kDampings, kSmoothStep, kFirstSize, kSizeStep, kTailStep);
    for (int s = 0; s < kSmooths; ++s)
    {
        std::printf("  {   // smooth %.2f\n", s * kSmoothStep);
        for (int z = 0; z < kSizes; ++z)
        {
            std::printf("    {   // size %.1f\n", kFirstSize + z * kSizeStep);
            for (int t = 0; t < kTails; ++t)
            {
                std::printf("      {");
                for (int d = 0; d < kDampings; ++d)
                    std::printf("%s%.3ff", d ? ", " : " ", gSeconds[s][z][t][d]);
                std::printf(" },   // tail %.2f\n", t * kTailStep);
            }
            std::printf("    },\n");
        }
        std::printf("  },\n");
    }
    std::printf("};\n\n}   // namespace Rt60SmoothTable\n\n#endif\n");
    return 0;
}
