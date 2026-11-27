/*
 * Tinplate Reverb - Smooth: the tail given to the plate so that its decay time at 500 Hz stays
 * that of Smooth 0.
 *
 * Smooth (PlateCore::Plate::setSmooth) lowers the coefficients of the allpasses of the tank: the
 * frequencies they delayed most, which made the end of the tail and its beats, no longer outlive
 * the others, and the decay is 15 to 40 % shorter for the same Tail.  The measured table
 * Rt60SmoothTable.hpp (tools/rt60_smooth_table.cpp) gives the decay time at 500 Hz against
 * Smooth, Size, Tail and Damping; it is interpolated linearly, and inverted in Tail.  The decay
 * time shown by the interface (Rt60.hpp, measured at Smooth 0) then holds at any Smooth.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TINPLATE_SMOOTH_HPP_INCLUDED
#define TINPLATE_SMOOTH_HPP_INCLUDED

#include "Rt60SmoothTable.hpp"

#include <algorithm>
#include <cmath>

namespace TinplateSmooth {

// tail for the plate at smooth (0..1) with the decay time of tail at Smooth 0; Smooth 0: tail
static inline float tail(const float tail, const float damping, const float size, const float smooth)
{
    if (smooth <= 0.0f)
        return tail;
    using namespace Rt60SmoothTable;
    const auto split = [](double x, const int last, int& i, double& w) {
        x = std::fmin(std::fmax(x, 0.0), static_cast<double>(last));
        i = std::min(static_cast<int>(x), last - 1);
        w = x - i;
    };
    int iz, id, is;
    double wz, wd, ws;
    split((size - kFirstSize) / kSizeStep, kSizes - 1, iz, wz);
    split(damping * (kDampings - 1), kDampings - 1, id, wd);
    split(smooth / kSmoothStep, kSmooths - 1, is, ws);

    // decay time at the smooth row s (weight of the next row w) and at the tail index t
    const auto at = [&](const int s, const double w, const int t) {
        double v = 0.0;
        for (int e = 0; e < 2; ++e)
            for (int a = 0; a < 2; ++a)
                for (int b = 0; b < 2; ++b)
                    v += (e ? w : 1.0 - w) * (a ? wz : 1.0 - wz) * (b ? wd : 1.0 - wd)
                         * kSeconds[s + e][iz + a][t][id + b];
        return v;
    };
    int it;
    double wt;
    split(tail / kTailStep, kTails - 1, it, wt);
    const double target = (1.0 - wt) * at(0, 0.0, it) + wt * at(0, 0.0, it + 1);

    // the first segment of the table at this smooth that reaches the target
    double lo = at(is, ws, 0);
    if (target <= lo)
        return 0.0f;
    for (int t = 0; t + 1 < kTails; ++t)
    {
        const double hi = at(is, ws, t + 1);
        if (target <= hi)
            return static_cast<float>((t + (hi > lo ? (target - lo) / (hi - lo) : 0.0)) * kTailStep);
        lo = hi;
    }
    return static_cast<float>((kTails - 1) * kTailStep);
}

}   // namespace TinplateSmooth

#endif
