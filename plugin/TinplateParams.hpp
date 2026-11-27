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
    // inputs: the three plate controls of CAPS PlateX2 (same laws), then the additions
    kParamBandwidth = 0,  // input low-pass of the tank (CAPS "bandwidth")
    kParamTail,           // decay of the tank (CAPS "tail"; up to 1.16 here, RT60 10 s at 500 Hz)
    kParamDamping,        // low-pass inside the tank (CAPS "damping")
    kParamSize,           // size of the tank: lengths of its delays, % (100 = CAPS)
    kParamModDepth,       // depth of the modulation of the tank, % (100 = CAPS)
    kParamModRate,        // rate of the modulation of the tank, Hz (1.2 = CAPS)
    kParamWet,            // level of the reverb, dB (-60 = off); CAPS has a blend instead
    kParamDry,            // level of the dry signal, dB (-60 = off)
    kParamLink,           // dry and wet linked: the mix below sets both, as the blend of CAPS (0/1)
    kParamMix,            // linked mix (blend of CAPS): wet gain mix^1.53, dry gain 1 - that
    kParamPredelay,       // delay before the tank, ms
    kParamLowCut,         // high-pass on the wet signal, Hz (minimum = off)
    kParamHighCut,        // low-pass on the wet signal, Hz (maximum = off)
    kParamWidth,          // stereo width of the wet signal, %
    kParamOutput,         // output gain, dB
    kParamDuck,           // ducking depth: largest cut of the reverb while the input plays, dB (0 = off)
    kParamThreshold,      // ducking threshold on the input peak, dBFS
    kParamAttack,         // ducking attack, ms
    kParamRelease,        // ducking release, ms
    kParamClear,          // a new attack empties the plate (0/1)
    kParamFloor,          // clear: the input under this peak level arms the detection, dBFS
    kParamTrigger,        // clear: the input over this level soon after triggers, dBFS
    kParamFade,           // clear: fade out of the old plate, ms
    kParamWindow,         // clear: longest rise from Floor to Trigger of an attack, ms
    kParamStereoIn,       // option: L and R each feed one half of the plate (0: their mono sum, the original)
    kParamMonoBass,       // option: the reverb is mono below this frequency, Hz (minimum = off)
    kParamSync,           // pre-delay locked to the tempo of the host (0/1)
    kParamNote,           // pre-delay when locked: note value, index into TinplateNote (0..12)
    kParamSplit,          // share of the input sent to plate B, % (0: plate A only, the original;
                          // constant power: plate A gets cos, plate B sin of split x 90 degrees)
    kParamBandwidth2,     // plate B: bandwidth, same law as plate A
    kParamTail2,          // plate B: tail
    kParamDamping2,       // plate B: damping
    kParamSize2,          // plate B: size, %
    kParamFreeze,         // infinite tail: input of the tank closed, no loss in the tank (0/1)
    kParamBypass,         // host bypass
    // outputs
    kParamMeterIn,        // input peak, dBFS
    kParamMeterWet,       // wet (reverb) peak, dBFS
    kParamMeterOut,       // output peak, dBFS
    kParamMeterDuck,      // cut of the reverb by the ducking, dB (positive)
    kParamMeterClear,     // clear: 0 off or waiting, 1 armed, 2 just cleared (held 200 ms)
    kParamHistIn,         // history: input peak over the last ~30 ms, linear
    kParamHistWet,        // history: reverb peak (after the ducking) over the last ~30 ms, linear
    kParamHistCut,        // history: largest cut of the ducking over the last ~30 ms, dB
    kParamHistClears,     // history: count of the clearings, modulo 1000
    kParamCount
};

static const int kParamFirstOutput = kParamMeterIn;

struct TinplateRange { float min, max, def; };

// defaults of the four CAPS controls: those a LADSPA host gives PlateX2 (DEFAULT_MID, MID, HIGH, LOW)
static const TinplateRange kTinplateRanges[kParamCount] = {
    {   0.0f,    1.0f,   0.5f },    // bandwidth
    {   0.0f,   1.16f,   0.5f },    // tail (CAPS: 0..1; same law beyond, RT60 10 s at 500 Hz)
    {   0.0f,    1.0f,  0.75f },    // damping
    {  50.0f,  200.0f, 100.0f },    // size, %
    {   0.0f,  200.0f, 100.0f },    // modulation depth, %
    {   0.1f,    5.0f,   1.2f },    // modulation rate, Hz
    { -60.0f,    6.0f, -18.0f },    // wet, dB (-60 = off)
    { -60.0f,    6.0f,   0.0f },    // dry, dB (-60 = off)
    {   0.0f,    1.0f,   0.0f },    // link
    {   0.0f,    1.0f,  0.25f },    // mix (blend of CAPS, its default)
    {   0.0f,  250.0f,   0.0f },    // pre-delay, ms
    {  20.0f, 1000.0f,  20.0f },    // low cut, Hz (20 = off)
    { 1000.0f, 20000.0f, 20000.0f }, // high cut, Hz (20000 = off)
    {   0.0f,  200.0f, 100.0f },    // width, %
    { -24.0f,   12.0f,   0.0f },    // output, dB
    {   0.0f,   30.0f,   0.0f },    // duck, dB (0 = off)
    { -60.0f,    0.0f, -30.0f },    // threshold, dBFS
    {   1.0f,  200.0f,  10.0f },    // attack, ms
    {  50.0f, 3000.0f, 300.0f },    // release, ms
    {   0.0f,    1.0f,   0.0f },    // clear
    { -80.0f,  -20.0f, -50.0f },    // floor, dBFS
    { -60.0f,    0.0f, -20.0f },    // trigger, dBFS
    {  10.0f,  500.0f,  60.0f },    // fade, ms
    {   1.0f,  200.0f,  20.0f },    // window, ms
    {   0.0f,    1.0f,   0.0f },    // stereo input
    {  20.0f,  500.0f,  20.0f },    // mono bass, Hz (20 = off)
    {   0.0f,    1.0f,   0.0f },    // pre-delay sync
    {   0.0f,   12.0f,   6.0f },    // pre-delay note (1/16)
    {   0.0f,  100.0f,   0.0f },    // split, % (0 = plate A only)
    {   0.0f,    1.0f,   0.5f },    // plate B: bandwidth
    {   0.0f,   1.16f,   0.5f },    // plate B: tail
    {   0.0f,    1.0f,  0.75f },    // plate B: damping
    {  50.0f,  200.0f, 150.0f },    // plate B: size, % (larger than plate A by default)
    {   0.0f,    1.0f,   0.0f },    // freeze
    {   0.0f,    1.0f,   0.0f },    // bypass
    { -60.0f,    6.0f, -60.0f },    // meter in
    { -60.0f,    6.0f, -60.0f },    // meter wet
    { -60.0f,    6.0f, -60.0f },    // meter out
    {   0.0f,   30.0f,   0.0f },    // meter duck, dB
    {   0.0f,    2.0f,   0.0f },    // meter clear
    {   0.0f,    2.0f,   0.0f },    // history in
    {   0.0f,    2.0f,   0.0f },    // history wet
    {   0.0f,   30.0f,   0.0f },    // history cut, dB
    {   0.0f, 1000.0f,   0.0f },    // history clearings
};

static const float kLowCutOff = 20.0f;   // low cut at its minimum: no filter
static const float kHighCutOff = 20000.0f;   // high cut at its maximum: no filter
static const float kLevelOff = -60.0f;   // dry and wet faders at their minimum: gain 0
static const float kMonoBassOff = 20.0f;   // mono bass at its minimum: no filter
static const float kPredelayMaxMs = 2000.0f;   // longest pre-delay, locked to the tempo (1/4 at 30 BPM)

// ------------------------------------------------------------------------------------------------
// Factory presets: values of the parameters kParamBandwidth .. kParamSize2 (freeze and bypass
// excluded).

static const int kPresetParamCount = kParamFreeze;

struct TinplatePreset {
    const char* name;
    float values[kPresetParamCount];
};

static const TinplatePreset kTinplatePresets[] = {
    //                      bandw  tail  damp   size   depth  rate  wet   dry   link  mix   pre   lowcut hicut  width  out
    //                      duck  thresh  att  rel  clear  floor  trigger  fade  window
    //                      stereo  monobass  sync  note
    //                      split  bandwB  tailB  dampB  sizeB (plate B: plate A with a size x 1.5, split 0)
    { "Default",          { 0.80f, 0.55f, 0.40f, 100.0f, 100.0f, 1.2f, -16.0f, -1.5f, 0.0f, 0.30f, 12.0f, 60.0f, 20000.0f,
                            100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.80f, 0.55f, 0.40f, 150.0f } },
    { "Original (CAPS)",  { 0.50f, 0.50f, 0.75f, 100.0f, 100.0f, 1.2f, -18.4f, -1.1f, 0.0f, 0.25f, 0.0f, 20.0f, 20000.0f,
                            100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.50f, 0.50f, 0.75f, 150.0f } },
    { "Vocal plate",      { 0.85f, 0.45f, 0.45f, 100.0f, 100.0f, 1.2f, -18.4f, -1.1f, 0.0f, 0.25f, 35.0f, 150.0f, 20000.0f,
                            110.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.85f, 0.45f, 0.45f, 150.0f } },
    { "Drum plate",       { 0.90f, 0.30f, 0.30f, 100.0f, 100.0f, 1.2f, -20.1f, -0.9f, 0.0f, 0.22f, 5.0f, 200.0f, 20000.0f,
                            120.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.90f, 0.30f, 0.30f, 150.0f } },
    { "Ambience",         { 0.70f, 0.15f, 0.50f, 100.0f, 100.0f, 1.2f, -21.4f, -0.8f, 0.0f, 0.20f, 0.0f, 100.0f, 20000.0f,
                            130.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.70f, 0.15f, 0.50f, 150.0f } },
    { "Dark and long",    { 0.55f, 0.85f, 0.75f, 100.0f, 100.0f, 1.2f, -14.0f, -1.9f, 0.0f, 0.35f, 20.0f, 60.0f, 20000.0f,
                            100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.55f, 0.85f, 0.75f, 150.0f } },
    { "Bright and wide",  { 1.00f, 0.70f, 0.10f, 100.0f, 100.0f, 1.2f, -16.0f, -1.5f, 0.0f, 0.30f, 15.0f, 120.0f, 20000.0f,
                            150.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 1.00f, 0.70f, 0.10f, 150.0f } },
    { "Send (100 % wet)", { 0.80f, 0.60f, 0.40f, 100.0f, 100.0f, 1.2f, 0.0f, -60.0f, 0.0f, 1.00f, 20.0f, 100.0f, 20000.0f,
                            100.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.80f, 0.60f, 0.40f, 150.0f } },
    { "Ducked vocal",     { 0.85f, 0.65f, 0.45f, 100.0f, 100.0f, 1.2f, -14.0f, -1.9f, 0.0f, 0.35f, 25.0f, 150.0f, 20000.0f,
                            110.0f, 0.0f, 12.0f, -30.0f, 10.0f, 400.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.85f, 0.65f, 0.45f, 150.0f } },
    { "Piano (clear)",    { 0.80f, 0.70f, 0.45f, 120.0f, 100.0f, 1.2f, -16.0f, -1.5f, 1.0f, 0.30f, 15.0f, 80.0f, 20000.0f,
                            120.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 1.0f, -50.0f, -20.0f, 120.0f, 40.0f,
                            1.0f, 20.0f, 0.0f, 6.0f,
                            0.0f, 0.80f, 0.70f, 0.45f, 180.0f } },
    { "Snare (clear)",    { 0.95f, 0.50f, 0.25f, 90.0f, 100.0f, 1.2f, -18.4f, -1.1f, 1.0f, 0.25f, 0.0f, 200.0f, 12000.0f,
                            130.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 1.0f, -45.0f, -15.0f, 40.0f, 10.0f,
                            0.0f, 20.0f, 1.0f, 3.0f,
                            0.0f, 0.95f, 0.50f, 0.25f, 135.0f } },
    { "Guitar (stereo)",  { 0.80f, 0.60f, 0.40f, 110.0f, 120.0f, 0.8f, -16.9f, -1.3f, 1.0f, 0.28f, 20.0f, 120.0f, 20000.0f,
                            140.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            1.0f, 150.0f, 0.0f, 6.0f,
                            0.0f, 0.80f, 0.60f, 0.40f, 165.0f } },
    { "Two plates",       { 0.90f, 0.35f, 0.25f, 70.0f, 100.0f, 1.2f, -16.0f, -1.5f, 0.0f, 0.30f, 15.0f, 100.0f, 20000.0f,
                            120.0f, 0.0f, 0.0f, -30.0f, 10.0f, 300.0f, 0.0f, -50.0f, -20.0f, 60.0f, 20.0f,
                            0.0f, 20.0f, 0.0f, 6.0f,
                            50.0f, 0.60f, 0.75f, 0.65f, 170.0f } },
};

static const int kTinplatePresetCount = sizeof(kTinplatePresets) / sizeof(kTinplatePresets[0]);

// ------------------------------------------------------------------------------------------------
// Pre-delay locked to the tempo: note values from 1/64 to 1/4, dotted and triplet, by length

namespace TinplateNote {

static const int kCount = 13;
static const char* const kNames[kCount] = {
    "1/64", "1/32 T", "1/64 D", "1/32", "1/16 T", "1/32 D", "1/16", "1/8 T", "1/16 D", "1/8", "1/4 T", "1/8 D", "1/4"
};
// length in quarter notes (beats)
static const double kBeats[kCount] = {
    1.0 / 16, 1.0 / 12, 3.0 / 32, 1.0 / 8, 1.0 / 6, 3.0 / 16, 1.0 / 4, 1.0 / 3, 3.0 / 8, 1.0 / 2, 2.0 / 3, 3.0 / 4, 1.0
};
static const double kDefaultBpm = 120.0;   // when the host gives no tempo

static inline int index(const float v)
{
    const int i = static_cast<int>(std::lround(v));
    return i < 0 ? 0 : i >= kCount ? kCount - 1 : i;
}

// length of a note value at a tempo, ms, at most kPredelayMaxMs
static inline double ms(const float note, const double bpm)
{
    const double b = bpm > 1.0 ? bpm : kDefaultBpm;
    return std::fmin(kBeats[index(note)] * 60000.0 / b, kPredelayMaxMs);
}

}   // namespace TinplateNote

// States saved with the project, written by the UI only (the DSP ignores them)
enum { kStateAB = 0, kStateCount };
static const char* const kStateKeys[kStateCount] = { "ab" };
static const char* const kStateLabels[kStateCount] = { "A/B comparison" };

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

// wet gain of CAPS; the dry gain is 1 - wet
static inline float wet(const float p)
{
    return static_cast<float>(std::pow(p, 1.53));
}

// level in dB of a gain, kLevelOff for 0 (dry and wet faders)
static inline float levelDb(const float g)
{
    return g > 0.0f ? std::fmax(20.0f * std::log10(g), kLevelOff) : kLevelOff;
}

// wet and dry levels (dB) equivalent to a blend of CAPS (comparison with CAPS in the tests)
static inline float blendWetDb(const float blend) { return levelDb(wet(blend)); }
static inline float blendDryDb(const float blend) { return levelDb(1.0f - wet(blend)); }

// gain of a dry or wet level: 0 at kLevelOff, exactly 1 at 0 dB
static inline float levelGain(const float db)
{
    if (db <= kLevelOff)
        return 0.0f;
    return db == 0.0f ? 1.0f : std::pow(10.0f, db * 0.05f);
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

// linked dry and wet (kParamLink): levels of the faders shown for a mix, and the mix for a wet
// gain (inverse of wet())
static inline float mixWetDb(const float mix) { return levelDb(wet(mix)); }
static inline float mixDryDb(const float mix) { return levelDb(1.0f - wet(mix)); }
static inline float mixOfWetGain(const float g)
{
    return static_cast<float>(std::pow(std::fmin(std::fmax(g, 0.0f), 1.0f), 1.0 / 1.53));
}

}   // namespace TinplateLaw

#endif
