/*
 * Tinplate Reverb - DSP.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.  See COPYING.
 *
 * Core: the plate reverb of CAPS by Tim Goetze, PlateX2 (called Plate2x2 in the 0.4 series):
 * Dattorro's plate, both inputs summed into the tank, stereo output (PlateCore.hpp).  The four
 * controls keep their 0..1 ranges and laws (TinplateParams.hpp).
 *
 * Signal flow:
 *   x = (L + R + normal) / 2 -> pre-delay -> x input gain (freeze) -> plate tank
 *     -> low cut -> width -> wet;  out = wet x blend^1.53 + dry x (1 - blend^1.53)
 *     -> output gain -> bypass crossfade
 *
 * With pre-delay 0, low cut off, width 100 %, output 0 dB, freeze off (preset "Original
 * (CAPS)" and any values of the four controls), the output is that of the original, sample for
 * sample, when the controls stay still (tests/test_dsp.cpp, against the original code).  The
 * original applies a change of a control at the next block (zipper noise on Tail and Blend);
 * here every coefficient goes linearly to its new value in 20 ms, then takes it exactly.
 *
 * "normal": the original adds a constant of +-1e-20 (-400 dB) to the input, its sign flipped
 * at every sample and after each block, to keep the tank out of denormal numbers; kept, for the
 * sample-exact output.  run() also switches FTZ / DAZ on and gives the host its own state back.
 *
 * Freeze: the input of the tank fades out, decay goes to 1 and damping to none in 20 ms, and
 * the depth of the modulation to 0 in 200 ms: the loop is then made of allpasses and integer
 * delays only and keeps its energy at every frequency (with the modulation, the linear
 * interpolation of the modulated delays loses 11 dB at 8 kHz in 10 s).  The dry signal still
 * passes.
 *
 * Pre-delay: integer delay in samples; a change crossfades from the old to the new delay in
 * 30 ms (no pitch glide), the next change waits for the end of the crossfade.
 */

#include "DistrhoPlugin.hpp"
#include "TinplateParams.hpp"
#include "PlateCore.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#if defined(__SSE__) || defined(_M_X64)
# include <xmmintrin.h>
#endif

START_NAMESPACE_DISTRHO

// Flush-to-zero and denormals-are-zero (SSE) for the lifetime of the object, then the previous
// state (it belongs to the host thread).  Nothing on other processors.
class ScopedNoDenormals
{
public:
#if defined(__SSE__) || defined(_M_X64)
    ScopedNoDenormals() : fSaved(_mm_getcsr()) { _mm_setcsr(fSaved | 0x8040u); }   // FTZ bit 15, DAZ bit 6
    ~ScopedNoDenormals() { _mm_setcsr(fSaved); }

private:
    const unsigned fSaved;
#endif
};

static inline float dbToGain(const float db) { return std::pow(10.0f, db * 0.05f); }

static inline float gainToDb(const float g, const float floorDb)
{
    return g > 0.0f ? std::fmax(20.0f * std::log10(g), floorDb) : floorDb;
}

// Linear ramp to a target in a given number of samples, then exactly the target.
template <typename T>
struct Ramp
{
    T cur = 0, target = 0, step = 0;
    int left = 0;

    void reset(const T v)
    {
        cur = target = v;
        left = 0;
    }

    void set(const T v, const int n)
    {
        if (v == target)
            return;
        target = v;
        if (n <= 0)
        {
            cur = v;
            left = 0;
            return;
        }
        step = (v - cur) / n;
        left = n;
    }

    bool moving() const { return left > 0; }

    T next()
    {
        if (left > 0)
        {
            if (--left == 0)
                cur = target;
            else
                cur += step;
        }
        return cur;
    }
};

// 2nd-order Butterworth high-pass (RBJ), transposed direct form II in double.  Off: identity
// coefficients, so the output is the input exactly once the state has emptied (two samples).
struct LowCut
{
    Ramp<double> c[5];   // b0, b1, b2, a1, a2
    double z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    static void coefficients(const float hz, const double fs, double k[5])
    {
        if (hz <= kLowCutOff)
        {
            k[0] = 1.0;
            k[1] = k[2] = k[3] = k[4] = 0.0;
            return;
        }
        const double w = 2.0 * M_PI * std::fmin(hz, 0.45 * fs) / fs;
        const double cw = std::cos(w), alpha = std::sin(w) / (2.0 * M_SQRT1_2);
        const double a0 = 1.0 + alpha;
        k[0] = (1.0 + cw) * 0.5 / a0;
        k[1] = -(1.0 + cw) / a0;
        k[2] = k[0];
        k[3] = -2.0 * cw / a0;
        k[4] = (1.0 - alpha) / a0;
    }

    void reset(const float hz, const double fs)
    {
        double k[5];
        coefficients(hz, fs, k);
        for (int i = 0; i < 5; ++i)
            c[i].reset(k[i]);
        z1[0] = z1[1] = z2[0] = z2[1] = 0.0;
    }

    void set(const float hz, const double fs, const int n)
    {
        double k[5];
        coefficients(hz, fs, k);
        for (int i = 0; i < 5; ++i)
            c[i].set(k[i], n);
    }

    bool identity() const
    {
        return !c[0].moving() && c[0].cur == 1.0 && c[1].cur == 0.0 && c[2].cur == 0.0 && c[3].cur == 0.0
            && c[4].cur == 0.0 && z1[0] == 0.0 && z1[1] == 0.0 && z2[0] == 0.0 && z2[1] == 0.0;
    }

    void process(float& l, float& r)
    {
        double k[5];
        for (int i = 0; i < 5; ++i)
            k[i] = c[i].next();
        float* const io[2] = { &l, &r };
        for (int ch = 0; ch < 2; ++ch)
        {
            const double x = *io[ch];
            const double y = k[0] * x + z1[ch];
            z1[ch] = k[1] * x - k[3] * y + z2[ch];
            z2[ch] = k[2] * x - k[4] * y;
            *io[ch] = static_cast<float>(y);
        }
    }
};

class TinplatePlugin : public Plugin
{
public:
    TinplatePlugin()
        : Plugin(kParamCount, kTinplatePresetCount, kStateCount)
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
            fParams[i] = kTinplateRanges[i].def;
        allocate();
        resetState();
    }

protected:
    const char* getLabel() const override { return "TinplateReverb"; }
    const char* getDescription() const override
    {
        return "Plate reverb after CAPS PlateX2 (Plate2x2) by Tim Goetze: Dattorro's plate, "
               "with pre-delay, low cut, width and freeze.";
    }
    const char* getMaker() const override { return "Tinplate"; }
    const char* getLicense() const override { return "GPL-3.0-or-later"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }

    void initParameter(const uint32_t index, Parameter& p) override
    {
        if (index == kParamBypass)
        {
            p.initDesignation(kParameterDesignationBypass);
            return;
        }

        p.ranges.min = kTinplateRanges[index].min;
        p.ranges.max = kTinplateRanges[index].max;
        p.ranges.def = kTinplateRanges[index].def;
        p.hints = kParameterIsAutomatable;

        switch (index)
        {
        case kParamBandwidth: set(p, "Bandwidth", "bandwidth", ""); break;
        case kParamTail:      set(p, "Tail", "tail", ""); break;
        case kParamDamping:   set(p, "Damping", "damping", ""); break;
        case kParamBlend:     set(p, "Blend", "blend", ""); break;
        case kParamPredelay:  set(p, "Pre-delay", "predelay", "ms"); break;
        case kParamLowCut:
            set(p, "Low cut", "lowcut", "Hz");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamWidth:     set(p, "Width", "width", "%"); break;
        case kParamOutput:    set(p, "Output", "output", "dB"); break;
        case kParamFreeze:
            set(p, "Freeze", "freeze", "");
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            break;
        case kParamMeterIn:  output(p, "Input level", "meter_in", "dB"); break;
        case kParamMeterWet: output(p, "Reverb level", "meter_wet", "dB"); break;
        case kParamMeterOut: output(p, "Output level", "meter_out", "dB"); break;
        }
    }

    void initProgramName(const uint32_t index, String& name) override
    {
        name = kTinplatePresets[index].name;
    }

    // state of the interface (A/B comparison), saved with the project; nothing for the DSP
    void initState(const uint32_t index, State& state) override
    {
        if (index >= static_cast<uint32_t>(kStateCount))
            return;
        state.key = kStateKeys[index];
        state.defaultValue = "";
        state.label = "A/B comparison";
        state.hints = kStateIsOnlyForUI;
    }

    void setState(const char* const key, const char* const value) override
    {
        for (int i = 0; i < kStateCount; ++i)
            if (std::strcmp(key, kStateKeys[i]) == 0)
                fStates[i] = value;
    }

    String getState(const char* const key) const override
    {
        for (int i = 0; i < kStateCount; ++i)
            if (std::strcmp(key, kStateKeys[i]) == 0)
                return fStates[i];
        return String();
    }

    void loadProgram(const uint32_t index) override
    {
        if (index >= static_cast<uint32_t>(kTinplatePresetCount))
            return;
        for (int i = 0; i < kPresetParamCount; ++i)
            fParams[i] = kTinplatePresets[index].values[i];
    }

    float getParameterValue(const uint32_t index) const override
    {
        return fParams[index];
    }

    void setParameterValue(const uint32_t index, const float value) override
    {
        if (index < static_cast<uint32_t>(kParamFirstOutput))
            fParams[index] = value;
    }

    void activate() override
    {
        resetState();
    }

    void sampleRateChanged(double) override
    {
        allocate();
        resetState();
    }

    void run(const float** inputs, float** outputs, const uint32_t frames) override
    {
        const ScopedNoDenormals noDenormals;
        if (frames == 0)
            return;
        setTargets(fRamp);

        const float* const inL = inputs[0];
        const float* const inR = inputs[1];
        float* const outL = outputs[0];
        float* const outR = outputs[1];
        float peakIn = 0.0f, peakWet = 0.0f, peakOut = 0.0f;

        for (uint32_t i = 0; i < frames; ++i)
        {
            const float sl = inL[i], sr = inR[i];
            fNormal = -fNormal;
            float x = (sl + sr + fNormal) * .5;
            x = preDelay(x) * fInGain.next();

            if (fBandwidth.moving())
                fPlate.setBandwidth(fBandwidth.next());
            if (fDamping.moving())
                fPlate.setDamping(fDamping.next());
            if (fModulation.moving())
                fPlate.setModulation(fModulation.next());

            float wl, wr;
            fPlate.process(x, fDecay.next(), &wl, &wr);

            if (!fLowCut.identity())
                fLowCut.process(wl, wr);
            if (fWidth.moving() || fWidth.cur != 1.0f)
            {
                const float w = fWidth.next();
                const float m = (wl + wr) * 0.5f, s = (wl - wr) * 0.5f * w;
                wl = m + s;
                wr = m - s;
            }
            peakWet = std::fmax(peakWet, std::fmax(std::fabs(wl), std::fabs(wr)));

            const float blend = fWet.next();
            const float dry = 1 - blend;
            const float g = fGain.next();
            float yl = (blend * wl + dry * sl) * g;
            float yr = (blend * wr + dry * sr) * g;

            // bypass: the input, crossfaded
            const float b = fBypass.next();
            if (b == 1.0f)
            {
                yl = sl;
                yr = sr;
            }
            else if (b != 0.0f)
            {
                yl += (sl - yl) * b;
                yr += (sr - yr) * b;
            }
            outL[i] = yl;
            outR[i] = yr;
            peakIn = std::fmax(peakIn, std::fmax(std::fabs(sl), std::fabs(sr)));
            peakOut = std::fmax(peakOut, std::fmax(std::fabs(yl), std::fabs(yr)));
        }
        fNormal = -fNormal;   // as the original after each block

        const float fall = std::exp(-static_cast<float>(frames) / (0.25f * static_cast<float>(getSampleRate())));
        fMeterIn = std::fmax(peakIn, fMeterIn * fall);
        fMeterWet = std::fmax(peakWet, fMeterWet * fall);
        fMeterOut = std::fmax(peakOut, fMeterOut * fall);
        fParams[kParamMeterIn] = gainToDb(fMeterIn, -60.0f);
        fParams[kParamMeterWet] = gainToDb(fMeterWet, -60.0f);
        fParams[kParamMeterOut] = gainToDb(fMeterOut, -60.0f);
    }

private:
    static void set(Parameter& p, const char* name, const char* symbol, const char* unit)
    {
        p.name = name;
        p.symbol = symbol;
        p.unit = unit;
    }

    static void output(Parameter& p, const char* name, const char* symbol, const char* unit)
    {
        p.hints = kParameterIsOutput;
        p.name = name;
        p.symbol = symbol;
        p.unit = unit;
        p.ranges.min = -60.0f;
        p.ranges.max = 6.0f;
        p.ranges.def = -60.0f;
    }

    void allocate()
    {
        const double sr = getSampleRate();
        fPlate.init(static_cast<float>(sr));
        const unsigned n = PlateCore::nextPowerOf2(static_cast<unsigned>(0.001 * kTinplateRanges[kParamPredelay].max * sr) + 2);
        fPre.assign(n, 0.0f);
        fPreMask = n - 1;
    }

    // targets of the ramps from the parameters; ramp = false: taken at once
    void setTargets(const bool ramp)
    {
        const double sr = getSampleRate();
        const int n = ramp ? static_cast<int>(0.02 * sr) : 0;
        const bool freeze = fParams[kParamFreeze] > 0.5f;

        fBandwidth.set(static_cast<float>(TinplateLaw::bandwidthCoef(fParams[kParamBandwidth])), n);
        fDecay.set(freeze ? 1.0f : TinplateLaw::decay(fParams[kParamTail]), n);
        fDamping.set(freeze ? 1.0f : static_cast<float>(TinplateLaw::dampingCoef(fParams[kParamDamping])), n);
        fInGain.set(freeze ? 0.0f : 1.0f, n);
        fModulation.set(freeze ? 0.0f : 1.0f, ramp ? static_cast<int>(0.2 * sr) : 0);
        fWet.set(TinplateLaw::wet(fParams[kParamBlend]), n);
        fWidth.set(fParams[kParamWidth] * 0.01f, n);
        fGain.set(fParams[kParamOutput] == 0.0f ? 1.0f : dbToGain(fParams[kParamOutput]), n);
        fBypass.set(fParams[kParamBypass] > 0.5f ? 1.0f : 0.0f, ramp ? static_cast<int>(0.01 * sr) : 0);
        fLowCut.set(fParams[kParamLowCut], sr, n);

        const unsigned pd = static_cast<unsigned>(std::lround(fParams[kParamPredelay] * 1e-3 * sr));
        if (!ramp)
        {
            fPreCur = fPreNext = std::min(pd, fPreMask - 1);
            fPreFade = 0;
        }
        else if (fPreFade == 0 && pd != fPreCur)
        {
            fPreNext = std::min(pd, fPreMask - 1);
            fPreFadeLength = fPreFade = std::max(1, static_cast<int>(0.03 * sr));
        }
        if (!ramp)
        {
            fPlate.setBandwidth(fBandwidth.cur);
            fPlate.setDamping(fDamping.cur);
            fPlate.setModulation(fModulation.cur);
        }
    }

    float preDelay(const float x)
    {
        fPre[fPrePos] = x;
        float y = fPre[(fPrePos - fPreCur) & fPreMask];
        if (fPreFade > 0)
        {
            const float t = 1.0f - static_cast<float>(fPreFade) / fPreFadeLength;
            y += (fPre[(fPrePos - fPreNext) & fPreMask] - y) * t;
            if (--fPreFade == 0)
                fPreCur = fPreNext;
        }
        fPrePos = (fPrePos + 1) & fPreMask;
        return y;
    }

    void resetState()
    {
        fPlate.activate();
        fBandwidth.reset(0.0f);
        fDecay.reset(0.0f);
        fDamping.reset(0.0f);
        fInGain.reset(0.0f);
        fModulation.reset(0.0f);
        fWet.reset(0.0f);
        fWidth.reset(0.0f);
        fGain.reset(0.0f);
        fBypass.reset(0.0f);
        fLowCut.reset(fParams[kParamLowCut], getSampleRate());
        std::fill(fPre.begin(), fPre.end(), 0.0f);
        fPrePos = 0;
        fRamp = false;
        setTargets(false);
        fRamp = true;
        fMeterIn = fMeterWet = fMeterOut = 0.0f;
    }

    float fParams[kParamCount];
    String fStates[kStateCount];   // strings of the interface, only kept for the host

    PlateCore::Plate fPlate;
    float fNormal = 1e-20f;        // NOISE_FLOOR of CAPS
    bool fRamp = false;
    Ramp<float> fBandwidth, fDecay, fDamping, fInGain, fModulation, fWet, fWidth, fGain, fBypass;
    LowCut fLowCut;
    std::vector<float> fPre;
    unsigned fPreMask = 0, fPrePos = 0, fPreCur = 0, fPreNext = 0;
    int fPreFade = 0, fPreFadeLength = 1;
    float fMeterIn = 0.0f, fMeterWet = 0.0f, fMeterOut = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TinplatePlugin)
};

Plugin* createPlugin()
{
    return new TinplatePlugin();
}

END_NAMESPACE_DISTRHO
