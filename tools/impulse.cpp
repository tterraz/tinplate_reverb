/*
 * Tinplate Reverb - impulse response of the plate alone (wet, before low cut and width), for the
 * figures of the manual (doc/figures.py builds and calls it).
 *
 *   g++ -O2 -std=gnu++17 -Iplugin tools/impulse.cpp -o build/impulse
 *   ./build/impulse SR SECONDS BANDWIDTH TAIL DAMPING > ir.f32
 *
 * Output: interleaved L, R as raw 32-bit floats; unit impulse into the tank, steady controls
 * (the coefficients of the original, TinplateParams.hpp), no "normal" constant.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "PlateCore.hpp"
#include "TinplateParams.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 6)
    {
        std::fprintf(stderr, "usage: impulse SR SECONDS BANDWIDTH TAIL DAMPING\n");
        return 1;
    }
    const double sr = std::atof(argv[1]);
    const size_t n = static_cast<size_t>(std::atof(argv[2]) * sr);
    const float bandwidth = std::atof(argv[3]), tail = std::atof(argv[4]), damping = std::atof(argv[5]);

    PlateCore::Plate plate;
    plate.init(static_cast<float>(sr));
    plate.activate();
    plate.setBandwidth(static_cast<float>(TinplateLaw::bandwidthCoef(bandwidth)));
    plate.setDamping(static_cast<float>(TinplateLaw::dampingCoef(damping)));
    plate.setModulation(1.0f);
    const float decay = TinplateLaw::decay(tail);

    std::vector<float> out(2 * n);
    for (size_t i = 0; i < n; ++i)
        plate.process(i == 0 ? 1.0f : 0.0f, decay, &out[2 * i], &out[2 * i + 1]);
    std::fwrite(out.data(), sizeof(float), out.size(), stdout);
    return 0;
}
