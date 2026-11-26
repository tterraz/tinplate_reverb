/*
 * Tinplate Reverb - writes plugin/Rt60Table.hpp: the decay time of the plate measured on its
 * impulse response (tools/rt60_measure.hpp) for a grid of Tail x Damping x frequency, at
 * 44.1 kHz.  The interface interpolates it (plugin/Rt60.hpp); tests/test_dsp.cpp checks the
 * interpolated values against new measurements between the points of the grid.
 *
 *   g++ -O2 -std=gnu++17 -Iplugin tools/rt60_table.cpp -o build/rt60_table
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
#include <vector>

// tail from 0 to 1.2 (beyond the maximum of the parameter, 1.16) in steps of 0.05
static const int kTails = 25, kDampings = 9, kFreqs = 9;
static const double kTailStep = 0.05;
static const double kFreq[kFreqs] = { 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

int main()
{
    const double sr = 44100.0;
    const size_t n = static_cast<size_t>(30.0 * sr);   // RT60 up to 15 s (tail 1.2, 125 Hz)
    std::printf("/*\n * Tinplate Reverb - decay time (s) measured on the impulse response at 44.1 kHz,\n"
                " * written by tools/rt60_table.cpp: do not edit.  [tail][damping][frequency];\n"
                " * tail and damping in steps of %.2f and 1/%d, frequencies in octaves from 62.5 Hz.\n"
                " *\n * Copyright (C) 2026 Tinplate authors\n * SPDX-License-Identifier: GPL-3.0-or-later\n */\n",
                kTailStep, kDampings - 1);
    std::printf("#ifndef RT60_TABLE_HPP_INCLUDED\n#define RT60_TABLE_HPP_INCLUDED\n\n");
    std::printf("namespace Rt60Table {\n\nstatic const int kTails = %d, kDampings = %d, kFreqs = %d;\n"
                "static const double kTailStep = %.2f, kFirstHz = 62.5;\n\n"
                "static const float kSeconds[kTails][kDampings][kFreqs] = {\n",
                kTails, kDampings, kFreqs, kTailStep);
    std::vector<float> l(n), r(n);
    for (int t = 0; t < kTails; ++t)
    {
        std::printf("    {   // tail %.2f\n", t * kTailStep);
        for (int d = 0; d < kDampings; ++d)
        {
            const float tail = static_cast<float>(t * kTailStep);
            const float damping = static_cast<float>(d) / (kDampings - 1);
            PlateCore::Plate p;
            p.init(static_cast<float>(sr));
            p.activate();
            p.setBandwidth(static_cast<float>(TinplateLaw::bandwidthCoef(1.0f)));
            p.setDamping(static_cast<float>(TinplateLaw::dampingCoef(damping)));
            const float decay = TinplateLaw::decay(tail);
            for (size_t i = 0; i < n; ++i)
                p.process(i == 0 ? 1.0f : 0.0f, decay, &l[i], &r[i]);
            std::printf("        {");
            for (int f = 0; f < kFreqs; ++f)
                std::printf("%s%.3ff", f ? ", " : " ", measureRt60(l, r, kFreq[f], sr));
            std::printf(" },\n");
        }
        std::printf("    },\n");
    }
    std::printf("};\n\n}   // namespace Rt60Table\n\n#endif\n");
    return 0;
}
