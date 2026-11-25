/*
 * Tinplate Reverb - decay time measured on an impulse response (tools/rt60_table.cpp and
 * tests/test_dsp.cpp use the same method).
 *
 * L + R, band-pass around f (RBJ, constant 0 dB peak, Q = 2, about 2/3 octave), Schroeder
 * backward integral of the energy, slope between -5 and -25 dB extrapolated to 60 dB (T20).
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef RT60_MEASURE_HPP_INCLUDED
#define RT60_MEASURE_HPP_INCLUDED

#include <cmath>
#include <vector>

// seconds; -1 if the response does not fall by 25 dB
static inline double measureRt60(const std::vector<float>& l, const std::vector<float>& r, const double f,
                                 const double sr)
{
    const double w = 2.0 * M_PI * f / sr, alpha = std::sin(w) / 4.0, a0 = 1.0 + alpha;
    const double b0 = alpha / a0, b2 = -alpha / a0, a1 = -2.0 * std::cos(w) / a0, a2 = (1.0 - alpha) / a0;
    std::vector<double> e(l.size());
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (size_t i = 0; i < l.size(); ++i)
    {
        const double x = static_cast<double>(l[i]) + r[i];
        const double y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        e[i] = y * y;
    }
    for (size_t i = e.size() - 1; i-- > 0;)
        e[i] += e[i + 1];
    size_t i5 = 0, i25 = 0;
    for (size_t i = 0; i < e.size() && e[0] > 0.0; ++i)
    {
        const double d = 10.0 * std::log10(e[i] / e[0] + 1e-300);
        if (i5 == 0 && d <= -5.0)
            i5 = i;
        if (i25 == 0 && d <= -25.0)
            i25 = i;
    }
    return i25 > 0 ? 3.0 * static_cast<double>(i25 - i5) / sr : -1.0;
}

#endif
