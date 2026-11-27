/*
 * Tinplate Reverb - periodic ripple of the level of a tail (the "small waves" heard at its end),
 * measured on an impulse response (tools/ripple.cpp and tests/test_dsp.cpp use it).
 *
 * L + R; level in windows of 5 ms, dB, relative to its mean between 0.1 and 0.2 s.  In segments
 * of 1 s every 0.25 s whose mean level lies in [lo, hi] dB: the level minus a quadratic fit (the
 * decay), Hann window, discrete Fourier transform of that ripple between 3 and 90 Hz; the share of
 * its power in its strongest line (3 bins) says how periodic it is.  Result: mean over the segments
 * of RMS x sqrt(share), dB: the part of the ripple that is a regular beat.  The plate of the
 * original gives 0.9 to 1.3 dB at -40 to -80 dB (beats at 11 and 33 Hz, the lengths of the
 * allpasses of its tank); a random ripple spreads its power over the lines and gives less.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef RIPPLE_MEASURE_HPP_INCLUDED
#define RIPPLE_MEASURE_HPP_INCLUDED

#include <algorithm>
#include <cmath>
#include <vector>

// periodic ripple, dB, over the segments of mean level between lo and hi dB (hi > lo); 0 if none;
// rms: the whole ripple there, dB (optional); f: frequency of the strongest line, Hz (optional)
static inline double periodicRipple(const std::vector<float>& l, const std::vector<float>& r, const double sr,
                                    const double hi, const double lo, double* const rms = nullptr,
                                    double* const f = nullptr)
{
    const double dt = 0.005;
    const size_t w = static_cast<size_t>(dt * sr);
    const size_t nw = l.size() / w;
    std::vector<double> lev(nw);
    for (size_t k = 0; k < nw; ++k)
    {
        double e = 0.0;
        for (size_t i = k * w; i < (k + 1) * w; ++i)
        {
            const double x = static_cast<double>(l[i]) + r[i];
            e += x * x;
        }
        lev[k] = 10.0 * std::log10(e / w + 1e-300);
    }
    double top = 0.0;
    for (size_t k = 20; k < 40; ++k)
        top += lev[k] / 20.0;

    const size_t seg = static_cast<size_t>(1.0 / dt), hop = seg / 4;
    double sumP = 0.0, sumR = 0.0, sumF = 0.0;
    int count = 0;
    std::vector<double> y(seg);
    for (size_t a = static_cast<size_t>(0.25 / dt); a + seg <= nw; a += hop)
    {
        double mean = 0.0;
        for (size_t i = 0; i < seg; ++i)
            mean += lev[a + i] / seg;
        if (mean - top > hi || mean - top < lo)
            continue;
        // quadratic least squares in t = -1 .. 1
        double s[5] = { 0, 0, 0, 0, 0 }, b[3] = { 0, 0, 0 };
        for (size_t i = 0; i < seg; ++i)
        {
            const double t = 2.0 * i / (seg - 1) - 1.0;
            double p = 1.0;
            for (int j = 0; j < 5; ++j, p *= t)
                s[j] += p;
            b[0] += lev[a + i];
            b[1] += t * lev[a + i];
            b[2] += t * t * lev[a + i];
        }
        // normal equations (symmetric 3 x 3), Cramer
        const double m[3][3] = { { s[0], s[1], s[2] }, { s[1], s[2], s[3] }, { s[2], s[3], s[4] } };
        const auto det = [](const double q[3][3]) {
            return q[0][0] * (q[1][1] * q[2][2] - q[1][2] * q[2][1]) - q[0][1] * (q[1][0] * q[2][2] - q[1][2] * q[2][0])
                   + q[0][2] * (q[1][0] * q[2][1] - q[1][1] * q[2][0]);
        };
        const double d = det(m);
        double c[3];
        for (int j = 0; j < 3; ++j)
        {
            double q[3][3];
            for (int u = 0; u < 3; ++u)
                for (int v = 0; v < 3; ++v)
                    q[u][v] = v == j ? b[u] : m[u][v];
            c[j] = det(q) / d;
        }
        double var = 0.0;
        for (size_t i = 0; i < seg; ++i)
        {
            const double t = 2.0 * i / (seg - 1) - 1.0;
            y[i] = lev[a + i] - (c[0] + c[1] * t + c[2] * t * t);
            var += y[i] * y[i] / seg;
        }
        // power spectrum of the windowed ripple, 3 .. 90 Hz (bins of 1 Hz)
        std::vector<double> P;
        for (int k = 3; k < 90; ++k)
        {
            double re = 0.0, im = 0.0;
            for (size_t i = 0; i < seg; ++i)
            {
                const double h = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / seg);
                const double ph = 2.0 * M_PI * k * i / seg;
                re += h * y[i] * std::cos(ph);
                im -= h * y[i] * std::sin(ph);
            }
            P.push_back(re * re + im * im);
        }
        size_t kmax = 1;
        double total = 0.0;
        for (size_t k = 0; k < P.size(); ++k)
        {
            total += P[k];
            if (k >= 1 && k + 1 < P.size() && P[k] > P[kmax])
                kmax = k;
        }
        const double share = total > 0.0 ? (P[kmax - 1] + P[kmax] + P[kmax + 1]) / total : 0.0;
        sumP += std::sqrt(var * share);
        sumR += std::sqrt(var);
        sumF += 3.0 + kmax;
        ++count;
    }
    if (rms)
        *rms = count ? sumR / count : 0.0;
    if (f)
        *f = count ? sumF / count : 0.0;
    return count ? sumP / count : 0.0;
}

#endif
