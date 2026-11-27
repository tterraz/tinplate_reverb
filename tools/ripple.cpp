/*
 * Tinplate Reverb - periodic ripple of the tail of the plate (tools/ripple_measure.hpp), its
 * echo density and its decay time, for a setting of the plate and of Smooth.
 *
 *   g++ -O2 -std=gnu++17 -Iplugin tools/ripple.cpp -o build/ripple
 *   ./build/ripple SR BANDWIDTH TAIL DAMPING SIZE SMOOTH [DEPTH]
 *
 * Unit impulse into the tank (mono input), 20 s, steady controls; SMOOTH 0..1 with the tail
 * compensated as in the plugin (plugin/Smooth.hpp).  Prints:
 *   - periodic ripple (and whole ripple, frequency of the beat) from -10 to -40 dB and from -40
 *     to -80 dB;
 *   - normalized echo density of Abel and Huang (2006) on L, 20 ms windows, mean over 50 .. 150 ms
 *     and 0.2 .. 0.5 s (1: as dense as Gaussian noise);
 *   - RT60 at 500 Hz (tools/rt60_measure.hpp).
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "PlateCore.hpp"
#include "Smooth.hpp"
#include "TinplateParams.hpp"
#include "ripple_measure.hpp"
#include "rt60_measure.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

// normalized echo density (Abel and Huang), mean over [t0, t1] s
static double echoDensity(const std::vector<float>& x, const double sr, const double t0, const double t1)
{
    const size_t w = static_cast<size_t>(0.02 * sr);
    double sum = 0.0;
    int count = 0;
    for (size_t c = static_cast<size_t>(t0 * sr); c < static_cast<size_t>(t1 * sr); c += w / 2)
    {
        double e = 0.0;
        for (size_t i = c - w / 2; i < c + w / 2; ++i)
            e += static_cast<double>(x[i]) * x[i];
        const double sd = std::sqrt(e / w);
        size_t out = 0;
        for (size_t i = c - w / 2; i < c + w / 2; ++i)
            out += std::fabs(x[i]) > sd;
        sum += (static_cast<double>(out) / w) / 0.3173;   // erfc(1/sqrt(2))
        ++count;
    }
    return sum / count;
}

int main(int argc, char** argv)
{
    if (argc != 7 && argc != 8)
    {
        std::fprintf(stderr, "usage: ripple SR BANDWIDTH TAIL DAMPING SIZE SMOOTH [DEPTH]\n");
        return 1;
    }
    const double sr = std::atof(argv[1]);
    const float bandwidth = std::atof(argv[2]), tail = std::atof(argv[3]), damping = std::atof(argv[4]);
    const float size = std::atof(argv[5]), smooth = std::atof(argv[6]);
    const float depth = argc == 8 ? std::atof(argv[7]) : 1.0f;
    const size_t n = static_cast<size_t>(20.0 * sr);

    PlateCore::Plate plate;
    plate.init(static_cast<float>(sr));
    plate.setSize(size, 0);
    plate.setSmooth(smooth);
    plate.activate();
    plate.setBandwidth(static_cast<float>(TinplateLaw::bandwidthCoef(bandwidth)));
    plate.setDamping(static_cast<float>(TinplateLaw::dampingCoef(damping)));
    plate.setModulation(depth);
    const float decay = TinplateLaw::decay(TinplateSmooth::tail(tail, damping, size, smooth));

    std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i)
        plate.process(i == 0 ? 1.0f : 0.0f, decay, &l[i], &r[i]);

    double rms1, f1, rms2, f2;
    const double p1 = periodicRipple(l, r, sr, -10.0, -40.0, &rms1, &f1);
    const double p2 = periodicRipple(l, r, sr, -40.0, -80.0, &rms2, &f2);
    std::printf("periodic ripple -10..-40 dB %.2f dB (all %.2f, %2.0f Hz), -40..-80 dB %.2f dB (all %.2f, %2.0f Hz)"
                " | echo density %.2f %.2f | RT60 500 Hz %.2f s\n",
                p1, rms1, f1, p2, rms2, f2, echoDensity(l, sr, 0.05, 0.15), echoDensity(l, sr, 0.2, 0.5),
                measureRt60(l, r, 500.0, sr));
    return 0;
}
