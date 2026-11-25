/*
 * Tinplate Reverb - decay time (RT60) of the plate shown by the interface: the measured table
 * Rt60Table.hpp (tools/rt60_table.cpp), interpolated linearly in Tail, Damping and octaves.
 *
 * Why a measured table: the round trip of the tank (0.725 s, gain decay^4 and twice the
 * damping low-pass) alone gives too short a time (by 0.5 to 0.65 s at small Tail): the
 * allpasses of the tank, with coefficients near 0.72, ring on their own (0.5 s even with
 * Tail at 0), and their group delay varies with frequency.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TINPLATE_RT60_HPP_INCLUDED
#define TINPLATE_RT60_HPP_INCLUDED

#include "Rt60Table.hpp"

#include <algorithm>
#include <cmath>

namespace TinplateRt60 {

// seconds, at frequency f (Hz) for the sample rate fs; Freeze aside
static inline double seconds(const float tail, const float damping, const double f, const double fs)
{
    using namespace Rt60Table;
    const auto split = [](double x, const int last, int& i, double& w) {
        x = std::fmin(std::fmax(x, 0.0), static_cast<double>(last));
        i = std::min(static_cast<int>(x), last - 1);
        w = x - i;
    };
    int it, id, jf;
    double wt, wd, wf;
    split(tail * (kTails - 1), kTails - 1, it, wt);
    split(damping * (kDampings - 1), kDampings - 1, id, wd);
    split(std::log2(f * 44100.0 / fs / kFirstHz), kFreqs - 1, jf, wf);
    double v = 0.0;
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b)
            for (int c = 0; c < 2; ++c)
                v += (a ? wt : 1.0 - wt) * (b ? wd : 1.0 - wd) * (c ? wf : 1.0 - wf)
                     * kSeconds[it + a][id + b][jf + c];
    return v;
}

}   // namespace TinplateRt60

#endif
