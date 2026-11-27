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
 *   L, R -> pre-delay -> x = (L + R + normal) / 2 -> x input gain (freeze) -> plate tank
 *     -> low cut -> high cut -> mono bass and width -> x ducking gain -> wet;
 *   out = wet x wet level + dry x dry level -> output gain -> bypass crossfade
 *   (levels in dB, -60 = off; CAPS has a blend instead: wet level blend^1.53, dry 1 - that;
 *   Link: the parameter Mix sets both, as the blend of CAPS (wet mix^1.53, dry 1 - wet), the
 *   levels are then ignored; the interface shows the faders at the levels of the mix)
 *
 * With pre-delay 0, low and high cut off, size and width 100 %, output 0 dB, freeze, ducking and
 * clear off, and
 * wet only (wet 0 dB, dry off) or dry only, the output is that of the original with blend 1 or
 * 0, sample for sample, for any values of bandwidth, tail (up to 1) and damping, when the
 * controls stay still (tests/test_dsp.cpp, against the original code).  Other mixes: the gains
 * from dB differ from blend^1.53 by rounding only.  The
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
 * Ducking (1.1.0): the reverb is lowered while the input plays.  Detector: input peak
 * max(|L|, |R|) of each sample, before the pre-delay; cut = as many dB as it goes over the
 * threshold, at most the depth (TinplateParams.hpp); smoothed in dB by a one-pole filter, attack
 * when the cut grows, release when it shrinks.  Depth 0 and the cut back under 1e-3 dB: the
 * gain is not applied at all (output to the bit as without ducking).  Freeze: the frozen tail is
 * ducked too.
 *
 * Clear (1.1.0): a new attack empties the plate.  Two plates: the input goes into the current
 * one; at an attack, the other one (empty) becomes current and the old one, its input closed,
 * fades out (raised cosine, Fade ms), is then emptied and stops computing.  Detection on the
 * input peak envelope max(|L|, |R|) (instant attack, release 30 ms), before the pre-delay: armed
 * while it stays under Floor, once the fade of the old plate has ended; an attack is a rise
 * from under Floor to over Trigger within Window ms (default 20); a slower rise disarms until the input falls
 * under Floor again.  No clearing while frozen (the frozen sound is kept).  Clear off: only
 * the first plate runs, the output is to the bit that without Clear.
 *
 * Size (1.1.0 after the release): lengths of the delays of the tank and of the output taps
 * times 0.5 to 2 (PlateCore), glided in 100 ms; the decay gain is not compensated (a larger
 * plate rings longer at the same Tail).  100 %: the original, to the bit.
 *
 * Pre-delay: integer delay in samples, of both channels; a change crossfades from the old to the
 * new delay in 30 ms (no pitch glide), the next change waits for the end of the crossfade.
 * Locked to the tempo (Sync): a note value (TinplateNote) at the tempo of the host (120 BPM
 * without one), at most 2 s; a change of tempo crossfades the same way.
 *
 * Stereo input (option): L and R each through the input filters of the plate (two sets) and into
 * one half of the tank (PlateCore::processStereo); switched by a 20 ms ramp between the mono and
 * the stereo feeds.  Off: the original.  A centred input gives the same output either way.
 *
 * Mono bass (option): the side signal of the reverb is high-passed (12 dB / octave), the mid
 * untouched: the reverb is mono below the frequency.  Off (minimum): no filter, to the bit.
 *
 * History (outputs for the interface): input and reverb peaks and the cut of the ducking over the
 * last ~30 ms, and the count of the clearings.
 *
 * Two plates in parallel (after 1.2.0): the input of the tank (after the pre-delay, the freeze
 * gain and the stereo input) feeds plate A times cos(a) and plate B times sin(a), a = Split x 90
 * degrees (constant power: the level of the reverb stays about the same over the knob when the
 * two tails are uncorrelated); their outputs are summed before the low cut.  Plate B has its own
 * Bandwidth, Tail, Damping and Size; the modulation (depth, rate), the freeze and the clearing are
 * shared (each plate has its own pair of plates for Clear, switched together).  Its modulation
 * starts 45 degrees later, so that two plates with the same settings still differ a little.  A
 * plate whose send is 0 is computed until its tail has stayed under -120 dBFS for 50 ms, then
 * emptied and no longer computed; it starts again empty, at its settings at once, its send
 * ramped up in 20 ms.  Split 0 (the default): plate A only, gain exactly 1, the output to the bit
 * that of a single plate; Split 100: plate B only.
 */

#include "DistrhoPlugin.hpp"
#include "TinplateParams.hpp"
#include "PlateCore.hpp"
#include "Smooth.hpp"

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

// 2nd-order Butterworth high-pass (low cut) or low-pass (high cut) (RBJ), transposed direct form
// II in double.  Off: identity coefficients, so the output is the input exactly once the state
// has emptied (two samples).
struct Cut2
{
    bool lowpass = false;
    float off = kLowCutOff;   // frequency at which the filter is off (low cut, mono bass: minimum; high cut: maximum)
    Ramp<double> c[5];   // b0, b1, b2, a1, a2
    double z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    void coefficients(const float hz, const double fs, double k[5]) const
    {
        if (lowpass ? hz >= off : hz <= off)
        {
            k[0] = 1.0;
            k[1] = k[2] = k[3] = k[4] = 0.0;
            return;
        }
        const double w = 2.0 * M_PI * std::fmin(hz, 0.45 * fs) / fs;
        const double cw = std::cos(w), alpha = std::sin(w) / (2.0 * M_SQRT1_2);
        const double a0 = 1.0 + alpha;
        const double b = lowpass ? (1.0 - cw) : (1.0 + cw);
        k[0] = b * 0.5 / a0;
        k[1] = (lowpass ? b : -b) / a0;
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

    // one channel only (the first state)
    void process1(float& io)
    {
        double k[5];
        for (int i = 0; i < 5; ++i)
            k[i] = c[i].next();
        const double x = io;
        const double y = k[0] * x + z1[0];
        z1[0] = k[1] * x - k[3] * y + z2[0];
        z2[0] = k[2] * x - k[4] * y;
        io = static_cast<float>(y);
    }
};

// Peak of the last ~30 ms, whatever the block size: 8 bins of 5 ms, the current one and the 6
// previous ones are read (as Brimstone).  The interface samples it every 25 ms without missing a peak.
struct RecentPeak
{
    static const int kBins = 8;
    static const int kRead = 7;
    float bins[kBins] = {};
    int cur = 0;
    unsigned filled = 0;

    void reset()
    {
        for (int i = 0; i < kBins; ++i)
            bins[i] = 0.0f;
        cur = 0;
        filled = 0;
    }

    void add(const float v, const unsigned frames, const unsigned binLength)
    {
        bins[cur] = std::fmax(bins[cur], v);
        filled += frames;
        if (filled >= binLength)
        {
            cur = (cur + 1) % kBins;
            bins[cur] = 0.0f;
            filled = 0;
        }
    }

    float value() const
    {
        float v = 0.0f;
        for (int i = 0; i < kRead; ++i)
            v = std::fmax(v, bins[(cur - i + kBins) % kBins]);
        return v;
    }
};

// One of the two plates in parallel, with the plate that takes over at a clearing, the ramps of
// its own coefficients and its send (share of the input).
struct PlateLine
{
    PlateCore::Plate plate[2];   // clear: the current one fCur, the other one fading out or empty
    Ramp<float> bandwidth, decay, damping, send;
    bool running = false;        // computed: its send is not 0, or its tail still rings
    int quiet = 0;               // samples in a row under kStopLevel while its send is 0
};

static const float kStopLevel = 1e-6f;   // -120 dBFS: a plate whose send is 0 stops under it

class TinplatePlugin : public Plugin
{
public:
    TinplatePlugin()
        : Plugin(kParamCount, kTinplatePresetCount, kStateCount)
    {
        fHighCut.lowpass = true;
        fHighCut.off = kHighCutOff;
        fMonoBass.off = kMonoBassOff;
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
               "with a second plate in parallel, pre-delay, size, smooth, modulation, low and high cut, width, "
               "freeze, ducking and clear.";
    }
    const char* getMaker() const override { return "Tinplate"; }
    const char* getLicense() const override { return "GPL-3.0-or-later"; }
    uint32_t getVersion() const override { return d_version(1, 4, 0); }

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
        case kParamSize:      set(p, "Size", "size", "%"); break;
        case kParamModDepth:  set(p, "Modulation depth", "depth", "%"); break;
        case kParamModRate:
            set(p, "Modulation rate", "rate", "Hz");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamWet:       set(p, "Reverb level", "wet", "dB"); break;
        case kParamDry:       set(p, "Dry level", "dry", "dB"); break;
        case kParamLink:
            set(p, "Dry / reverb link", "link", "");
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            break;
        case kParamMix:       set(p, "Mix (linked)", "mix", ""); break;
        case kParamPredelay:  set(p, "Pre-delay", "predelay", "ms"); break;
        case kParamLowCut:
            set(p, "Low cut", "lowcut", "Hz");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamHighCut:
            set(p, "High cut", "highcut", "Hz");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamWidth:     set(p, "Width", "width", "%"); break;
        case kParamOutput:    set(p, "Output", "output", "dB"); break;
        case kParamSplit:     set(p, "Plate split (A to B)", "split", "%"); break;
        case kParamBandwidth2: set(p, "Bandwidth B", "bandwidth_b", ""); break;
        case kParamTail2:     set(p, "Tail B", "tail_b", ""); break;
        case kParamDamping2:  set(p, "Damping B", "damping_b", ""); break;
        case kParamSize2:     set(p, "Size B", "size_b", "%"); break;
        case kParamSmooth:    set(p, "Smooth", "smooth", "%"); break;
        case kParamFreeze:
            set(p, "Freeze", "freeze", "");
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            break;
        case kParamMeterIn:  output(p, "Input level", "meter_in", "dB"); break;
        case kParamMeterWet: output(p, "Reverb level", "meter_wet", "dB"); break;
        case kParamMeterOut: output(p, "Output level", "meter_out", "dB"); break;
        case kParamDuck:      set(p, "Duck", "duck", "dB"); break;
        case kParamThreshold: set(p, "Duck threshold", "threshold", "dB"); break;
        case kParamAttack:
            set(p, "Duck attack", "attack", "ms");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamRelease:
            set(p, "Duck release", "release", "ms");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamClear:
            set(p, "Clear", "clear", "");
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            break;
        case kParamFloor:   set(p, "Clear floor", "floor", "dB"); break;
        case kParamTrigger: set(p, "Clear trigger", "trigger", "dB"); break;
        case kParamFade:
            set(p, "Clear fade", "fade", "ms");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamWindow:
            set(p, "Clear window", "window", "ms");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamStereoIn:
            set(p, "Stereo input", "stereo", "");
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            break;
        case kParamMonoBass:
            set(p, "Mono bass", "monobass", "Hz");
            p.hints |= kParameterIsLogarithmic;
            break;
        case kParamSync:
            set(p, "Pre-delay sync", "sync", "");
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            break;
        case kParamNote:
            set(p, "Pre-delay note", "note", "");
            p.hints |= kParameterIsInteger;
            p.enumValues.count = TinplateNote::kCount;
            p.enumValues.restrictedMode = true;
            {
                ParameterEnumerationValue* const v = new ParameterEnumerationValue[TinplateNote::kCount];
                for (int i = 0; i < TinplateNote::kCount; ++i)
                {
                    v[i].label = TinplateNote::kNames[i];
                    v[i].value = static_cast<float>(i);
                }
                p.enumValues.values = v;
            }
            break;
        case kParamHistIn:     output(p, "History input", "hist_in", ""); break;
        case kParamHistWet:    output(p, "History reverb", "hist_wet", ""); break;
        case kParamHistCut:    output(p, "History cut", "hist_cut", "dB"); break;
        case kParamHistClears: output(p, "History clearings", "hist_clears", ""); break;
        case kParamMeterClear:
            output(p, "Clear state", "meter_clear", "");
            p.ranges.min = 0.0f;
            p.ranges.max = 2.0f;
            p.ranges.def = 0.0f;
            break;
        case kParamMeterDuck:
            output(p, "Duck cut", "meter_duck", "dB");
            p.ranges.min = 0.0f;
            p.ranges.max = 30.0f;
            p.ranges.def = 0.0f;
            break;
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
        state.label = kStateLabels[index];
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
        if (index >= static_cast<uint32_t>(kParamFirstOutput))
            return;
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
        const TimePosition& tp = getTimePosition();
        fBpm = tp.bbt.valid && tp.bbt.beatsPerMinute > 1.0 ? tp.bbt.beatsPerMinute : TinplateNote::kDefaultBpm;
        setTargets(fRamp);

        const float* const inL = inputs[0];
        const float* const inR = inputs[1];
        float* const outL = outputs[0];
        float* const outR = outputs[1];
        float peakIn = 0.0f, peakWet = 0.0f, peakOut = 0.0f, peakDuck = 0.0f;
        const bool monoBass = !fMonoBass.identity();   // on, or its state not yet empty

        for (uint32_t i = 0; i < frames; ++i)
        {
            const float sl = inL[i], sr = inR[i];
            fNormal = -fNormal;
            float dl, dr;
            preDelay(sl, sr, dl, dr);
            float x = (dl + dr + fNormal) * .5;
            const float inGain = fInGain.next();
            x = x * inGain;
            // stereo input: t = 1, each channel into one half of the plate (t ramps on a change)
            const bool stereo = fStereo.moving() || fStereo.cur != 0.0f;
            float xa = x, xb = 0.0f, t = 0.0f;
            if (stereo)
            {
                t = fStereo.next();
                const float l = (dl + fNormal) * inGain;
                xa = t == 1.0f ? l : x + (l - x) * t;
                xb = (dr + fNormal) * inGain;
            }

            if (fModulation.moving())
            {
                const float v = fModulation.next();
                for (PlateLine& line : fLines)
                {
                    line.plate[0].setModulation(v);
                    line.plate[1].setModulation(v);
                }
            }
            if (fSmooth.moving())
            {
                const float v = fSmooth.next();
                for (PlateLine& line : fLines)
                {
                    line.plate[0].setSmooth(v);
                    line.plate[1].setSmooth(v);
                }
            }

            if (fClearOn || fArmed)
                detectAttack(std::fmax(std::fabs(sl), std::fabs(sr)));

            // the two plates in parallel, each fed its share of the input
            const bool fading = fFadeLeft > 0;
            const float fade = fading ? 0.5f + 0.5f * std::cos(static_cast<float>(M_PI)
                                                               * (1.0f - static_cast<float>(fFadeLeft) / fFadeLength))
                                      : 0.0f;
            float wl = 0.0f, wr = 0.0f;
            if (fLines[0].running)
                runLine(fLines[0], stereo, x, xa, xb, t, fading, fade, wl, wr);
            if (fLines[1].running)
            {
                float pl, pr;
                runLine(fLines[1], stereo, x, xa, xb, t, fading, fade, pl, pr);
                wl += pl;
                wr += pr;
            }
            if (fading && --fFadeLeft == 0)
                for (PlateLine& line : fLines)
                    line.plate[1 - fCur].activate();
            if (stereo && t == 0.0f)
            {
                // back to the mono input: the second channel is emptied, then no longer computed
                for (PlateLine& line : fLines)
                {
                    line.plate[0].resetInput2();
                    line.plate[1].resetInput2();
                }
            }
            if (fFlash > 0)
                --fFlash;

            if (!fLowCut.identity())
                fLowCut.process(wl, wr);
            if (!fHighCut.identity())
                fHighCut.process(wl, wr);
            if (fWidth.moving() || fWidth.cur != 1.0f || monoBass)
            {
                // mono bass: the side signal high-passed (the mid untouched), then the width
                const float w = fWidth.next();
                const float m = (wl + wr) * 0.5f;
                float s = (wl - wr) * 0.5f;
                if (monoBass)
                    fMonoBass.process1(s);
                s = s * w;
                wl = m + s;
                wr = m - s;
            }
            if (fDuckDepth > 0.0f || fDuckDb > 0.0f)
            {
                const float g = duckGain(std::fmax(std::fabs(sl), std::fabs(sr)));
                wl *= g;
                wr *= g;
                peakDuck = std::fmax(peakDuck, fDuckDb);
            }
            peakWet = std::fmax(peakWet, std::fmax(std::fabs(wl), std::fabs(wr)));

            const float wet = fWet.next();
            const float dry = fDry.next();
            const float g = fGain.next();
            float yl = (wet * wl + dry * sl) * g;
            float yr = (wet * wr + dry * sr) * g;

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

        // a plate whose send is 0 stops once its tail has died away
        const int quietLength = static_cast<int>(0.05 * getSampleRate());
        for (PlateLine& line : fLines)
            if (line.running && line.send.cur == 0.0f && !line.send.moving() && line.quiet >= quietLength)
            {
                line.plate[0].activate();
                line.plate[1].activate();
                line.running = false;
                line.quiet = 0;
            }

        const float fall = std::exp(-static_cast<float>(frames) / (0.25f * static_cast<float>(getSampleRate())));
        fMeterIn = std::fmax(peakIn, fMeterIn * fall);
        fMeterWet = std::fmax(peakWet, fMeterWet * fall);
        fMeterOut = std::fmax(peakOut, fMeterOut * fall);
        fParams[kParamMeterIn] = gainToDb(fMeterIn, -60.0f);
        fParams[kParamMeterWet] = gainToDb(fMeterWet, -60.0f);
        fParams[kParamMeterOut] = gainToDb(fMeterOut, -60.0f);
        fParams[kParamMeterDuck] = peakDuck;
        fParams[kParamMeterClear] = fFlash > 0 ? 2.0f : fArmed ? 1.0f : 0.0f;

        // history: peaks of the last ~30 ms, count of the clearings
        const unsigned bin = static_cast<unsigned>(0.005 * getSampleRate()) + 1;
        fRecentIn.add(peakIn, frames, bin);
        fRecentWet.add(peakWet, frames, bin);
        fRecentCut.add(peakDuck, frames, bin);
        fParams[kParamHistIn] = std::fmin(fRecentIn.value(), 2.0f);
        fParams[kParamHistWet] = std::fmin(fRecentWet.value(), 2.0f);
        fParams[kParamHistCut] = fRecentCut.value();
        fParams[kParamHistClears] = static_cast<float>(fClears % 1000u);
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
        for (int k = 0; k < 2; ++k)
            for (PlateCore::Plate& p : fLines[k].plate)
            {
                p.init(static_cast<float>(sr));
                p.setPhase(k == 0 ? 0.0 : 0.25 * M_PI);   // plate B: modulation 45 degrees later
            }
        const unsigned n = PlateCore::nextPowerOf2(static_cast<unsigned>(0.001 * kPredelayMaxMs * sr) + 2);
        fPre[0].assign(n, 0.0f);
        fPre[1].assign(n, 0.0f);
        fPreMask = n - 1;
    }

    // targets of the ramps from the parameters; ramp = false: taken at once
    void setTargets(const bool ramp)
    {
        const double sr = getSampleRate();
        const int n = ramp ? static_cast<int>(0.02 * sr) : 0;
        const bool freeze = fParams[kParamFreeze] > 0.5f;

        fInGain.set(freeze ? 0.0f : 1.0f, n);
        fModulation.set(freeze ? 0.0f : fParams[kParamModDepth] / 100.0f, ramp ? static_cast<int>(0.2 * sr) : 0);
        // rate: 1.2 Hz exactly as the original (the float parameter is not exactly 1.2)
        const double rate = fParams[kParamModRate] == 1.2f ? 1.2 : static_cast<double>(fParams[kParamModRate]);
        for (PlateLine& line : fLines)
            for (PlateCore::Plate& p : line.plate)
                p.setModRate(rate);
        // smooth: the coefficients of the tanks in 20 ms; each tail compensated (Smooth.hpp)
        const float smooth = std::fmin(std::fmax(fParams[kParamSmooth], 0.0f), 100.0f) * 0.01f;
        fSmooth.set(smooth, n);
        // the two plates: sends (constant power; exactly 1 and 0 at the ends) and coefficients
        const float split = std::fmin(std::fmax(fParams[kParamSplit], 0.0f), 100.0f);
        const double angle = 0.5 * M_PI * 0.01 * split;
        const float sends[2] = { split <= 0.0f ? 1.0f : split >= 100.0f ? 0.0f : static_cast<float>(std::cos(angle)),
                                 split <= 0.0f ? 0.0f : split >= 100.0f ? 1.0f : static_cast<float>(std::sin(angle)) };
        static const int kPlateParams[2][4] = { { kParamBandwidth, kParamTail, kParamDamping, kParamSize },
                                                { kParamBandwidth2, kParamTail2, kParamDamping2, kParamSize2 } };
        for (int k = 0; k < 2; ++k)
        {
            PlateLine& line = fLines[k];
            const int* const q = kPlateParams[k];
            const float bw = static_cast<float>(TinplateLaw::bandwidthCoef(fParams[q[0]]));
            const float size = fParams[q[3]] / 100.0f;
            const float decay = freeze ? 1.0f
                                       : TinplateLaw::decay(TinplateSmooth::tail(fParams[q[1]], fParams[q[2]], size, smooth));
            const float damp = freeze ? 1.0f : static_cast<float>(TinplateLaw::dampingCoef(fParams[q[2]]));
            if (!ramp || (!line.running && sends[k] > 0.0f))
            {
                // reset, or a stopped (empty) plate starting again: its settings at once
                line.bandwidth.reset(bw);
                line.decay.reset(decay);
                line.damping.reset(damp);
                for (PlateCore::Plate& p : line.plate)
                {
                    p.setBandwidth(bw);
                    p.setDamping(damp);
                    p.setModulation(fModulation.cur);
                    p.setSmooth(fSmooth.cur);
                    p.setSize(size, 0);
                    p.finishGlide();
                }
                line.send.reset(ramp ? 0.0f : sends[k]);
                line.send.set(sends[k], n);
                line.running = sends[k] > 0.0f;
                line.quiet = 0;
            }
            else
            {
                line.bandwidth.set(bw, n);
                line.decay.set(decay, n);
                line.damping.set(damp, n);
                line.send.set(sends[k], n);
                // size: the lengths glide in 100 ms (pitch glide)
                for (PlateCore::Plate& p : line.plate)
                    p.setSize(size, static_cast<int>(0.1 * sr));
            }
        }
        if (fParams[kParamLink] > 0.5f)
        {
            // linked: the mix, as the blend of CAPS (same gains, to the bit)
            const float wet = TinplateLaw::wet(fParams[kParamMix]);
            fWet.set(wet, n);
            fDry.set(1 - wet, n);
        }
        else
        {
            fWet.set(TinplateLaw::levelGain(fParams[kParamWet]), n);
            fDry.set(TinplateLaw::levelGain(fParams[kParamDry]), n);
        }
        fWidth.set(fParams[kParamWidth] * 0.01f, n);
        fStereo.set(fParams[kParamStereoIn] > 0.5f ? 1.0f : 0.0f, n);
        fMonoBass.set(fParams[kParamMonoBass], sr, n);
        fGain.set(fParams[kParamOutput] == 0.0f ? 1.0f : dbToGain(fParams[kParamOutput]), n);
        fBypass.set(fParams[kParamBypass] > 0.5f ? 1.0f : 0.0f, ramp ? static_cast<int>(0.01 * sr) : 0);
        fLowCut.set(fParams[kParamLowCut], sr, n);
        fHighCut.set(fParams[kParamHighCut], sr, n);
        // ducking: no ramp, the cut is smoothed by the attack and release themselves
        fDuckDepth = fParams[kParamDuck];
        fDuckThresholdDb = fParams[kParamThreshold];
        fDuckThreshold = std::pow(10.0f, fDuckThresholdDb * 0.05f);
        fDuckAttack = std::exp(-1000.0 / (std::fmax(fParams[kParamAttack], 0.01f) * sr));
        fDuckRelease = std::exp(-1000.0 / (std::fmax(fParams[kParamRelease], 0.01f) * sr));
        // clear
        fClearOn = fParams[kParamClear] > 0.5f;
        fFloor = std::pow(10.0f, fParams[kParamFloor] * 0.05f);
        fTrigger = std::pow(10.0f, fParams[kParamTrigger] * 0.05f);
        fEnvRelease = static_cast<float>(std::exp(-1.0 / (0.03 * sr)));
        fRiseMax = std::max(1, static_cast<int>(1e-3 * fParams[kParamWindow] * sr));
        fFlashLength = static_cast<int>(0.2 * sr);

        // pre-delay: ms, or a note value at the tempo of the host
        const double preMs = fParams[kParamSync] > 0.5f ? TinplateNote::ms(fParams[kParamNote], fBpm)
                                                        : static_cast<double>(fParams[kParamPredelay]);
        const unsigned pd = static_cast<unsigned>(std::lround(preMs * 1e-3 * sr));
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
    }

    // one sample of one of the plates in parallel (the current one, and the old one fading out)
    inline void runLine(PlateLine& line, const bool stereo, const float x, const float xa, const float xb, const float t,
                        const bool fading, const float fade, float& pl, float& pr)
    {
        if (line.bandwidth.moving())
        {
            const float v = line.bandwidth.next();
            line.plate[0].setBandwidth(v);
            line.plate[1].setBandwidth(v);
        }
        if (line.damping.moving())
        {
            const float v = line.damping.next();
            line.plate[0].setDamping(v);
            line.plate[1].setDamping(v);
        }
        const float decay = line.decay.next();
        const float send = line.send.next();
        if (stereo)
            line.plate[fCur].processStereo(xa * send, xb * send, t, decay, &pl, &pr);
        else
            line.plate[fCur].process(x * send, decay, &pl, &pr);
        if (fading)
        {
            // the old plate: input closed, fading out, then emptied
            float ol, orr;
            if (stereo)
                line.plate[1 - fCur].processStereo(0.0f, 0.0f, t, decay, &ol, &orr);
            else
                line.plate[1 - fCur].process(0.0f, decay, &ol, &orr);
            pl += ol * fade;
            pr += orr * fade;
        }
        if (send == 0.0f)
            line.quiet = std::fmax(std::fabs(pl), std::fabs(pr)) < kStopLevel ? line.quiet + 1 : 0;
    }

    // gain of the reverb for an input peak; follows the cut fDuckDb
    float duckGain(const float peak)
    {
        const float target = peak > fDuckThreshold
                           ? TinplateDuck::targetDb(8.6858896f * std::log(peak), fDuckThresholdDb, fDuckDepth)
                           : 0.0f;
        const float a = target > fDuckDb ? fDuckAttack : fDuckRelease;
        fDuckDb = target + a * (fDuckDb - target);
        if (target == 0.0f && fDuckDb < 1e-3f)
            fDuckDb = 0.0f;
        return fDuckDb > 0.0f ? std::exp(-0.11512925f * fDuckDb) : 1.0f;
    }

    // clear: state of the detection for one input peak; switches the plates at an attack
    void detectAttack(const float peak)
    {
        fEnv = std::fmax(peak, fEnv * fEnvRelease);
        if (!fClearOn)
        {
            fArmed = false;
            return;
        }
        if (fEnv < fFloor)
        {
            if (fFadeLeft == 0)   // no re-arming while the old plate fades out
            {
                fArmed = true;
                fRise = 0;
            }
            return;
        }
        if (!fArmed)
            return;
        if (fEnv >= fTrigger)
        {
            fArmed = false;
            if (fParams[kParamFreeze] > 0.5f || fInGain.cur != 1.0f)
                return;           // frozen (or freezing): the frozen sound is kept
            fCur = 1 - fCur;
            for (PlateLine& line : fLines)
                line.plate[fCur].finishGlide();   // empty: its lengths go to the size at once
            fFadeLength = fFadeLeft = std::max(1, static_cast<int>(1e-3 * fParams[kParamFade] * getSampleRate()));
            fFlash = fFlashLength;
            ++fClears;
        }
        else if (++fRise > fRiseMax)
            fArmed = false;       // too slow for an attack
    }

    // both channels delayed (pre-delay 0: the same samples)
    void preDelay(const float l, const float r, float& dl, float& dr)
    {
        fPre[0][fPrePos] = l;
        fPre[1][fPrePos] = r;
        dl = fPre[0][(fPrePos - fPreCur) & fPreMask];
        dr = fPre[1][(fPrePos - fPreCur) & fPreMask];
        if (fPreFade > 0)
        {
            const float t = 1.0f - static_cast<float>(fPreFade) / fPreFadeLength;
            dl += (fPre[0][(fPrePos - fPreNext) & fPreMask] - dl) * t;
            dr += (fPre[1][(fPrePos - fPreNext) & fPreMask] - dr) * t;
            if (--fPreFade == 0)
                fPreCur = fPreNext;
        }
        fPrePos = (fPrePos + 1) & fPreMask;
    }

    void resetState()
    {
        for (PlateLine& line : fLines)
        {
            line.plate[0].activate();
            line.plate[1].activate();
        }
        fCur = 0;
        fFadeLeft = fFlash = fRise = 0;
        fArmed = false;
        fEnv = 0.0f;
        fInGain.reset(0.0f);
        fModulation.reset(0.0f);
        fSmooth.reset(0.0f);
        fWet.reset(0.0f);
        fDry.reset(0.0f);
        fWidth.reset(0.0f);
        fStereo.reset(0.0f);
        fGain.reset(0.0f);
        fBypass.reset(0.0f);
        fLowCut.reset(fParams[kParamLowCut], getSampleRate());
        fHighCut.reset(fParams[kParamHighCut], getSampleRate());
        fMonoBass.reset(fParams[kParamMonoBass], getSampleRate());
        std::fill(fPre[0].begin(), fPre[0].end(), 0.0f);
        std::fill(fPre[1].begin(), fPre[1].end(), 0.0f);
        fRecentIn.reset();
        fRecentWet.reset();
        fRecentCut.reset();
        fPrePos = 0;
        fRamp = false;
        setTargets(false);
        fRamp = true;
        fMeterIn = fMeterWet = fMeterOut = 0.0f;
        fDuckDb = 0.0f;
    }

    float fParams[kParamCount];
    String fStates[kStateCount];   // strings of the interface, only kept for the host

    PlateLine fLines[2];           // plates A and B in parallel; clear: plate[fCur] of each is current
    float fNormal = 1e-20f;        // NOISE_FLOOR of CAPS
    bool fRamp = false;
    Ramp<float> fInGain, fModulation, fSmooth, fWet, fDry, fWidth, fStereo, fGain, fBypass;
    Cut2 fLowCut, fHighCut, fMonoBass;   // mono bass: high-pass of the side signal
    std::vector<float> fPre[2];          // pre-delay of the left and right channels
    double fBpm = TinplateNote::kDefaultBpm;
    RecentPeak fRecentIn, fRecentWet, fRecentCut;
    unsigned fClears = 0;
    unsigned fPreMask = 0, fPrePos = 0, fPreCur = 0, fPreNext = 0;
    int fPreFade = 0, fPreFadeLength = 1;
    float fMeterIn = 0.0f, fMeterWet = 0.0f, fMeterOut = 0.0f;
    float fDuckDepth = 0.0f, fDuckThresholdDb = 0.0f, fDuckThreshold = 1.0f, fDuckDb = 0.0f;
    float fDuckAttack = 0.0f, fDuckRelease = 0.0f;   // one-pole coefficients
    bool fClearOn = false, fArmed = false;
    int fCur = 0, fFadeLeft = 0, fFadeLength = 1, fFlash = 0, fFlashLength = 1, fRise = 0, fRiseMax = 1;
    float fFloor = 0.0f, fTrigger = 1.0f, fEnv = 0.0f, fEnvRelease = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TinplatePlugin)
};

Plugin* createPlugin()
{
    return new TinplatePlugin();
}

END_NAMESPACE_DISTRHO
