/*
 * Tinplate Reverb - parameters, presets and the control laws of the original, shared by the DSP,
 * the UI and the tests.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TINPLATE_PARAMS_HPP_INCLUDED
#define TINPLATE_PARAMS_HPP_INCLUDED

#include <cmath>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

enum TinplateParam {
    // inputs: the four controls of CAPS PlateX2 (same 0..1 ranges and laws), then the additions
    kParamBandwidth = 0,  // input low-pass of the tank (CAPS "bandwidth")
    kParamTail,           // decay of the tank (CAPS "tail"; up to 1.16 here, RT60 10 s at 500 Hz)
    kParamDamping,        // low-pass inside the tank (CAPS "damping")
    kParamBlend,          // dry / wet (CAPS "blend": wet = blend^1.53, dry = 1 - wet)
    kParamPredelay,       // delay before the tank, ms
    kParamLowCut,         // high-pass on the wet signal, Hz (minimum = off)
    kParamWidth,          // stereo width of the wet signal, %
    kParamOutput,         // output gain, dB
    kParamFreeze,         // infinite tail: input of the tank closed, no loss in the tank (0/1)
    kParamBypass,         // host bypass
    // added in 1.1.0, after the parameters of 1.0.0 (host automation is by index)
    kParamDuck,           // ducking depth: largest cut of the reverb while the input plays, dB (0 = off)
    kParamThreshold,      // ducking threshold on the input peak, dBFS
    kParamAttack,         // ducking attack, ms
    kParamRelease,        // ducking release, ms
    // outputs
    kParamMeterIn,        // input peak, dBFS
    kParamMeterWet,       // wet (reverb) peak, dBFS
    kParamMeterOut,       // output peak, dBFS
    kParamMeterDuck,      // cut of the reverb by the ducking, dB (positive)
    kParamCount
};

static const int kParamFirstOutput = kParamMeterIn;

struct TinplateRange { float min, max, def; };

// defaults of the four CAPS controls: those a LADSPA host gives PlateX2 (DEFAULT_MID, MID, HIGH, LOW)
static const TinplateRange kTinplateRanges[kParamCount] = {
    {   0.0f,    1.0f,   0.5f },    // bandwidth
    {   0.0f,   1.16f,   0.5f },    // tail (CAPS: 0..1; same law beyond, RT60 10 s at 500 Hz)
    {   0.0f,    1.0f,  0.75f },    // damping
    {   0.0f,    1.0f,  0.25f },    // blend
    {   0.0f,  250.0f,   0.0f },    // pre-delay, ms
    {  20.0f, 1000.0f,  20.0f },    // low cut, Hz (20 = off)
    {   0.0f,  200.0f, 100.0f },    // width, %
    { -24.0f,   12.0f,   0.0f },    // output, dB
    {   0.0f,    1.0f,   0.0f },    // freeze
    {   0.0f,    1.0f,   0.0f },    // bypass
    {   0.0f,   30.0f,   0.0f },    // duck, dB (0 = off)
    { -60.0f,    0.0f, -30.0f },    // threshold, dBFS
    {   1.0f,  200.0f,  10.0f },    // attack, ms
    {  50.0f, 3000.0f, 300.0f },    // release, ms
    { -60.0f,    6.0f, -60.0f },    // meter in
    { -60.0f,    6.0f, -60.0f },    // meter wet
    { -60.0f,    6.0f, -60.0f },    // meter out
    {   0.0f,   30.0f,   0.0f },    // meter duck, dB
};

static const float kLowCutOff = 20.0f;   // low cut at its minimum: no filter

// ------------------------------------------------------------------------------------------------
// Factory presets: values of the parameters kPresetParams (freeze and bypass excluded).  The
// first eight are those of 1.0.0 (A/B states of 1.0.0 hold only them).

static const int kPresetParams[] = {
    kParamBandwidth, kParamTail, kParamDamping, kParamBlend, kParamPredelay, kParamLowCut, kParamWidth,
    kParamOutput, kParamDuck, kParamThreshold, kParamAttack, kParamRelease,
};
static const int kPresetParamCount = sizeof(kPresetParams) / sizeof(kPresetParams[0]);
static const int kPresetParamCountV1 = 8;

struct TinplatePreset {
    const char* name;
    float values[kPresetParamCount];
};

static const TinplatePreset kTinplatePresets[] = {
    //                        bandw  tail  damp  blend  pre   lowcut width  out   duck  thresh  att  rel
    { "Default",            { 0.80f, 0.55f, 0.40f, 0.30f, 12.0f,  60.0f, 100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Original (CAPS)",    { 0.50f, 0.50f, 0.75f, 0.25f,  0.0f,  20.0f, 100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Vocal plate",        { 0.85f, 0.45f, 0.45f, 0.25f, 35.0f, 150.0f, 110.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Drum plate",         { 0.90f, 0.30f, 0.30f, 0.22f,  5.0f, 200.0f, 120.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Ambience",           { 0.70f, 0.15f, 0.50f, 0.20f,  0.0f, 100.0f, 130.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Dark and long",      { 0.55f, 0.85f, 0.75f, 0.35f, 20.0f,  60.0f, 100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Bright and wide",    { 1.00f, 0.70f, 0.10f, 0.30f, 15.0f, 120.0f, 150.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Send (100 % wet)",   { 0.80f, 0.60f, 0.40f, 1.00f, 20.0f, 100.0f, 100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f } },
    { "Ducked vocal",       { 0.85f, 0.65f, 0.45f, 0.35f, 25.0f, 150.0f, 110.0f, 0.0f, 12.0f, -30.0f, 10.0f, 400.0f } },
};

static const int kTinplatePresetCount = sizeof(kTinplatePresets) / sizeof(kTinplatePresets[0]);

// States saved with the project, written by the UI only (the DSP ignores them)
enum { kStateAB = 0, kStateCount };
static const char* const kStateKeys[kStateCount] = { "ab" };

// ------------------------------------------------------------------------------------------------
// Ducking: the cut of the reverb, dB, for an input peak of a given level (dBFS): as many dB as
// the input goes over the threshold, at most the depth.  The attack and release smooth it.
namespace TinplateDuck {

static inline float targetDb(const float levelDb, const float thresholdDb, const float depthDb)
{
    return std::fmin(std::fmax(levelDb - thresholdDb, 0.0f), depthDb);
}

}   // namespace TinplateDuck

// ------------------------------------------------------------------------------------------------
// Control laws of CAPS PlateX2::cycle(), with its types (float controls, double expressions):
// the DSP must give the same coefficients to the last bit.

namespace TinplateLaw {

// input low-pass coefficient a0 (y = a0 x + (1 - a0) y1)
static inline double bandwidthCoef(const float p)
{
    const float bw = .005 + .994 * p;
    return std::exp(-M_PI * (1. - bw));
}

// feedback gain of each half of the tank
static inline float decay(const float p)
{
    return .749 * p;
}

// damping low-pass coefficient a0 inside the tank
static inline double dampingCoef(const float p)
{
    return std::exp(-M_PI * (.0005 + .9995 * p));
}

// wet gain; the dry gain is 1 - wet
static inline float wet(const float p)
{
    return static_cast<float>(std::pow(p, 1.53));
}

// gain of a one-pole low-pass y = a x + (1 - a) y1 at frequency f
static inline double onePoleGain(const double a, const double f, const double fs)
{
    const double p = 1.0 - a, w = 2.0 * M_PI * f / fs;
    return a / std::sqrt(1.0 - 2.0 * p * std::cos(w) + p * p);
}

// -3 dB frequency of that low-pass; 0 when it stays above -3 dB up to Nyquist
static inline double onePoleCutoff(const double a, const double fs)
{
    const double p = 1.0 - a;
    if (p <= 1e-12)
        return 0.0;
    const double c = (1.0 + p * p - 2.0 * a * a) / (2.0 * p);
    if (c <= -1.0)
        return 0.0;
    return std::acos(std::fmin(c, 1.0)) * fs / (2.0 * M_PI);
}

}   // namespace TinplateLaw

#endif
