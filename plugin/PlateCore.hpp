/*
 * Tinplate Reverb - the plate tank of CAPS PlateX2 (formerly Plate2x2).
 *
 * Copyright (C) 2002-13 Tim Goetze <tim@quitte.de> (CAPS, http://quitte.de/dsp/)
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Transcription of PlateStub (Reverb.h, Reverb.cc) and of the DSP classes it uses (dsp/Delay.h,
 * dsp/IIR1.h, dsp/Sine.h) of CAPS 0.9.26, after Jon Dattorro, "Effect Design, Part 1: Reverberator
 * and Other Filters", J. Audio Eng. Soc. 45(9), 1997.  The types of every operation are those of
 * the original (float samples, some products and sums in double), so that the output is the
 * same to the last bit when compiled with the same options (tests/test_dsp.cpp compares it with
 * the original code, tests/caps/).  Changes: no LADSPA, std::vector buffers, the x87-only
 * truncation mode dropped (the original also truncates with (int) on SSE2), and the coefficients
 * set by the caller (for smoothing; with steady controls they are those of PlateX2::cycle()),
 * and a depth of modulation (1: the original; Freeze takes it to 0).
 *
 * Signal flow (mono input, stereo output):
 *   x -> bandwidth low-pass -> 4 input allpasses (diffusion)
 *     -> two cross-coupled halves, each: modulated allpass -> delay -> damping low-pass
 *        -> x decay -> allpass -> delay -> x decay -> into the other half
 *   output: 6 taps per channel, taken in both halves (signs alternate), x 0.6.
 */
#ifndef PLATE_CORE_HPP_INCLUDED
#define PLATE_CORE_HPP_INCLUDED

#include <algorithm>
#include <cmath>
#include <vector>

namespace PlateCore {

inline unsigned nextPowerOf2(unsigned n)
{
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return ++n;
}

// circular delay line; size is a power of two, used as a mask
class Delay
{
public:
    unsigned size = 0;
    std::vector<float> data;
    unsigned read = 0, write = 0;

    void init(const unsigned n)
    {
        size = nextPowerOf2(n);
        data.assign(size, 0.0f);
        --size;
        write = n;
        read = 0;
    }

    void reset() { std::fill(data.begin(), data.end(), 0.0f); }

    float& operator[](const int i) { return data[(write - i) & size]; }

    void put(const float x)
    {
        data[write] = x;
        write = (write + 1) & size;
    }

    float get()
    {
        const float x = data[read];
        read = (read + 1) & size;
        return x;
    }

    float putget(const float x)
    {
        put(x);
        return get();
    }

    // fractional lookup, linear interpolation
    float getLinear(float f)
    {
        const int n = static_cast<int>(f);
        f -= n;
        return (1 - f) * (*this)[n] + f * (*this)[n + 1];
    }
};

// allpass lattice
class Lattice : public Delay
{
public:
    float process(float x, const double d)
    {
        const float y = get();
        x -= d * y;
        put(x);
        return d * x + y;
    }
};

// sine oscillator (recursive)
class Sine
{
public:
    int z = 0;
    double y[2] = { 0.0, 0.0 };
    double b = 0.0;

    void setF(const double f, const double fs, const double phase) { setW(f * 2 * M_PI / fs, phase); }

    void setW(const double w, const double phase)
    {
        b = 2 * std::cos(w);
        y[0] = std::sin(phase - w);
        y[1] = std::sin(phase - 2 * w);
        z = 0;
    }

    double get()
    {
        double s = b * y[z];
        z ^= 1;
        s -= y[z];
        return y[z] = s;
    }
};

// allpass lattice whose delay is modulated by a sine (chorus of the tank)
class ModLattice
{
public:
    float n0 = 0.0f, width = 0.0f;
    Delay delay;
    Sine lfo;

    void init(const int n, const int w)
    {
        n0 = static_cast<float>(n);
        width = static_cast<float>(w);
        delay.init(n + w);
    }

    void reset() { delay.reset(); }

    float process(float x, const double d)
    {
        const float y = delay.getLinear(n0 + width * lfo.get());
        x += d * y;
        delay.put(x);
        return y - d * x;   // note sign
    }
};

// one-pole low-pass y = a0 x + b1 y1
class LP1
{
public:
    float a0 = 1.0f, b1 = 0.0f, y1 = 0.0f;

    float process(const float x) { return y1 = a0 * x + b1 * y1; }
    void reset() { y1 = 0.0f; }
    void set(const float d)
    {
        a0 = d;
        b1 = 1 - d;
    }
};

class Plate
{
public:
    // diffusion coefficients, "tuned for soft attack, ambience"
    float indiff1 = .742f, indiff2 = .712f, dediff1 = .723f, dediff2 = .729f;

    struct {
        LP1 bandwidth;
        Lattice lattice[4];
    } input;

    struct {
        ModLattice mlattice[2];
        Lattice lattice[2];
        Delay delay[4];
        LP1 damping[2];
        int taps[12];
    } tank;

    // lengths of the delay lines for sample rate fs (allocates)
    void init(const float fs)
    {
        fFs = fs;
        static const float l[] = {
            0.004771345048889486f, 0.0035953092974026408f,
            0.01273478713752898f, 0.0093074829474816042f,
            0.022579886428547427f, 0.030509727495715868f,
            0.14962534861059779f, 0.060481838647894894f, 0.12499579987231611f,
            0.14169550754342933f, 0.089244313027116023f, 0.10628003091293972f
        };
        const auto L = [&](const int i) { return static_cast<int>(l[i] * fs); };

        input.lattice[0].init(L(0));   // lh
        input.lattice[1].init(L(1));
        input.lattice[2].init(L(2));   // rh
        input.lattice[3].init(L(3));

        // modulated, width about 12 samples @ 44.1
        tank.mlattice[0].init(L(4), static_cast<int>(0.000403221 * fs));
        tank.mlattice[1].init(L(5), static_cast<int>(0.000403221 * fs));
        fModWidth = tank.mlattice[0].width;

        tank.delay[0].init(L(6));      // lh
        tank.lattice[0].init(L(7));
        tank.delay[1].init(L(8));

        tank.delay[2].init(L(9));      // rh
        tank.lattice[1].init(L(10));
        tank.delay[3].init(L(11));

        static const float t[] = {
            0.0089378717113000241f, 0.099929437854910791f, 0.064278754074123853f,
            0.067067638856221232f, 0.066866032727394914f, 0.006283391015086859f,
            0.01186116057928161f, 0.12187090487550822f, 0.041262054366452743f,
            0.089815530392123921f, 0.070931756325392295f, 0.011256342192802662f
        };
        for (int i = 0; i < 12; ++i)
            tank.taps[i] = static_cast<int>(t[i] * fs);
    }

    // state cleared, modulation restarted (PlateStub::activate)
    void activate()
    {
        input.bandwidth.reset();
        for (int i = 0; i < 4; ++i)
        {
            input.lattice[i].reset();
            tank.delay[i].reset();
        }
        for (int i = 0; i < 2; ++i)
        {
            tank.mlattice[i].reset();
            tank.lattice[i].reset();
            tank.damping[i].reset();
        }
        tank.mlattice[0].lfo.setF(1.2, fFs, 0);
        tank.mlattice[1].lfo.setF(1.2, fFs, .5 * M_PI);
    }

    // coefficients of the two low-passes (a0)
    void setBandwidth(const float a0) { input.bandwidth.set(a0); }
    void setDamping(const float a0)
    {
        tank.damping[0].set(a0);
        tank.damping[1].set(a0);
    }

    // depth of the modulation, 1 = original; 0: integer delays, no interpolation loss (freeze)
    void setModulation(const float scale)
    {
        tank.mlattice[0].width = tank.mlattice[1].width = fModWidth * scale;
    }

    // one mono sample in, one stereo sample out (PlateStub::process)
    void process(float x, const float decay, float* const outL, float* const outR)
    {
        x = input.bandwidth.process(x);

        x = input.lattice[0].process(x, indiff1);   // lh
        x = input.lattice[1].process(x, indiff1);
        x = input.lattice[2].process(x, indiff2);   // rh
        x = input.lattice[3].process(x, indiff2);

        // summation point
        double xl = x + decay * tank.delay[3].get();
        double xr = x + decay * tank.delay[1].get();

        // lh
        xl = tank.mlattice[0].process(xl, dediff1);
        xl = tank.delay[0].putget(xl);
        xl = tank.damping[0].process(xl);
        xl *= decay;
        xl = tank.lattice[0].process(xl, dediff2);
        tank.delay[1].put(xl);

        // rh
        xr = tank.mlattice[1].process(xr, dediff1);
        xr = tank.delay[2].putget(xr);
        xr = tank.damping[1].process(xr);
        xr *= decay;
        xr = tank.lattice[1].process(xr, dediff2);
        tank.delay[3].put(xr);

        // gather output
        xl  = .6 * tank.delay[2][tank.taps[0]];
        xl += .6 * tank.delay[2][tank.taps[1]];
        xl -= .6 * tank.lattice[1][tank.taps[2]];
        xl += .6 * tank.delay[3][tank.taps[3]];
        xl -= .6 * tank.delay[0][tank.taps[4]];
        xl += .6 * tank.lattice[0][tank.taps[5]];

        xr  = .6 * tank.delay[0][tank.taps[6]];
        xr += .6 * tank.delay[0][tank.taps[7]];
        xr -= .6 * tank.lattice[0][tank.taps[8]];
        xr += .6 * tank.delay[1][tank.taps[9]];
        xr -= .6 * tank.delay[2][tank.taps[10]];
        xr += .6 * tank.lattice[1][tank.taps[11]];

        *outL = static_cast<float>(xl);
        *outR = static_cast<float>(xr);
    }

private:
    float fFs = 44100.0f;
    float fModWidth = 0.0f;
};

}   // namespace PlateCore

#endif
