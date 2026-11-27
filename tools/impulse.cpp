/*
 * Tinplate Reverb - impulse response of the plate alone (wet, before low cut and width), for the
 * figures of the manual (doc/figures.py builds and calls it).
 *
 *   g++ -O2 -std=gnu++17 -Iplugin tools/impulse.cpp -o build/impulse
 *   ./build/impulse SR SECONDS BANDWIDTH TAIL DAMPING [left] [smooth=S] > ir.f32
 *
 * Output: interleaved L, R as raw 32-bit floats; unit impulse into the tank, steady controls
 * (the coefficients of the original, TinplateParams.hpp), no "normal" constant.
 * "left": stereo input (PlateCore::processStereo), the impulse on the left channel only.
 * "smooth=S": Smooth S (0..1), the tail compensated as in the plugin (Smooth.hpp).
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "PlateCore.hpp"
#include "Smooth.hpp"
#include "TinplateParams.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int main(int argc, char** argv)
{
    bool left = false;
    float smooth = 0.0f;
    bool ok = argc >= 6;
    for (int i = 6; ok && i < argc; ++i)
        if (std::strcmp(argv[i], "left") == 0)
            left = true;
        else if (std::strncmp(argv[i], "smooth=", 7) == 0)
            smooth = std::atof(argv[i] + 7);
        else
            ok = false;
    if (!ok)
    {
        std::fprintf(stderr, "usage: impulse SR SECONDS BANDWIDTH TAIL DAMPING [left] [smooth=S]\n");
        return 1;
    }
    const double sr = std::atof(argv[1]);
    const size_t n = static_cast<size_t>(std::atof(argv[2]) * sr);
    const float bandwidth = std::atof(argv[3]), tail = std::atof(argv[4]), damping = std::atof(argv[5]);

    PlateCore::Plate plate;
    plate.init(static_cast<float>(sr));
    plate.setSmooth(smooth);
    plate.activate();
    plate.setBandwidth(static_cast<float>(TinplateLaw::bandwidthCoef(bandwidth)));
    plate.setDamping(static_cast<float>(TinplateLaw::dampingCoef(damping)));
    plate.setModulation(1.0f);
    const float decay = TinplateLaw::decay(TinplateSmooth::tail(tail, damping, 1.0f, smooth));

    std::vector<float> out(2 * n);
    for (size_t i = 0; i < n; ++i)
        if (left)
            plate.processStereo(i == 0 ? 1.0f : 0.0f, 0.0f, 1.0f, decay, &out[2 * i], &out[2 * i + 1]);
        else
            plate.process(i == 0 ? 1.0f : 0.0f, decay, &out[2 * i], &out[2 * i + 1]);
    std::fwrite(out.data(), sizeof(float), out.size(), stdout);
    return 0;
}
