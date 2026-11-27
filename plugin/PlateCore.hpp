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
 * a depth (1: the original, up to 2; Freeze takes it to 0) and a rate of modulation (1.2 Hz:
 * the original; changed without a jump of phase), and a size: the lengths of the
 * delays of the tank and the output taps times 0.5 to 2, rounded (1: the original).  A delay
 * reads the sample written n steps before (the read pointer of the original, computed from the
 * write one, same values).  A change of size glides each length linearly to its new integer
 * value, with linear interpolation of the reads while it glides (a brief pitch glide).
 *
 * Stereo input (option, processStereo): each channel through its own input filters, into one half
 * of the tank each.
 *
 * Smooth (setSmooth, 0..1; 0: the original): the allpasses of the tank, with coefficients near
 * 0.72, delay some frequencies much more than others (group delay from 0.16 to 6.3 times their
 * length, peaks every 1 / length): those make fewer round trips per second and outlive the others,
 * so that the end of the tail is a comb of frequencies 11 and 33 Hz apart whose level beats at
 * those rates.  Smooth lowers the four coefficients towards kSmoothDiff and, to keep the density
 * of echoes, opens two short allpasses at the read end of each of the four delays (kShortMs, not
 * scaled by the size; the delays are that much shorter).  At Smooth 0 the short allpasses are
 * bypassed and the delays have their full length: the original loop and cost.  When Smooth leaves
 * 0, their memories are filled from that of the delay, as if they had run as the pure delays they
 * are at Smooth 0 (exact, out of a glide of the size).  The tail is shorter for the same decay:
 * the caller compensates (TinplateSmooth).
 *
 * Signal flow (mono input, stereo output):
 *   x -> bandwidth low-pass -> 4 input allpasses (diffusion)
 *     -> two cross-coupled halves, each: modulated allpass -> delay -> damping low-pass
 *        -> x decay -> allpass -> delay -> x decay -> into the other half
 *     (Smooth: each delay followed by two short allpasses)
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
    unsigned write = 0;
    unsigned n = 0;          // length: get() gives the sample put n steps before
    float len = 0.0f;        // fractional length while gliding (glide), else n
    bool glide = false;

    // length n, room for lengths up to capacity
    void init(const unsigned length, const unsigned capacity)
    {
        size = nextPowerOf2(std::max(length, capacity) + 1);
        data.assign(size, 0.0f);
        --size;
        write = 0;
        n = length;
        len = static_cast<float>(length);
        glide = false;
    }

    void init(const unsigned length) { init(length, length); }

    void reset() { std::fill(data.begin(), data.end(), 0.0f); }

    float& operator[](const int i) { return data[(write - i) & size]; }

    // fractional tap, linear interpolation
    float tap(const float p)
    {
        const int i = static_cast<int>(p);
        const float f = p - i;
        return f == 0.0f ? (*this)[i] : (1 - f) * (*this)[i] + f * (*this)[i + 1];
    }

    void put(const float x)
    {
        data[write] = x;
        write = (write + 1) & size;
    }

    // the sample put n steps before (before this step's put)
    float get()
    {
        if (!glide)
            return data[(write - n) & size];
        const int i = static_cast<int>(len);
        const float f = len - i;
        return (1 - f) * data[(write - i) & size] + f * data[(write - i - 1) & size];
    }

    float putget(const float x)
    {
        const float y = get();   // read before the put: the slot of the original read pointer
        put(x);
        return y;
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

    // new angular frequency, same phase: the next output continues the current one (no jump)
    void retune(const double w)
    {
        const double last = y[z], before = y[z ^ 1];   // sin(phi), sin(phi - w0)
        const double c = 0.5 * b, s = std::sqrt(std::fmax(1.0 - c * c, 1e-30));
        const double phi = std::atan2(last, (last * c - before) / s);
        setW(w, phi + w);
    }

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

    // room for a width up to 2 w (depth 200 %)
    void init(const int n, const int w, const int capacity)
    {
        n0 = static_cast<float>(n);
        width = static_cast<float>(w);
        delay.init(n + w, capacity + 2 * w + 2);
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

    // Smooth at 1: coefficient of the four allpasses of the tank, and of the short allpasses
    static constexpr float kSmoothDiff = 0.5f, kShortDiff = 0.5f;

    struct Input {
        LP1 bandwidth;
        Lattice lattice[4];
    };
    Input input;
    Input input2;   // stereo input: the second channel, same filters (processStereo)

    struct {
        ModLattice mlattice[2];
        Lattice lattice[2];
        Delay delay[4];
        LP1 damping[2];
        int taps[12];
        Lattice shorts[4][2];   // Smooth: short allpasses at the read end of each delay
    } tank;

    static constexpr float kMinSize = 0.5f, kMaxSize = 2.0f;

    // lengths of the delay lines for sample rate fs, size 1 (allocates room for kMaxSize)
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
        const auto room = [&](const int i) { return static_cast<unsigned>(std::ceil(kMaxSize * L(i))) + 2; };

        for (Input* in : { &input, &input2 })
        {
            in->lattice[0].init(L(0));   // lh
            in->lattice[1].init(L(1));
            in->lattice[2].init(L(2));   // rh
            in->lattice[3].init(L(3));
        }

        // modulated, width about 12 samples @ 44.1
        tank.mlattice[0].init(L(4), static_cast<int>(0.000403221 * fs), room(4));
        tank.mlattice[1].init(L(5), static_cast<int>(0.000403221 * fs), room(5));
        fModWidth = tank.mlattice[0].width;

        tank.delay[0].init(L(6), room(6));      // lh
        tank.lattice[0].init(L(7), room(7));
        tank.delay[1].init(L(8), room(8));

        tank.delay[2].init(L(9), room(9));      // rh
        tank.lattice[1].init(L(10), room(10));
        tank.delay[3].init(L(11), room(11));

        // the short allpasses after each delay (Smooth), ms; not scaled by the size
        static const float kShortMs[4][2] = { { 3.1f, 4.7f }, { 2.3f, 5.3f }, { 3.7f, 4.3f }, { 2.9f, 5.9f } };
        for (int i = 0; i < 4; ++i)
        {
            fShort[i] = 0;
            for (int j = 0; j < 2; ++j)
            {
                const int m = static_cast<int>(kShortMs[i][j] * 1e-3f * fs);
                tank.shorts[i][j].init(m);
                fShort[i] += m;
            }
        }

        static const float t[] = {
            0.0089378717113000241f, 0.099929437854910791f, 0.064278754074123853f,
            0.067067638856221232f, 0.066866032727394914f, 0.006283391015086859f,
            0.01186116057928161f, 0.12187090487550822f, 0.041262054366452743f,
            0.089815530392123921f, 0.070931756325392295f, 0.011256342192802662f
        };
        for (int i = 0; i < 12; ++i)
            tank.taps[i] = static_cast<int>(t[i] * fs);

        // lengths that follow the size: 4 delays (with their short allpasses), 2 allpasses,
        // 2 modulated allpasses, 12 taps
        for (int i = 0; i < 4; ++i)
            fBase[i] = static_cast<float>(tank.delay[i].n);
        for (int i = 0; i < 2; ++i)
        {
            fBase[4 + i] = static_cast<float>(tank.lattice[i].n);
            fBase[6 + i] = tank.mlattice[i].n0;
        }
        for (int i = 0; i < 12; ++i)
            fBase[8 + i] = static_cast<float>(tank.taps[i]);
        for (int i = 0; i < kLengths; ++i)
            fLen[i] = fTarget[i] = fBase[i];
        fSize = 1.0f;
        fGlideLeft = 0;
        apply();
    }

    // size of the tank (kMinSize .. kMaxSize, 1 = the original); new lengths reached in steps samples
    void setSize(float s, const int steps)
    {
        s = std::fmin(std::fmax(s, kMinSize), kMaxSize);
        if (s == fSize)
            return;
        fSize = s;
        for (int i = 0; i < kLengths; ++i)
        {
            fTarget[i] = std::round(fBase[i] * s);
            fStep[i] = steps > 0 ? (fTarget[i] - fLen[i]) / steps : 0.0f;
        }
        fGlideLeft = steps;
        if (steps <= 0)
            finishGlide();
        else
            apply();
    }

    // lengths at their targets at once (an empty plate: nothing to glide)
    void finishGlide()
    {
        for (int i = 0; i < kLengths; ++i)
            fLen[i] = fTarget[i];
        fGlideLeft = 0;
        apply();
    }

    bool gliding() const { return fGlideLeft > 0; }

    // state cleared, modulation restarted (PlateStub::activate)
    void activate()
    {
        input.bandwidth.reset();
        resetInput2();
        for (int i = 0; i < 4; ++i)
        {
            input.lattice[i].reset();
            tank.delay[i].reset();
            tank.shorts[i][0].reset();
            tank.shorts[i][1].reset();
        }
        for (int i = 0; i < 2; ++i)
        {
            tank.mlattice[i].reset();
            tank.lattice[i].reset();
            tank.damping[i].reset();
        }
        tank.mlattice[0].lfo.setF(fRate, fFs, fPhase);
        tank.mlattice[1].lfo.setF(fRate, fFs, fPhase + .5 * M_PI);
    }

    // phase of the modulation at activate(), radians (0: the original); a second plate in
    // parallel starts elsewhere, so that two plates with the same settings are not identical
    void setPhase(const double phase) { fPhase = phase; }

    // rate of the modulation, Hz (the original: 1.2), changed without a jump of phase
    void setModRate(const double hz)
    {
        if (hz == fRate)
            return;
        fRate = hz;
        tank.mlattice[0].lfo.retune(2 * M_PI * hz / fFs);
        tank.mlattice[1].lfo.retune(2 * M_PI * hz / fFs);
    }

    // the second input channel emptied (stereo input off: it is not computed)
    void resetInput2()
    {
        input2.bandwidth.reset();
        for (int i = 0; i < 4; ++i)
            input2.lattice[i].reset();
    }

    // coefficients of the two low-passes (a0)
    void setBandwidth(const float a0)
    {
        input.bandwidth.set(a0);
        input2.bandwidth.set(a0);
    }
    void setDamping(const float a0)
    {
        tank.damping[0].set(a0);
        tank.damping[1].set(a0);
    }

    // smoothness of the tail, 0..1 (0: the original coefficients, the short allpasses pure delays);
    // the four coefficients go linearly to kSmoothDiff, those of the short allpasses as s^3 to
    // kShortDiff (measured: opened linearly, they make the ripple worse half way)
    void setSmooth(const float s)
    {
        if (s == fSmooth)
            return;
        fSmooth = s;
        dediff1 = s <= 0.0f ? .723f : .723f + s * (kSmoothDiff - .723f);
        dediff2 = s <= 0.0f ? .729f : .729f + s * (kSmoothDiff - .729f);
        fShortDiff = s <= 0.0f ? 0.0f : s * s * s * kShortDiff;   // weak, they add beats of their own
        if ((s > 0.0f) != fShortsOn)
        {
            fShortsOn = s > 0.0f;
            if (fShortsOn)
                primeShorts();
            apply();
        }
    }

    // depth of the modulation, 1 = original, up to 2; 0: integer delays, no interpolation loss
    void setModulation(const float scale)
    {
        tank.mlattice[0].width = tank.mlattice[1].width = fModWidth * scale;
    }

    // one mono sample in, one stereo sample out (PlateStub::process)
    void process(const float x, const float decay, float* const outL, float* const outR)
    {
        glideStep();
        const float d = diffuse(input, x);
        tankStep(d, d, decay, outL, outR);
    }

    // stereo input: a and b each through its own input filters (the same as the original's);
    // a feeds the right half of the tank, whose taps make most of the left output, and b, by
    // the weight t, the left half (1 - t: a, as the mono input).  t = 0: process(a), to the bit;
    // t = 1: a = left channel, b = right channel, the reverb follows the panning; a == b: the
    // output of process(a).
    void processStereo(const float a, const float b, const float t, const float decay, float* const outL,
                       float* const outR)
    {
        glideStep();
        const float da = diffuse(input, a), db = diffuse(input2, b);
        tankStep(da + (db - da) * t, da, decay, outL, outR);
    }

private:
    void glideStep()
    {
        if (fGlideLeft > 0)
        {
            if (--fGlideLeft == 0)
                for (int i = 0; i < kLengths; ++i)
                    fLen[i] = fTarget[i];
            else
                for (int i = 0; i < kLengths; ++i)
                    fLen[i] += fStep[i];
            apply();
        }
    }

    // bandwidth low-pass and the 4 input allpasses (diffusion)
    float diffuse(Input& in, float x)
    {
        x = in.bandwidth.process(x);

        x = in.lattice[0].process(x, indiff1);   // lh
        x = in.lattice[1].process(x, indiff1);
        x = in.lattice[2].process(x, indiff2);   // rh
        x = in.lattice[3].process(x, indiff2);
        return x;
    }

    // the tank, its two halves fed with xLeft and xRight (the original: both the same)
    void tankStep(const float inLeft, const float inRight, const float decay, float* const outL, float* const outR)
    {
        // summation point
        double xl = inLeft + decay * delayGet(3);
        double xr = inRight + decay * delayGet(1);

        // lh
        xl = tank.mlattice[0].process(xl, dediff1);
        xl = delayPutGet(0, xl);
        xl = tank.damping[0].process(xl);
        xl *= decay;
        xl = tank.lattice[0].process(xl, dediff2);
        tank.delay[1].put(xl);

        // rh
        xr = tank.mlattice[1].process(xr, dediff1);
        xr = delayPutGet(2, xr);
        xr = tank.damping[1].process(xr);
        xr *= decay;
        xr = tank.lattice[1].process(xr, dediff2);
        tank.delay[3].put(xr);

        // gather output
        if (!fGlide)
        {
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
        }
        else
        {
            const float* const t = fLen + 8;   // fractional taps while the size glides
            xl  = .6 * tank.delay[2].tap(t[0]);
            xl += .6 * tank.delay[2].tap(t[1]);
            xl -= .6 * tank.lattice[1].tap(t[2]);
            xl += .6 * tank.delay[3].tap(t[3]);
            xl -= .6 * tank.delay[0].tap(t[4]);
            xl += .6 * tank.lattice[0].tap(t[5]);

            xr  = .6 * tank.delay[0].tap(t[6]);
            xr += .6 * tank.delay[0].tap(t[7]);
            xr -= .6 * tank.lattice[0].tap(t[8]);
            xr += .6 * tank.delay[1].tap(t[9]);
            xr -= .6 * tank.delay[2].tap(t[10]);
            xr += .6 * tank.lattice[1].tap(t[11]);
        }

        *outL = static_cast<float>(xl);
        *outR = static_cast<float>(xr);
    }

    // the two short allpasses after delay i (alternate signs; pure delays at Smooth 0)
    float shorts(const int i, const float x)
    {
        return tank.shorts[i][1].process(tank.shorts[i][0].process(x, fShortDiff), -fShortDiff);
    }

    // output of delay i, through its short allpasses when Smooth is on
    float delayGet(const int i) { return fShortsOn ? shorts(i, tank.delay[i].get()) : tank.delay[i].get(); }

    // delay i and its short allpasses: x in, the output of the chain out (as Delay::putget)
    float delayPutGet(const int i, const float x)
    {
        const float y = delayGet(i);
        tank.delay[i].put(x);
        return y;
    }

    // Smooth leaves 0: the short allpasses as if they had run as pure delays, the first one fed
    // the samples of delay i from its shortened length on, the second one the output of the first
    void primeShorts()
    {
        for (int i = 0; i < 4; ++i)
        {
            Delay& d = tank.delay[i];
            unsigned k = static_cast<unsigned>(fLen[i]) - fShort[i];   // shortened length
            for (Lattice& a : tank.shorts[i])
            {
                for (unsigned j = 1; j <= a.n; ++j)
                    a.data[(a.write - j) & a.size] = d.data[(d.write - k - j) & d.size];
                k += a.n;
            }
        }
    }

    // lengths of the current step into the delays; interpolated reads while they glide
    void apply()
    {
        fGlide = fGlideLeft > 0;
        for (int i = 0; i < 4; ++i)
            setLength(tank.delay[i], fShortsOn ? fLen[i] - fShort[i] : fLen[i]);   // the short allpasses: the rest
        for (int i = 0; i < 2; ++i)
        {
            setLength(tank.lattice[i], fLen[4 + i]);
            tank.mlattice[i].n0 = fLen[6 + i];
        }
        for (int i = 0; i < 12; ++i)
            tank.taps[i] = static_cast<int>(fLen[8 + i]);
    }

    void setLength(Delay& d, const float len)
    {
        d.len = len;
        d.n = static_cast<unsigned>(len);
        d.glide = fGlide;
    }

    static const int kLengths = 20;
    float fFs = 44100.0f;
    float fModWidth = 0.0f;
    double fRate = 1.2;
    double fPhase = 0.0;
    float fSize = 1.0f;
    float fSmooth = 0.0f, fShortDiff = 0.0f;
    bool fShortsOn = false;   // Smooth > 0: the short allpasses in the loop
    int fShort[4] = {};   // total length of the short allpasses after each delay, samples
    float fBase[kLengths] = {}, fLen[kLengths] = {}, fTarget[kLengths] = {}, fStep[kLengths] = {};
    int fGlideLeft = 0;
    bool fGlide = false;
};

}   // namespace PlateCore

#endif
