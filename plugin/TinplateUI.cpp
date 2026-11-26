/*
 * Tinplate Reverb - user interface (DPF / NanoVG).
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Everything is drawn in a logical 900 x 730 space, scaled to the window size (resizable,
 * aspect ratio kept).  Three sections, left to right in the order of the signal:
 *   1 INPUT (pre-delay, bandwidth) -> 2 PLATE (tail, damping, freeze, decay time)
 *   -> 3 OUTPUT (width, low cut; faders DRY, REVERB, OUTPUT; meters),
 * below them 4 DUCKING (duck, threshold, attack, release, cut meter) and 5 CLEAR (on / off,
 * floor, trigger, fade, state),
 * and at the bottom the decay time (RT60) of the plate against frequency with the tone of the wet
 * signal (bandwidth and low cut).
 * Knobs: drag vertically (Shift = fine), mouse wheel, double-click or Ctrl+click = default,
 * right-click = type a value (Enter / Escape).  Every control has a tooltip.
 * A / B: two sets of settings to compare (header: A | B, copy to the other); the inactive one is
 * saved with the project (DPF state "ab").
 * Presets: drop-down list in the header (click to open, wheel over the closed box to step),
 * factory presets then the presets saved by the user (TinplateUserPresets.hpp: "Save current
 * settings...", name typed in the box; delete with the cross of a saved preset, clicked twice).
 * Two themes, Slate (dark) and Tin (light), switched by the SLATE | TIN pill of the header; the
 * choice is a preference of the user (TinplatePrefs.hpp), not saved with the project.
 */

#include "DistrhoUI.hpp"
#include "TinplateParams.hpp"
#include "TinplatePrefs.hpp"
#include "TinplateUserPresets.hpp"
#include "Rt60.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;

static const float kW = 900.0f;
static const float kH = 730.0f;
static const float kPi = 3.14159265358979f;
static const float kArcStart = 0.75f * kPi;
static const float kArcSweep = 1.5f * kPi;

// ------------------------------------------------------------------------------------------------
// Themes: every colour of the interface.  Slate (dark: steel blue and brass on slate) and Tin
// (light: the same accents on pale metal).  The choice is a preference of the user
// (TinplatePrefs.hpp), switched by the SLATE | TIN pill of the header.

struct Theme {
    // background, header, panels
    Color bg0, bg1, bgGlow, header0, header1, headerLine, panel0, panel1, panelEdge, panelShadow, divider;
    // text: normal, secondary, faint, on an accent fill
    Color text, dim, faint, onAccent;
    // accents: steel (hot), brass (gold), dark steel (deep)
    Color hot, gold, deep, arrow, brandGlow;
    // buttons, LEDs, segmented selectors, recessed wells
    Color btnOn0, btnOn1, btnOff0, btnOff1, btnOnEdge, ledGlow, ledGlowEnd, ledOn, ledOff;
    Color well, plot, deepWell, hoverEdge, sel0, sel1;
    // knobs
    Color knobShadow, knob0, knob1, knobIn0, knobIn1, knobEdge, arcGlow, arcGlowActive, pointerGlow, pointer;
    // decay plot, meters
    Color grid, gridZero, curveGlow, tone, meterEdge;
    // tooltip; label of the Bypass button when bypassed
    Color tipBg, tipEdge, bypassText;
};

static Color transparent(const Color& c)
{
    Color t(c);
    t.alpha = 0.0f;
    return t;
}

static const Theme kThemes[TinplatePrefs::kThemeCount] = {
    {   // Slate
        Color(12, 15, 19), Color(24, 30, 37), Color(80, 160, 200, 0.16f), Color(28, 35, 43), Color(14, 18, 23),
        Color(Color(110, 190, 225), Color(0, 0, 0), 0.35f),
        Color(27, 33, 40), Color(20, 25, 31), Color(52, 64, 76), Color(0, 0, 0, 0.55f), Color(52, 64, 76, 0.7f),
        Color(220, 228, 234), Color(130, 146, 158), Color(84, 98, 110), Color(235, 245, 250),
        Color(110, 190, 225), Color(232, 196, 120), Color(40, 90, 120), Color(110, 190, 225, 0.7f),
        Color(110, 190, 225, 0.6f),
        Color(52, 110, 145), Color(28, 70, 98), Color(32, 40, 48), Color(21, 26, 32), Color(110, 190, 225, 0.8f),
        Color(130, 210, 240, 0.8f), Color(130, 210, 240, 0.0f), Color(200, 240, 255), Color(44, 54, 62),
        Color(13, 17, 21), Color(11, 14, 18), Color(9, 12, 15), Color(110, 190, 225, 0.6f),
        Color(60, 130, 170), Color(30, 80, 110),
        Color(0, 0, 0, 0.7f), Color(78, 90, 100), Color(26, 31, 37), Color(66, 77, 86), Color(22, 27, 32),
        Color(0, 0, 0, 0.6f), Color(110, 190, 225, 0.18f), Color(110, 190, 225, 0.30f), Color(110, 190, 225, 0.35f),
        Color(190, 225, 240),
        Color(60, 75, 88, 0.6f), Color(100, 120, 135, 0.8f), Color(232, 196, 120, 0.22f), Color(110, 190, 225),
        Color(44, 56, 66),
        Color(8, 11, 14, 0.95f), Color(110, 190, 225, 0.6f), Color(232, 196, 120),
    },
    {   // Tin (light grey with a slight sage green tint)
        Color(201, 209, 208), Color(214, 221, 219), Color(30, 110, 150, 0.07f), Color(223, 230, 227),
        Color(210, 219, 217), Color(30, 110, 150, 0.55f),
        Color(228, 234, 230), Color(221, 228, 225), Color(182, 194, 195), Color(40, 55, 70, 0.16f),
        Color(182, 194, 195, 0.8f),
        Color(30, 38, 45), Color(95, 108, 118), Color(140, 150, 158), Color(229, 236, 233),
        Color(25, 110, 150), Color(150, 105, 25), Color(15, 70, 100), Color(25, 110, 150, 0.75f),
        Color(25, 110, 150, 0.22f),
        Color(40, 130, 170), Color(20, 90, 125), Color(229, 234, 231), Color(214, 221, 219),
        Color(25, 110, 150, 0.9f),
        Color(40, 160, 210, 0.55f), Color(40, 160, 210, 0.0f), Color(20, 140, 200), Color(182, 193, 194),
        Color(215, 223, 221), Color(229, 234, 231), Color(199, 209, 208), Color(25, 110, 150, 0.7f),
        Color(40, 130, 170), Color(20, 90, 125),
        Color(40, 55, 70, 0.30f), Color(231, 237, 233), Color(199, 209, 208), Color(228, 234, 230),
        Color(207, 216, 214),
        Color(90, 105, 118, 0.45f), Color(25, 110, 150, 0.12f), Color(25, 110, 150, 0.22f), Color(25, 110, 150, 0.18f),
        Color(20, 90, 125),
        Color(140, 155, 165, 0.35f), Color(110, 125, 135, 0.7f), Color(150, 105, 25, 0.15f), Color(25, 110, 150),
        Color(178, 191, 192),
        Color(231, 237, 233, 0.97f), Color(25, 110, 150, 0.6f), Color(255, 236, 200),
    },
};

static const char* const kThemeNames[TinplatePrefs::kThemeCount] = { "SLATE", "TIN" };

// ------------------------------------------------------------------------------------------------
// Layout

struct Rect {
    float x, y, w, h;
    bool contains(const float px, const float py) const
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

// sections
static const Rect kSecIn   = { 20, 70, 220, 380 };
static const Rect kSecTank = { 250, 70, 370, 380 };
static const Rect kSecOut  = { 630, 70, 250, 380 };
static const Rect kSecDuck = { 20, 460, 425, 122 };
static const Rect kSecClear = { 455, 460, 425, 122 };
static const Rect kDecay   = { 20, 592, 860, 128 };
static const Rect kPlot    = { 150, 600, 680, 104 };   // decay time against frequency

// knobs and faders (a fader: cx, cy = centre of its track, r = half its length)
enum { kKnobPredelay = 0, kKnobBandwidth, kKnobTail, kKnobDamping, kKnobWet, kKnobWidth, kKnobLowCut,
       kKnobOutput, kKnobDuck, kKnobThreshold, kKnobAttack, kKnobRelease, kKnobFloor, kKnobTrigger, kKnobFade,
       kKnobWindow, kKnobDry, kNumKnobs };

struct KnobDef {
    int param;
    float cx, cy, r;
    const char* label;
    const char* help;
};

static const KnobDef kKnobs[kNumKnobs] = {
    { kParamPredelay,  130.0f, 162.0f, 44.0f, "PRE-DELAY",
      "Delay before the plate: separates the reverb from the direct sound (clearer vocals). "
      "A change crossfades in 30 ms." },
    { kParamBandwidth, 130.0f, 324.0f, 44.0f, "BANDWIDTH",
      "Low-pass at the input of the plate (control of the original). Low: darker, softer onset. "
      "Shown: its -3 dB frequency at this sample rate." },
    { kParamTail,      360.0f, 165.0f, 50.0f, "TAIL",
      "Length of the reverb: gain of the loop of the plate (control of the original, 0 to 1; "
      "same law up to 1.16: 10 s at 500 Hz). Shown: the decay time at 500 Hz, measured on the plate." },
    { kParamDamping,   510.0f, 165.0f, 50.0f, "DAMPING",
      "Low-pass inside the loop (control of the original): the highs die sooner than the lows. "
      "Shown: its -3 dB frequency at this sample rate." },
    { kParamWet,       797.0f, 215.0f, 95.0f, "REVERB",
      "Level of the reverb alone, dB (at the bottom: off). Reverb at 0 dB and Dry off: reverb only "
      "(for a send)." },
    { kParamWidth,     675.0f, 160.0f, 24.0f, "WIDTH",
      "Stereo width of the reverb: 0 % mono, 100 % as the plate gives it, 200 % side doubled." },
    { kParamLowCut,    675.0f, 288.0f, 24.0f, "LOW CUT",
      "High-pass on the reverb only (12 dB / octave): keeps the low end of the mix clear. "
      "At the minimum: off." },
    { kParamOutput,    849.0f, 215.0f, 95.0f, "OUTPUT",
      "Gain of the whole output, dry and wet." },
    { kParamDuck,       85.0f, 520.0f, 22.0f, "DUCK",
      "Ducking: the reverb is lowered while the input plays and comes back in the gaps "
      "(clearer vocals). Largest cut of the reverb, dB. At 0: off." },
    { kParamThreshold, 185.0f, 520.0f, 22.0f, "THRESHOLD",
      "Input peak level where the ducking starts: the reverb is cut by as many dB as the input "
      "goes over it, at most the Duck depth." },
    { kParamAttack,    285.0f, 520.0f, 22.0f, "ATTACK",
      "How fast the reverb goes down when the input comes in." },
    { kParamRelease,   385.0f, 520.0f, 22.0f, "RELEASE",
      "How fast the reverb comes back when the input stops: short, it swells in every gap; "
      "long, it comes back at the end of the phrase." },
    { kParamFloor,     600.0f, 520.0f, 20.0f, "FLOOR",
      "Clear: the detection is armed while the input stays under this peak level (and the "
      "previous clearing has ended)." },
    { kParamTrigger,   680.0f, 520.0f, 20.0f, "TRIGGER",
      "Clear: an armed input that goes over this level within the Window is a new attack, and "
      "empties the plate." },
    { kParamFade,      760.0f, 520.0f, 20.0f, "FADE",
      "Clear: how fast the old reverb fades out at a new attack (the new sound goes into an "
      "empty plate at once)." },
    { kParamWindow,    838.0f, 520.0f, 20.0f, "WINDOW",
      "Clear: longest time for the input to go from Floor to Trigger to count as an attack. "
      "Short: only sharp attacks (drums, plucks); long: softer ones too." },
    { kParamDry,       745.0f, 215.0f, 95.0f, "DRY",
      "Level of the dry signal, dB (at the bottom: off). Not delayed: no latency." },
};

static const Rect kFreeze  = { 380, 318, 110, 30 };
static const Rect kRt      = { 270, 360, 330, 80 };    // decay time readout
static const Rect kPreset  = { 384, 16, 196, 26 };     // drop-down box; the list opens below it
static const Rect kAB      = { 588, 17, 50, 24 };      // A | B
static const Rect kCopy    = { 642, 17, 34, 24 };      // copy the current set to the other
static const float kMenuItemH = 22.0f;     // preset list: item
static const float kMenuHeaderH = 20.0f;   // section title (FACTORY, USER)
static const float kMenuDividerH = 9.0f;   // line before "Save current settings..."

// rows of the preset list
enum { kRowHeader = 0, kRowFactory, kRowUser, kRowNone, kRowDivider, kRowSave };

struct MenuRow {
    int kind;
    int index;    // factory preset, saved preset (in fUserNames) or section (0 factory, 1 user)
    float y, h;   // y from the top of the list content
};
static const Rect kTheme   = { 684, 17, 76, 24 };
static const Rect kBypass  = { 768, 14, 108, 30 };

// meters: three horizontal bars in the output section
static const float kMeterX = 690.0f, kMeterW = 132.0f, kMeterH = 9.0f;
static const float kMeterY[3] = { 384.0f, 404.0f, 424.0f };   // IN, REVERB, OUT
static const Rect kDuckMeter = { 272, 473, 110, 8 };             // cut of the reverb by the ducking
static const Rect kClearBtn  = { 470, 498, 84, 30 };              // clear on / off
static const Rect kLink      = { 760, 92, 22, 18 };               // chain: dry and reverb faders linked
static const Rect kClearState = { 470, 540, 84, 30 };             // off, waiting, armed, cleared

// hover zones with a tooltip (knobs are handled separately)
enum {
    kZoneFreeze = 0, kZoneRt, kZonePlot, kZonePreset, kZoneBypass, kZoneTheme, kZoneAB, kZoneCopy,
    kZoneIn, kZoneWet, kZoneOut, kZoneDuckMeter, kZoneClear, kZoneClearState, kZoneLink, kNumZones
};

struct Zone {
    Rect r;
    const char* title;
    const char* help;
};

static const Zone kZones[kNumZones] = {
    { kFreeze, "FREEZE",
      "Holds the reverb forever: the input of the plate closes and the loop stops losing energy "
      "(no damping, no modulation). Play over the frozen sound; the dry signal still passes." },
    { kRt, "DECAY TIME",
      "Time for the reverb to fall by 60 dB (RT60), measured on the impulse response of the plate "
      "(table interpolated, within about 10 %)." },
    { kPlot, "DECAY TIME AND TONE",
      "Brass: decay time (RT60, left scale, seconds) against frequency, set by Tail and Damping. "
      "Steel, dashed: tone of the reverb (right scale, dB), set by Bandwidth and Low cut." },
    { kPreset, "PRESETS",
      "Click to choose a preset: factory presets, then yours (saved for all projects on this "
      "computer). Wheel: next / previous. A * means a setting was changed since it was loaded." },
    { kBypass, "BYPASS",
      "Passes the input unprocessed (crossfaded in 10 ms, no click)." },
    { kTheme, "THEME",
      "SLATE: dark. TIN: light. Kept for all instances and projects (a setting of this computer, "
      "not of the project); other open windows change when reopened." },
    { kAB, "A / B",
      "Two sets of settings to compare: click A or B to switch. Both are saved with the project." },
    { kCopy, "COPY",
      "Copies the current settings into the other set (A > B or B > A)." },
    { { kMeterX - 46, kMeterY[0] - 7, kMeterW + 100, 16 }, "IN", "Input peak level, dBFS." },
    { { kMeterX - 46, kMeterY[1] - 7, kMeterW + 100, 16 }, "REVERB",
      "Peak level of the reverb alone (after low cut, width and ducking, before the Reverb fader), "
      "dBFS." },
    { { kMeterX - 46, kMeterY[2] - 7, kMeterW + 100, 16 }, "OUT", "Output peak level, dBFS." },
    { { kDuckMeter.x - 36, kDuckMeter.y - 7, kDuckMeter.w + 76, 22 }, "CUT",
      "Cut of the reverb by the ducking now, dB (0 to 30)." },
    { kClearBtn, "CLEAR",
      "A new attack empties the plate: the old reverb fades out and the new sound starts in a "
      "clean plate. Attack: the input rises from under Floor to over Trigger within the Window. "
      "Not while frozen." },
    { kClearState, "CLEAR STATE",
      "WAITING: the input is not under Floor yet, or the old reverb is still fading. "
      "ARMED: the next attack will clear. CLEARED: lit 0.2 s at each clearing." },
    { kLink, "LINK",
      "Links the Dry and Reverb faders as the Blend of the original: raising one lowers the "
      "other (their gains add up to 1, both at most 0 dB). When linking, the Dry fader sets the "
      "mix. Host automation follows the link too: the fader moved last leads." },
};

static const int kHoverZoneBase = 100;

// position of the dry and wet faders: piecewise linear in dB, off at the bottom
static const int kLawPoints = 6;
static const float kLawDb[kLawPoints] = { -60.0f, -40.0f, -24.0f, -12.0f, 0.0f, 6.0f };
static const float kLawPos[kLawPoints] = { 0.0f, 0.15f, 0.35f, 0.55f, 0.82f, 1.0f };

// ------------------------------------------------------------------------------------------------

class TinplateUI : public UI
{
public:
    TinplateUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true)
    {
        for (int i = 0; i < kParamCount; ++i)
            fValues[i] = kTinplateRanges[i].def;
        for (int i = 0; i < kPresetParamCount; ++i)
            fPresetValues[i] = kTinplatePresets[0].values[i];
        loadSharedResources();
        fFont = findFont(NANOVG_DEJAVU_SANS_TTF);
#ifdef TINPLATE_UI_DEMO_THEME
        fTheme = TINPLATE_UI_DEMO_THEME;   // screenshots: forced, never saved
#else
        fTheme = TinplatePrefs::loadTheme();
#endif
        fT = &kThemes[fTheme];
        fLastPaint = std::chrono::steady_clock::now();
#ifdef TINPLATE_UI_DEMO
        // screenshots only (-DTINPLATE_UI_DEMO=<hover id> [-DTINPLATE_UI_DEMO_PRESET=<index>]):
        // a tooltip and meters, since there is no audio
        fHover = TINPLATE_UI_DEMO;
        fValues[kParamMeterIn] = -9.5f;
        fValues[kParamMeterWet] = -17.0f;
        fValues[kParamMeterOut] = -8.2f;
        fValues[kParamMeterDuck] = 7.5f;
        fValues[kParamMeterClear] = 1.0f;
#endif
    }

protected:
    // ------------------------------------------------------------------ host -> UI

    void parameterChanged(const uint32_t index, const float value) override
    {
        if (index >= kParamCount || fValues[index] == value)
            return;
#ifdef TINPLATE_UI_DEMO
        if (index >= static_cast<uint32_t>(kParamFirstOutput))
            return;   // keep the meters of the screenshot
#endif
        fValues[index] = value;
        // linked (by the host too): the other level as the DSP computes it
        if (index == kParamDry || index == kParamWet)
        {
            const int q = partnerOf(static_cast<int>(index));
            if (q >= 0)
                fValues[q] = TinplateLaw::linkedDb(value);
        }
        else if (index == kParamLink && linked())
            fValues[kParamWet] = TinplateLaw::linkedDb(fValues[kParamDry]);
        if (index < static_cast<uint32_t>(kParamFirstOutput))
            repaint();
        else
            fMetersChanged = true;
    }

    // saved A / B set: "slot=<0|1>;other=<values of the inactive set>"
    void stateChanged(const char* const key, const char* const value) override
    {
        if (std::strcmp(key, kStateKeys[kStateAB]) == 0)
            readABState(value);
    }

    void programLoaded(const uint32_t index) override
    {
        if (index < static_cast<uint32_t>(kTinplatePresetCount))
        {
            fPreset = static_cast<int>(index);
            fPresetUser = false;
            fPresetName = kTinplatePresets[index].name;
            for (int i = 0; i < kPresetParamCount; ++i)
                fValues[i] = fPresetValues[i] = kTinplatePresets[index].values[i];
            repaint();
        }
    }

    void uiIdle() override
    {
#ifdef TINPLATE_UI_DEMO_PRESET
        if (!fDemoPresetDone)
        {
            fDemoPresetDone = true;
            applyPreset(TINPLATE_UI_DEMO_PRESET);
        }
#endif
#ifdef TINPLATE_UI_DEMO_FREEZE
        if (!fDemoFreezeDone)
        {
            fDemoFreezeDone = true;
            setOnce(kParamFreeze, 1.0f);
        }
#endif
#ifdef TINPLATE_UI_DEMO_CLEAR
        if (!fDemoClearDone)
        {
            fDemoClearDone = true;
            setOnce(kParamClear, 1.0f);
        }
#endif
#ifdef TINPLATE_UI_DEMO_LINK
        if (!fDemoLinkDone)
        {
            fDemoLinkDone = true;
            toggleLink();
        }
#endif
#ifdef TINPLATE_UI_DEMO_MENU
        // screenshots: the list open, the mouse over row TINPLATE_UI_DEMO_MENU
        // (with TINPLATE_UI_DEMO_ARM: its cross clicked once)
        if (!fDemoMenuDone)
        {
            fDemoMenuDone = true;
            openMenu();
            fMenuHover = TINPLATE_UI_DEMO_MENU;
# ifdef TINPLATE_UI_DEMO_ARM
            fDeleteArmed = fMenuHover;
# endif
        }
#endif
#ifdef TINPLATE_UI_DEMO_NAME
        // screenshots: a name being typed
        if (!fDemoNameDone)
        {
            fDemoNameDone = true;
            startNaming();
            fNameText = TINPLATE_UI_DEMO_NAME;
        }
#endif
        // meters: at most 30 repaints per second
        const auto now = std::chrono::steady_clock::now();
        if (!fMetersChanged || now - fLastPaint < std::chrono::milliseconds(33))
            return;
        fMetersChanged = false;
        fLastPaint = now;
        repaint();
    }

    // ------------------------------------------------------------------ drawing

    void onNanoDisplay() override
    {
        const float s = scaleFactor();
        save();
        scale(s, s);

        drawBackground();
        drawHeader();
        drawInputSection();
        drawTankSection();
        drawOutputSection();
        drawDuckSection();
        drawClearSection();
        drawDecayPlot();
        if (fMenuOpen)
            drawPresetMenu();
        else if (fNaming)
            drawNamingHint();
        else if (fHover >= 0 && fDrag < 0 && fEdit < 0)
            drawTooltip(fHover);

        restore();
    }

    // ------------------------------------------------------------------ mouse

    bool onMouse(const MouseEvent& ev) override
    {
        const float s = scaleFactor();
        const float x = static_cast<float>(ev.pos.getX()) / s;
        const float y = static_cast<float>(ev.pos.getY()) / s;

        if (!ev.press)
        {
            if (ev.button == kMouseButtonLeft && fDrag >= 0)
            {
                editParameter(kKnobs[fDrag].param, false);
                if (fDragPartner >= 0)
                    editParameter(fDragPartner, false);
                fDrag = -1;
                repaint();
                return true;
            }
            return false;
        }

        if (fEdit >= 0)
            commitEdit(true);
        if (fNaming)
            cancelNaming();   // a click elsewhere cancels the name being typed

        // open list: a click chooses an item; outside the list it closes it (and goes no further)
        if (fMenuOpen)
        {
            menuClick(x, y, ev.button == kMouseButtonLeft);
            return true;
        }

        const int k = knobAt(x, y);

        if (ev.button == kMouseButtonRight)
        {
            if (k < 0)
                return false;
            fEdit = k;
            formatNumber(kKnobs[k].param, fEditText, sizeof(fEditText));
            fEditFresh = true;
            repaint();
            return true;
        }

        if (ev.button != kMouseButtonLeft)
            return false;

        if (kPreset.contains(x, y)) { openMenu(); return true; }
        if (kAB.contains(x, y)) { switchSlot(static_cast<int>(segmentAt(kAB, x, 2))); return true; }
        if (kCopy.contains(x, y)) { copySlot(); return true; }
        if (kBypass.contains(x, y)) { toggle(kParamBypass); return true; }
        if (kTheme.contains(x, y)) { setTheme(static_cast<int>(segmentAt(kTheme, x, TinplatePrefs::kThemeCount))); return true; }
        if (kFreeze.contains(x, y)) { toggle(kParamFreeze); return true; }
        if (kClearBtn.contains(x, y)) { toggle(kParamClear); return true; }
        if (kLink.contains(x, y)) { toggleLink(); return true; }

        if (k < 0)
            return false;

        const int p = kKnobs[k].param;
        const bool doubleClick = k == fLastClickTarget && ev.time - fLastClickTime < 350;
        fLastClickTarget = k;
        fLastClickTime = ev.time;

        if (doubleClick || (ev.mod & kModifierControl))
        {
            setOnce(p, defaultValue(p));
            fLastClickTarget = -1;
            return true;
        }

        fDrag = k;
        fLastY = y;
        editParameter(p, true);
        fDragPartner = partnerOf(p);
        if (fDragPartner >= 0)
            editParameter(fDragPartner, true);
        repaint();
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = scaleFactor();
        const float x = static_cast<float>(ev.pos.getX()) / s;
        const float y = static_cast<float>(ev.pos.getY()) / s;

        if (fMenuOpen)
        {
            int row = rowAt(x, y);
            if (row >= 0 && fRows[row].kind != kRowFactory && fRows[row].kind != kRowUser && fRows[row].kind != kRowSave)
                row = -1;
            const bool onCross = row >= 0 && fRows[row].kind == kRowUser && crossRect(rowRect(row)).contains(x, y);
            if (row != fMenuHover || onCross != fMenuOnCross)
            {
                if (row != fMenuHover)
                    fDeleteArmed = -1;
                fMenuHover = row;
                fMenuOnCross = onCross;
                repaint();
            }
            return true;
        }

        if (fDrag >= 0)
        {
            const int p = kKnobs[fDrag].param;
            float pixels = (ev.mod & kModifierShift) ? 900.0f : 220.0f;
            if (isFader(p))
                pixels = 2.0f * kKnobs[fDrag].r * ((ev.mod & kModifierShift) ? 4.0f : 1.0f);   // follows the mouse
            const float norm = toNorm(p, fValues[p]) + (fLastY - y) / pixels;
            fLastY = y;
            changeValue(p, fromNorm(p, norm));
            return true;
        }

        int h = knobAt(x, y);
        if (h < 0)
            for (int z = 0; z < kNumZones; ++z)
                if (kZones[z].r.contains(x, y))
                {
                    h = kHoverZoneBase + z;
                    break;
                }
        if (h != fHover)
        {
            fHover = h;
            repaint();
        }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = scaleFactor();
        const float x = static_cast<float>(ev.pos.getX()) / s, y = static_cast<float>(ev.pos.getY()) / s;
        const float d = static_cast<float>(ev.delta.getY());
        if (fMenuOpen)
        {
            // long list: scrolls
            const float most = fMenuContentH - menuVisibleH();
            if (most > 0.0f && d != 0.0f)
            {
                fMenuScroll = std::fmin(std::fmax(fMenuScroll - d * kMenuItemH, 0.0f), most);
                fMenuHover = fDeleteArmed = -1;
                repaint();
            }
            return true;
        }
        if (kPreset.contains(x, y) && !fNaming)
        {
            // wheel up: previous preset, down: next
            if (d != 0.0f)
                stepPreset(d > 0.0f ? -1 : 1);
            return true;
        }
        const int k = knobAt(x, y);
        if (k < 0)
            return false;
        const int p = kKnobs[k].param;
        const float step = (ev.mod & kModifierShift) ? 0.005f : 0.025f;
        setOnce(p, fromNorm(p, toNorm(p, fValues[p]) + d * step));
        return true;
    }

    // ------------------------------------------------------------------ keyboard (value entry)

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (fMenuOpen && ev.press && ev.key == kKeyEscape)
        {
            closeMenu();
            return true;
        }
        if (fNaming)
        {
            if (!ev.press)
                return true;
            if (ev.key == kKeyEnter)
                commitNaming();
            else if (ev.key == kKeyEscape)
                cancelNaming();
            else if ((ev.key == kKeyBackspace || ev.key == kKeyDelete) && !fNameText.empty())
            {
                fNameText.pop_back();
                fNameError = false;
                repaint();
            }
            return true;
        }
        if (fEdit < 0 || !ev.press)
            return fEdit >= 0;
        if (ev.key == kKeyEnter)
            commitEdit(true);
        else if (ev.key == kKeyEscape)
            commitEdit(false);
        else if (ev.key == kKeyBackspace || ev.key == kKeyDelete)
        {
            const size_t n = std::strlen(fEditText);
            if (fEditFresh)
                fEditText[0] = '\0';
            else if (n > 0)
                fEditText[n - 1] = '\0';
            fEditFresh = false;
            repaint();
        }
        return true;
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (fNaming)
        {
            // characters allowed in a preset name (TinplateUserPresets::validName)
            const uint32_t c = ev.character;
            if (c >= 32 && c <= 126 && std::strchr("<>:\"/\\|?*", static_cast<int>(c)) == nullptr
                && fNameText.size() < TinplateUserPresets::kMaxNameLength)
            {
                fNameText.push_back(static_cast<char>(c));
                fNameError = false;
                repaint();
            }
            return true;
        }
        if (fEdit < 0)
            return false;
        char c = static_cast<char>(ev.character);
        if (c == ',')
            c = '.';
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+')
        {
            if (fEditFresh)
                fEditText[0] = '\0';
            fEditFresh = false;
            const size_t n = std::strlen(fEditText);
            if (n + 1 < sizeof(fEditText))
            {
                fEditText[n] = c;
                fEditText[n + 1] = '\0';
            }
            repaint();
        }
        return true;
    }

private:
    // ------------------------------------------------------------------ helpers

    float scaleFactor() const { return static_cast<float>(getWidth()) / kW; }

    double sampleRate() const { return getSampleRate() > 0.0 ? getSampleRate() : 44100.0; }

    bool frozen() const { return fValues[kParamFreeze] > 0.5f; }

    static float defaultValue(const int p) { return kTinplateRanges[p].def; }

    static bool logarithmic(const int p)
    {
        return p == kParamLowCut || p == kParamAttack || p == kParamRelease || p == kParamFade
            || p == kParamWindow;
    }

    static bool isFader(const int p) { return p == kParamWet || p == kParamDry || p == kParamOutput; }

    // position of the dry and wet faders: piecewise linear in dB (kLawDb, kLawPos)
    static float levelToNorm(const float db)
    {
        if (db <= kLawDb[0])
            return 0.0f;
        for (int i = 1; i < kLawPoints; ++i)
            if (db <= kLawDb[i])
                return kLawPos[i - 1] + (kLawPos[i] - kLawPos[i - 1]) * (db - kLawDb[i - 1]) / (kLawDb[i] - kLawDb[i - 1]);
        return 1.0f;
    }

    static float normToLevel(const float n)
    {
        for (int i = 1; i < kLawPoints; ++i)
            if (n <= kLawPos[i])
                return kLawDb[i - 1] + (kLawDb[i] - kLawDb[i - 1]) * (n - kLawPos[i - 1]) / (kLawPos[i] - kLawPos[i - 1]);
        return kLawDb[kLawPoints - 1];
    }

    // knob position: low cut, attack and release logarithmic, the others linear
    static float toNorm(const int p, const float v)
    {
        const TinplateRange& r = kTinplateRanges[p];
        if (p == kParamWet || p == kParamDry)
            return levelToNorm(v);
        if (logarithmic(p))
            return std::log(std::fmax(v, r.min) / r.min) / std::log(r.max / r.min);
        return (v - r.min) / (r.max - r.min);
    }

    static float fromNorm(const int p, float n)
    {
        const TinplateRange& r = kTinplateRanges[p];
        n = std::fmin(std::fmax(n, 0.0f), 1.0f);
        if (p == kParamWet || p == kParamDry)
            return normToLevel(n);
        if (logarithmic(p))
            return r.min * std::pow(r.max / r.min, n);
        return r.min + n * (r.max - r.min);
    }

    static float clampValue(const int p, const float v)
    {
        const TinplateRange& r = kTinplateRanges[p];
        return std::fmin(std::fmax(v, r.min), r.max);
    }

    static float segmentAt(const Rect& r, const float x, const int count)
    {
        const int i = static_cast<int>((x - r.x) / (r.w / count));
        return static_cast<float>(i < 0 ? 0 : i >= count ? count - 1 : i);
    }

    bool linked() const { return fValues[kParamLink] > 0.5f; }

    // the other of the linked dry and reverb faders, or -1
    int partnerOf(const int p) const
    {
        if (!linked() || fLoading)
            return -1;
        return p == kParamDry ? kParamWet : p == kParamWet ? kParamDry : -1;
    }

    void changeValue(const int p, float v)
    {
        const int q = partnerOf(p);
        if (q >= 0)
        {
            // linked, as the blend of the original: gains of dry and reverb add up to 1
            v = std::fmin(v, 0.0f);
            const float w = TinplateLaw::linkedDb(v);
            if (fValues[q] != w)
            {
                fValues[q] = w;
                setParameterValue(q, w);
            }
        }
        if (fValues[p] == v)
            return;
        fValues[p] = v;
        setParameterValue(p, v);
        repaint();
    }

    // one complete edit gesture (both faders when they are linked)
    void setOnce(const int p, const float v)
    {
        const int q = partnerOf(p);
        editParameter(p, true);
        if (q >= 0)
            editParameter(q, true);
        changeValue(p, v);
        editParameter(p, false);
        if (q >= 0)
            editParameter(q, false);
    }

    // link or unlink the dry and reverb faders (parameter Link, applied by the DSP); when
    // linking, the dry fader sets the mix
    void toggleLink()
    {
        toggle(kParamLink);
        if (linked())
            setOnce(kParamDry, fValues[kParamDry]);
        repaint();
    }

    void toggle(const int p) { setOnce(p, fValues[p] > 0.5f ? 0.0f : 1.0f); }

    // interface only: no parameter, no effect on the sound; saved as a preference of the user
    void setTheme(const int theme)
    {
        if (theme == fTheme)
            return;
        fTheme = theme;
        fT = &kThemes[theme];
#ifndef TINPLATE_UI_DEMO_THEME
        TinplatePrefs::saveTheme(theme);
#endif
        repaint();
    }

    // ---- A / B: the active set is the live parameters, the other one is kept here

    void storeSlot(const int slot)
    {
        Slot& s = fSlots[slot];
        for (int i = 0; i < kPresetParamCount; ++i)
        {
            s.values[i] = fValues[i];
            s.presetValues[i] = fPresetValues[i];
        }
        s.preset = fPreset;
        s.user = fPresetUser;
        s.name = fPresetName;
        s.set = true;
    }

    void switchSlot(const int slot)
    {
        if (slot == fSlot)
            return;
        storeSlot(fSlot);
        fSlot = slot;
        if (!fSlots[slot].set)
            storeSlot(slot);   // first visit: starts as a copy of the other set
        const Slot s = fSlots[slot];
        loadValues(s.values);
        for (int i = 0; i < kPresetParamCount; ++i)
            fPresetValues[i] = s.presetValues[i];
        fPreset = s.preset;
        fPresetUser = s.user;
        fPresetName = s.name;
        writeABState();
        repaint();
    }

    void copySlot()
    {
        storeSlot(1 - fSlot);
        writeABState();
        repaint();
    }

    void writeABState()
    {
        const Slot& other = fSlots[1 - fSlot];
        std::string text = "slot=" + std::to_string(fSlot) + ";other=";
        for (int i = 0; i < kPresetParamCount; ++i)
            text += (i ? "," : "") + TinplateUserPresets::number(other.set ? other.values[i] : fValues[i]);
        setState(kStateKeys[kStateAB], text.c_str());
    }

    void readABState(const char* const value)
    {
        const char* const slot = std::strstr(value, "slot=");
        const char* const other = std::strstr(value, "other=");
        if (slot == nullptr || other == nullptr || (slot[5] != '0' && slot[5] != '1'))
            return;
        std::istringstream in(other + 6);
        in.imbue(std::locale::classic());
        float values[kPresetParamCount];
        for (int i = 0; i < kPresetParamCount; ++i)
        {
            float v = 0.0f;
            char comma = ',';
            if (!(in >> v) || !std::isfinite(v) || (i + 1 < kPresetParamCount && !(in >> comma)) || comma != ',')
                return;
            values[i] = clampValue(i, v);
        }
        fSlot = slot[5] - '0';
        Slot& s = fSlots[1 - fSlot];
        for (int i = 0; i < kPresetParamCount; ++i)
            s.values[i] = s.presetValues[i] = values[i];
        s.preset = -1;
        s.user = false;
        s.name = fSlot == 0 ? "Set B" : "Set A";   // the preset of a saved set is not kept
        s.set = true;
        repaint();
    }

    // loads a set of values: one edit gesture per parameter, remembered to show the "*"
    void loadValues(const float values[kPresetParamCount])
    {
        fLoading = true;   // the values are taken as they are, link or not
        for (int i = 0; i < kPresetParamCount; ++i)
        {
            setOnce(i, values[i]);
            fPresetValues[i] = values[i];
        }
        fLoading = false;
        repaint();
    }

    void applyPreset(const int index)
    {
        loadValues(kTinplatePresets[index].values);
        fPreset = index;
        fPresetUser = false;
        fPresetName = kTinplatePresets[index].name;
    }

    // a saved preset, read from its file now (it may have been changed by another instance)
    void applyUserPreset(const std::string& name)
    {
        float values[kPresetParamCount];
        if (!TinplateUserPresets::load(name, values))
            return;
        loadValues(values);
        fPreset = -1;
        fPresetUser = true;
        fPresetName = name;
    }

    // next (+1) or previous (-1) preset, factory presets then saved ones, in a loop
    void stepPreset(const int step)
    {
        fUserNames = TinplateUserPresets::list();
        const int n = kTinplatePresetCount + static_cast<int>(fUserNames.size());
        int pos = fPreset;
        if (fPresetUser)
        {
            pos = kTinplatePresetCount - 1;   // deleted meanwhile: between the two sections
            for (size_t j = 0; j < fUserNames.size(); ++j)
                if (TinplateUserPresets::sameName(fUserNames[j], fPresetName))
                    pos = kTinplatePresetCount + static_cast<int>(j);
        }
        pos = (pos + step + n) % n;
        if (pos < kTinplatePresetCount)
            applyPreset(pos);
        else
            applyUserPreset(fUserNames[pos - kTinplatePresetCount]);
    }

    bool isCurrentRow(const MenuRow& m) const
    {
        if (m.kind == kRowFactory)
            return !fPresetUser && fPreset == m.index;
        return m.kind == kRowUser && fPresetUser && TinplateUserPresets::sameName(fUserNames[m.index], fPresetName);
    }

    // ---- preset list: rows, read again from the folder each time the list opens

    void buildRows()
    {
        fUserNames = TinplateUserPresets::list();
        fRows.clear();
        float y = 4.0f;
        const auto add = [&](const int kind, const int index, const float h) {
            fRows.push_back({ kind, index, y, h });
            y += h;
        };
        add(kRowHeader, 0, kMenuHeaderH);
        for (int i = 0; i < kTinplatePresetCount; ++i)
            add(kRowFactory, i, kMenuItemH);
        add(kRowHeader, 1, kMenuHeaderH + 10.0f);
        if (fUserNames.empty())
            add(kRowNone, 0, kMenuItemH);
        for (size_t j = 0; j < fUserNames.size(); ++j)
            add(kRowUser, static_cast<int>(j), kMenuItemH);
        add(kRowDivider, 0, kMenuDividerH);
        add(kRowSave, 0, kMenuItemH);
        fMenuContentH = y + 4.0f;
        fMenuScroll = std::fmin(fMenuScroll, std::fmax(fMenuContentH - menuVisibleH(), 0.0f));
    }

    static float menuTop() { return kPreset.y + kPreset.h + 4.0f; }

    float menuVisibleH() const { return std::fmin(fMenuContentH, kH - menuTop() - 8.0f); }

    Rect rowRect(const int i) const
    {
        return { kPreset.x, menuTop() + fRows[i].y - fMenuScroll, kPreset.w, fRows[i].h };
    }

    // cross that deletes a saved preset (on the right of its row)
    static Rect crossRect(const Rect& r) { return { r.x + r.w - 26.0f, r.y, 26.0f, r.h }; }

    int rowAt(const float x, const float y) const
    {
        const Rect list = { kPreset.x, menuTop(), kPreset.w, menuVisibleH() };
        if (!list.contains(x, y))
            return -1;
        for (int i = 0; i < static_cast<int>(fRows.size()); ++i)
            if (rowRect(i).contains(x, y))
                return i;
        return -1;
    }

    void openMenu()
    {
        fMenuScroll = 0.0f;
        buildRows();
        fMenuOpen = true;
        fMenuHover = fDeleteArmed = -1;
        fMenuOnCross = false;
        repaint();
    }

    void closeMenu()
    {
        fMenuOpen = false;
        fMenuHover = fDeleteArmed = -1;
        repaint();
    }

    void menuClick(const float x, const float y, const bool left)
    {
        const int row = rowAt(x, y);
        if (row < 0)
        {
            closeMenu();
            return;
        }
        if (!left)
            return;
        const MenuRow m = fRows[row];
        if (m.kind == kRowUser && crossRect(rowRect(row)).contains(x, y))
        {
            // first click arms, second click deletes (no undo)
            if (fDeleteArmed == row)
            {
                TinplateUserPresets::erase(fUserNames[m.index]);
                buildRows();
                fDeleteArmed = fMenuHover = -1;
            }
            else
                fDeleteArmed = row;
            repaint();
            return;
        }
        if (m.kind == kRowFactory)
        {
            closeMenu();
            applyPreset(m.index);
        }
        else if (m.kind == kRowUser)
        {
            const std::string name = fUserNames[m.index];
            closeMenu();
            applyUserPreset(name);
        }
        else if (m.kind == kRowSave)
        {
            closeMenu();
            startNaming();
        }
    }

    // ---- saving: the name is typed in the preset box

    void startNaming()
    {
        fUserNames = TinplateUserPresets::list();
        fNaming = true;
        fNameText = fPresetUser ? fPresetName : std::string();
        fNameError = false;
        repaint();
    }

    void cancelNaming()
    {
        fNaming = false;
        repaint();
    }

    static std::string trimmed(const std::string& s)
    {
        const size_t a = s.find_first_not_of(' ');
        if (a == std::string::npos)
            return std::string();
        return s.substr(a, s.find_last_not_of(' ') - a + 1);
    }

    void commitNaming()
    {
        const std::string name = trimmed(fNameText);
        if (!TinplateUserPresets::validName(name) || TinplateUserPresets::isFactoryName(name))
            return;   // the hint says why
        float values[kPresetParamCount];
        for (int i = 0; i < kPresetParamCount; ++i)
            values[i] = fValues[i];
        if (!TinplateUserPresets::save(name, values))
        {
            fNameError = true;
            repaint();
            return;
        }
        for (int i = 0; i < kPresetParamCount; ++i)
            fPresetValues[i] = values[i];
        fNaming = false;
        fPreset = -1;
        fPresetUser = true;
        fPresetName = name;
        repaint();
    }

    // what Enter will do with the name typed; error: Enter does nothing
    const char* namingHint(bool& error) const
    {
        const std::string name = trimmed(fNameText);
        error = true;
        if (fNameError)
            return "Could not write the preset file (folder not writable).";
        if (!name.empty() && TinplateUserPresets::isFactoryName(name))
            return "This is the name of a factory preset: choose another one.";
        if (!name.empty() && !TinplateUserPresets::validName(name))
            return "Invalid name: no dot at the end, not CON, PRN, AUX, NUL, COM1 or LPT1.";
        error = false;
        if (name.empty())
            return "Type a name. Enter: save. Esc: cancel.";
        for (const std::string& other : fUserNames)
            if (TinplateUserPresets::sameName(other, name))
                return "Enter: replace your preset of this name. Esc: cancel.";
        return "Enter: save (kept for all projects on this computer). Esc: cancel.";
    }

    bool presetModified() const
    {
        for (int i = 0; i < kPresetParamCount; ++i)
            if (std::fabs(fValues[i] - fPresetValues[i]) > 1e-4f)
                return true;
        return false;
    }

    void commitEdit(const bool apply)
    {
        if (fEdit < 0)
            return;
        if (apply && fEditText[0] != '\0')
        {
            char* end = nullptr;
            const float v = std::strtof(fEditText, &end);
            if (end != fEditText && std::isfinite(v))
            {
                const int p = kKnobs[fEdit].param;
                setOnce(p, clampValue(p, v));
            }
        }
        fEdit = -1;
        repaint();
    }

    int knobAt(const float x, const float y) const
    {
        for (int i = 0; i < kNumKnobs; ++i)
        {
            const float dx = x - kKnobs[i].cx, dy = y - kKnobs[i].cy;
            if (isFader(kKnobs[i].param))
            {
                if (std::fabs(dx) <= 16.0f && std::fabs(dy) <= kKnobs[i].r + 10.0f)
                    return i;
                continue;
            }
            const float r = kKnobs[i].r + 8.0f;
            if (dx * dx + dy * dy <= r * r)
                return i;
        }
        return -1;
    }

    // value typed with the keyboard: the number in the unit of the parameter
    void formatNumber(const int p, char* buf, const size_t size) const
    {
        const float v = fValues[p];
        const char* fmt = "%.1f";
        if (p <= kParamDamping)
            fmt = "%.3f";
        else if (p == kParamPredelay || p == kParamLowCut || p == kParamWidth || p == kParamThreshold
                 || p == kParamRelease || p == kParamFloor || p == kParamTrigger || p == kParamFade
                 || p == kParamWindow)
            fmt = "%.0f";
        std::snprintf(buf, size, fmt, v);
    }

    // decay time at 500 Hz, s (infinite when frozen)
    double decaySeconds(const double hz) const
    {
        if (frozen())
            return INFINITY;
        return TinplateRt60::seconds(fValues[kParamTail], fValues[kParamDamping], hz, sampleRate());
    }

    static void frequencyText(const double hz, char* buf, const size_t size)
    {
        if (hz <= 0.0)
            std::snprintf(buf, size, "open");
        else if (hz >= 1000.0)
            std::snprintf(buf, size, "%.1f kHz", hz / 1000.0);
        else
            std::snprintf(buf, size, "%.0f Hz", hz);
    }

    // short text under a knob; detail: the longer form of the tooltip title
    void valueText(const int p, char* buf, const size_t size, const bool detail) const
    {
        const float v = fValues[p];
        char f[24];
        switch (p)
        {
        case kParamBandwidth:
            frequencyText(TinplateLaw::onePoleCutoff(static_cast<float>(TinplateLaw::bandwidthCoef(v)), sampleRate()),
                          f, sizeof(f));
            std::snprintf(buf, size, detail ? "%.3f (-3 dB at %s)" : "%.2f  |  %s", v, f);
            break;
        case kParamDamping:
            frequencyText(TinplateLaw::onePoleCutoff(static_cast<float>(TinplateLaw::dampingCoef(v)), sampleRate()),
                          f, sizeof(f));
            std::snprintf(buf, size, detail ? "%.3f (-3 dB at %s)" : "%.2f  |  %s", v, f);
            break;
        case kParamTail:
            std::snprintf(buf, size, detail ? "%.3f (RT60 %.2f s at 500 Hz)" : "%.2f  |  %.1f s", v,
                          TinplateRt60::seconds(v, fValues[kParamDamping], 500.0, sampleRate()));
            break;
        case kParamWet:
        case kParamDry:
            if (v <= kLevelOff + 0.05f)
                std::snprintf(buf, size, "off");
            else
                std::snprintf(buf, size, "%+.1f dB", v);
            break;
        case kParamPredelay: std::snprintf(buf, size, "%.0f ms", v); break;
        case kParamLowCut:
            if (v <= kLowCutOff + 0.05f)
                std::snprintf(buf, size, "off");
            else
                std::snprintf(buf, size, "%.0f Hz", v);
            break;
        case kParamWidth:  std::snprintf(buf, size, "%.0f %%", v); break;
        case kParamOutput: std::snprintf(buf, size, "%+.1f dB", v); break;
        case kParamDuck:
            if (v < 0.05f)
                std::snprintf(buf, size, "off");
            else
                std::snprintf(buf, size, detail ? "%.1f dB (largest cut of the reverb)" : "%.1f dB", v);
            break;
        case kParamThreshold:
        case kParamFloor:
        case kParamTrigger:   std::snprintf(buf, size, "%.0f dBFS", v); break;
        case kParamFade:      std::snprintf(buf, size, "%.0f ms", v); break;
        case kParamWindow:    std::snprintf(buf, size, v < 9.95f ? "%.1f ms" : "%.0f ms", v); break;
        case kParamAttack:    std::snprintf(buf, size, v < 9.95f ? "%.1f ms" : "%.0f ms", v); break;
        case kParamRelease:
            if (v >= 999.5f)
                std::snprintf(buf, size, "%.2f s", v * 0.001f);
            else
                std::snprintf(buf, size, "%.0f ms", v);
            break;
        default:           std::snprintf(buf, size, "%.2f", v); break;
        }
    }

    static std::string dbText(const float g)
    {
        char b[24];
        if (g <= 1e-6f)
            std::snprintf(b, sizeof(b), "-inf dB");
        else
            std::snprintf(b, sizeof(b), "%.1f dB", 20.0f * std::log10(g));
        return b;
    }

    void label(const float x, const float y, const float size, const Color& c, const int align,
               const char* str, const float spacing = 0.0f)
    {
        fontFaceId(fFont);
        fontSize(size);
        textLetterSpacing(spacing);
        fillColor(c);
        textAlign(align);
        text(x, y, str, nullptr);
    }

    void panel(const Rect& r)
    {
        beginPath();
        rect(r.x - 10, r.y - 10, r.w + 20, r.h + 24);
        roundedRect(r.x, r.y, r.w, r.h, 8);
        pathWinding(CW);
        fillPaint(boxGradient(r.x, r.y + 3, r.w, r.h, 10, 14, fT->panelShadow, transparent(fT->panelShadow)));
        fill();

        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 8);
        fillPaint(linearGradient(r.x, r.y, r.x, r.y + r.h, fT->panel0, fT->panel1));
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->panelEdge);
        stroke();
    }

    // numbered section title: "1  INPUT  what it does"
    void sectionTitle(const Rect& r, const char* number, const char* title, const char* subtitle)
    {
        const float cx = r.x + 18, cy = r.y + 17;
        beginPath();
        circle(cx, cy, 8.5f);
        fillPaint(linearGradient(0, cy - 8, 0, cy + 8, fT->hot, fT->deep));
        fill();
        label(cx, cy + 0.5f, 10, fT->onAccent, ALIGN_CENTER | ALIGN_MIDDLE, number);
        label(cx + 15, cy, 11, fT->text, ALIGN_LEFT | ALIGN_MIDDLE, title, 2);
        label(r.x + r.w - 12, cy, 9, fT->faint, ALIGN_RIGHT | ALIGN_MIDDLE, subtitle, 0.5f);
    }

    void divider(const float x0, const float x1, const float y)
    {
        beginPath();
        moveTo(x0, y + 0.5f);
        lineTo(x1, y + 0.5f);
        strokeWidth(1.0f);
        strokeColor(fT->divider);
        stroke();
    }

    // small arrow in the gap between two sections: the signal goes from left to right
    void flowArrow(const float x, const float y)
    {
        beginPath();
        moveTo(x - 3, y - 7);
        lineTo(x + 4, y);
        lineTo(x - 3, y + 7);
        closePath();
        fillColor(fT->arrow);
        fill();
    }

    void button(const Rect& r, const bool on, const char* str, const float size = 11.0f)
    {
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 5);
        fillPaint(linearGradient(r.x, r.y, r.x, r.y + r.h,
                                 on ? fT->btnOn0 : fT->btnOff0,
                                 on ? fT->btnOn1 : fT->btnOff1));
        fill();
        strokeWidth(1.0f);
        strokeColor(on ? fT->btnOnEdge : fHover == hoverOf(r) ? fT->hoverEdge : fT->panelEdge);
        stroke();
        if (str)
            label(r.x + r.w * 0.5f, r.y + r.h * 0.5f, size, on ? fT->onAccent : fT->dim, ALIGN_CENTER | ALIGN_MIDDLE, str, 1);
    }

    void led(const float x, const float y, const bool on)
    {
        if (on)
        {
            beginPath();
            circle(x, y, 10);
            fillPaint(radialGradient(x, y, 2, 10, fT->ledGlow, fT->ledGlowEnd));
            fill();
        }
        beginPath();
        circle(x, y, 4.0f);
        fillColor(on ? fT->ledOn : fT->ledOff);
        fill();
    }

    void segmented(const Rect& r, const char* const* labels, const int count, const int selected,
                   const float size)
    {
        const float sw = r.w / count;
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 5);
        fillColor(fT->well);
        fill();
        strokeWidth(1.0f);
        strokeColor(fHover == hoverOf(r) ? fT->hoverEdge : fT->panelEdge);
        stroke();
        for (int i = 0; i < count; ++i)
        {
            if (i == selected)
            {
                beginPath();
                roundedRect(r.x + i * sw + 1.5f, r.y + 1.5f, sw - 3.0f, r.h - 3.0f, 4);
                fillPaint(linearGradient(0, r.y, 0, r.y + r.h, fT->sel0, fT->sel1));
                fill();
            }
            label(r.x + (i + 0.5f) * sw, r.y + r.h * 0.5f, size, i == selected ? fT->onAccent : fT->dim,
                  ALIGN_CENTER | ALIGN_MIDDLE, labels[i], 0.5f);
        }
    }

    int hoverOf(const Rect& r) const
    {
        for (int z = 0; z < kNumZones; ++z)
            if (kZones[z].r.x == r.x && kZones[z].r.y == r.y)
                return kHoverZoneBase + z;
        return -2;
    }

    // ------------------------------------------------------------------ parts

    void drawBackground()
    {
        beginPath();
        rect(0, 0, kW, kH);
        fillPaint(linearGradient(0, 0, 0, kH, fT->bg1, fT->bg0));
        fill();

        beginPath();
        rect(0, 0, kW, kH);
        fillPaint(radialGradient(kW * 0.5f, kH + 80, 40, 560, fT->bgGlow, transparent(fT->bgGlow)));
        fill();

        panel(kSecIn);
        panel(kSecTank);
        panel(kSecOut);
        panel(kSecDuck);
        panel(kSecClear);
        panel(kDecay);
        flowArrow(kSecIn.x + kSecIn.w + 5, kSecIn.y + kSecIn.h * 0.5f);
        flowArrow(kSecTank.x + kSecTank.w + 5, kSecTank.y + kSecTank.h * 0.5f);
    }

    void drawHeader()
    {
        beginPath();
        rect(0, 0, kW, 56);
        fillPaint(linearGradient(0, 0, 0, 56, fT->header0, fT->header1));
        fill();
        beginPath();
        moveTo(0, 56.5f);
        lineTo(kW, 56.5f);
        strokeColor(fT->headerLine);
        strokeWidth(1.0f);
        stroke();

        fontFaceId(fFont);
        fontSize(30);
        textLetterSpacing(6);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fontBlur(10);
        fillColor(fT->brandGlow);
        text(26, 29, "TINPLATE", nullptr);
        fontBlur(0);
        fillPaint(linearGradient(0, 14, 0, 44, fT->ledOn, fT->hot));
        text(26, 29, "TINPLATE", nullptr);
        label(246, 31, 14, fT->dim, ALIGN_LEFT | ALIGN_MIDDLE, "REVERB", 5);

        // preset drop-down box
        beginPath();
        roundedRect(kPreset.x, kPreset.y, kPreset.w, kPreset.h, 5);
        fillColor(fT->well);
        fill();
        strokeWidth(1.0f);
        strokeColor(fMenuOpen || fNaming ? fT->gold : fHover == hoverOf(kPreset) ? fT->hoverEdge : fT->panelEdge);
        stroke();
        char name[64];
        if (fNaming)
        {
            std::snprintf(name, sizeof(name), "%s_", fNameText.c_str());
            label(kPreset.x + 12, kPreset.y + kPreset.h * 0.5f, 12, fT->gold, ALIGN_LEFT | ALIGN_MIDDLE, name, 0.5f);
        }
        else
        {
            std::snprintf(name, sizeof(name), "%s%s", fPresetName.c_str(), presetModified() ? " *" : "");
            label(kPreset.x + 12, kPreset.y + kPreset.h * 0.5f, 12, fT->text, ALIGN_LEFT | ALIGN_MIDDLE, name, 0.5f);
        }
        // arrow: down when closed, up when open
        const float ax = kPreset.x + kPreset.w - 14, ay = kPreset.y + kPreset.h * 0.5f, d = fMenuOpen ? -1.0f : 1.0f;
        beginPath();
        moveTo(ax - 5, ay - 2.5f * d);
        lineTo(ax + 5, ay - 2.5f * d);
        lineTo(ax, ay + 3.5f * d);
        closePath();
        fillColor(fMenuOpen ? fT->gold : fT->dim);
        fill();

        // A / B and copy
        const char* const ab[2] = { "A", "B" };
        segmented(kAB, ab, 2, fSlot, 11);
        button(kCopy, false, fSlot == 0 ? "A>B" : "B>A", 9);

        // theme
        segmented(kTheme, kThemeNames, TinplatePrefs::kThemeCount, fTheme, 9);

        // bypass
        const bool bypassed = fValues[kParamBypass] > 0.5f;
        button(kBypass, bypassed, nullptr);
        led(kBypass.x + 16, kBypass.y + kBypass.h * 0.5f, !bypassed);
        label(kBypass.x + kBypass.w * 0.5f + 9, kBypass.y + kBypass.h * 0.5f, 11.5f, bypassed ? fT->bypassText : fT->text,
              ALIGN_CENTER | ALIGN_MIDDLE, bypassed ? "BYPASSED" : "ACTIVE", 1.8f);
    }

    void drawInputSection()
    {
        sectionTitle(kSecIn, "1", "INPUT", "into the plate");
        drawKnob(kKnobPredelay);
        divider(kSecIn.x + 12, kSecIn.x + kSecIn.w - 12, 262);
        drawKnob(kKnobBandwidth);
    }

    void drawTankSection()
    {
        const bool frozen = this->frozen();
        sectionTitle(kSecTank, "2", "PLATE", "the tank of the original");
        if (frozen)
            globalAlpha(0.35f);   // tail and damping do not act while frozen
        drawKnob(kKnobTail);
        drawKnob(kKnobDamping);
        globalAlpha(1.0f);
        divider(kSecTank.x + 12, kSecTank.x + kSecTank.w - 12, 300);

        button(kFreeze, frozen, nullptr);
        led(kFreeze.x + 16, kFreeze.y + kFreeze.h * 0.5f, frozen);
        label(kFreeze.x + kFreeze.w * 0.5f + 9, kFreeze.y + kFreeze.h * 0.5f, 11, frozen ? fT->onAccent : fT->dim,
              ALIGN_CENTER | ALIGN_MIDDLE, "FREEZE", 2);

        // decay time readout
        const float cx = kSecTank.x + kSecTank.w * 0.5f;
        label(cx, 376, 9.5f, fT->dim, ALIGN_CENTER | ALIGN_MIDDLE, "DECAY TIME (RT60) AT 500 HZ", 1.2f);
        char b[64];
        if (frozen)
            std::snprintf(b, sizeof(b), "infinite");
        else
            std::snprintf(b, sizeof(b), "%.2f s", decaySeconds(500.0));
        label(cx, 402, 22, fT->gold, ALIGN_CENTER | ALIGN_MIDDLE, b, 1);
        if (frozen)
            std::snprintf(b, sizeof(b), "frozen: the plate keeps ringing");
        else
            std::snprintf(b, sizeof(b), "125 Hz  %.2f s      4 kHz  %.2f s", decaySeconds(125.0), decaySeconds(4000.0));
        label(cx, 428, 10, fT->dim, ALIGN_CENTER | ALIGN_MIDDLE, b, 0.5f);
    }

    void drawOutputSection()
    {
        sectionTitle(kSecOut, "3", "OUTPUT", "mix and level");
        drawKnob(kKnobWidth);
        drawKnob(kKnobLowCut);
        // scale of the dry and reverb faders, between them and the knobs
        const KnobDef& f = kKnobs[kKnobDry];
        const float marks[] = { 6.0f, 0.0f, -12.0f, -24.0f, -40.0f, -60.0f };
        for (const float db : marks)
        {
            char b[8];
            std::snprintf(b, sizeof(b), db <= kLevelOff ? "-inf" : db > 0.0f ? "+%.0f" : "%.0f", db);
            label(f.cx - 15, f.cy + f.r - 2.0f * f.r * levelToNorm(db), 7.5f, fT->faint, ALIGN_RIGHT | ALIGN_MIDDLE, b);
        }
        drawKnob(kKnobDry);
        drawKnob(kKnobWet);
        drawKnob(kKnobOutput);
        drawLink();
        divider(kSecOut.x + 12, kSecOut.x + kSecOut.w - 12, 360);
        drawMeters();
    }

    void drawDuckSection()
    {
        const bool off = fValues[kParamDuck] < 0.05f;
        sectionTitle(kSecDuck, "4", "DUCKING", "");
        drawKnob(kKnobDuck);
        if (off)
            globalAlpha(0.35f);   // no effect while the depth is 0
        drawKnob(kKnobThreshold);
        drawKnob(kKnobAttack);
        drawKnob(kKnobRelease);
        globalAlpha(1.0f);

        // cut of the reverb, 0 .. 30 dB, growing from the left
        const Rect& m = kDuckMeter;
        const float cy = m.y + m.h * 0.5f;
        label(m.x - 8, cy, 9.5f, fT->text, ALIGN_RIGHT | ALIGN_MIDDLE, "CUT", 1);
        beginPath();
        roundedRect(m.x, m.y, m.w, m.h, 2);
        fillColor(fT->deepWell);
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->meterEdge);
        stroke();
        const float f = std::fmin(std::fmax(fValues[kParamMeterDuck] / 30.0f, 0.0f), 1.0f);
        if (f > 0.002f)
        {
            beginPath();
            roundedRect(m.x + 1.5f, m.y + 1.5f, (m.w - 3.0f) * f, m.h - 3.0f, 1.5f);
            fillPaint(linearGradient(m.x, 0, m.x + m.w, 0, fT->deep, fT->hot));
            fill();
        }
        beginPath();
        for (int d = 10; d < 30; d += 10)
        {
            const float x = m.x + 1.5f + (m.w - 3.0f) * d / 30.0f;
            moveTo(x + 0.5f, m.y - 2);
            lineTo(x + 0.5f, m.y + m.h + 2);
        }
        strokeColor(fT->grid);
        stroke();
        char b[16];
        std::snprintf(b, sizeof(b), fValues[kParamMeterDuck] < 0.05f ? "0.0" : "-%.1f", fValues[kParamMeterDuck]);
        label(m.x + m.w + 8, cy, 9.5f, fT->dim, ALIGN_LEFT | ALIGN_MIDDLE, b);
    }

    void drawClearSection()
    {
        const bool on = fValues[kParamClear] > 0.5f;
        sectionTitle(kSecClear, "5", "CLEAR", "a new attack empties the plate");
        button(kClearBtn, on, nullptr);
        led(kClearBtn.x + 16, kClearBtn.y + kClearBtn.h * 0.5f, on);
        label(kClearBtn.x + kClearBtn.w * 0.5f + 9, kClearBtn.y + kClearBtn.h * 0.5f, 11, on ? fT->onAccent : fT->dim,
              ALIGN_CENTER | ALIGN_MIDDLE, "CLEAR", 2);

        const int state = static_cast<int>(fValues[kParamMeterClear] + 0.5f);
        const char* text = !on ? "off" : state == 2 ? "CLEARED" : state == 1 ? "ARMED" : "waiting";
        const Color c = !on ? fT->faint : state == 2 ? fT->hot : state == 1 ? fT->gold : fT->dim;
        label(kClearState.x + kClearState.w * 0.5f, kClearState.y + kClearState.h * 0.5f, 10.5f, c,
              ALIGN_CENTER | ALIGN_MIDDLE, text, 1.5f);

        if (!on)
            globalAlpha(0.35f);   // no effect while clear is off
        drawKnob(kKnobFloor);
        drawKnob(kKnobTrigger);
        drawKnob(kKnobFade);
        drawKnob(kKnobWindow);
        globalAlpha(1.0f);
    }

    // chain between the dry and reverb faders: whole when linked, broken when not
    void drawLink()
    {
        const float cx = kLink.x + kLink.w * 0.5f, cy = kLink.y + kLink.h * 0.5f;
        const bool hover = fHover == hoverOf(kLink);
        strokeWidth(1.6f);
        strokeColor(linked() ? fT->gold : hover ? fT->text : fT->dim);
        beginPath();
        if (linked())
        {
            roundedRect(cx - 9.5f, cy - 3.5f, 11.0f, 7.0f, 3.5f);
            roundedRect(cx - 1.5f, cy - 3.5f, 11.0f, 7.0f, 3.5f);
        }
        else
        {
            roundedRect(cx - 11.0f, cy - 3.5f, 9.0f, 7.0f, 3.5f);
            roundedRect(cx + 2.0f, cy - 3.5f, 9.0f, 7.0f, 3.5f);
            moveTo(cx - 1.0f, cy - 5.0f);
            lineTo(cx - 0.2f, cy - 8.0f);
            moveTo(cx + 1.0f, cy + 5.0f);
            lineTo(cx + 0.2f, cy + 8.0f);
        }
        stroke();
    }

    // vertical fader: track, fill from the bottom to the cap, ticks, label and value below
    void drawFader(const int i)
    {
        const KnobDef& k = kKnobs[i];
        const int p = k.param;
        const bool active = i == fHover || i == fDrag || i == fEdit;
        const float n = toNorm(p, fValues[p]);
        const float top = k.cy - k.r, bottom = k.cy + k.r, y = bottom - 2.0f * k.r * n;

        beginPath();
        roundedRect(k.cx - 3.0f, top - 3.0f, 6.0f, 2.0f * k.r + 6.0f, 3.0f);
        fillColor(fT->deepWell);
        fill();
        if (n > 0.001f)
        {
            beginPath();
            roundedRect(k.cx - 3.0f, y, 6.0f, bottom + 3.0f - y, 3.0f);
            fillPaint(linearGradient(0, bottom, 0, top, fT->deep, active ? fT->gold : fT->hot));
            fill();
        }
        // ticks: 0 dB and the marks of the scale
        beginPath();
        if (p == kParamOutput)
        {
            for (float db = -24.0f; db <= 12.0f; db += 12.0f)
            {
                const float ty = bottom - 2.0f * k.r * toNorm(p, db);
                moveTo(k.cx + 8.0f, ty + 0.5f);
                lineTo(k.cx + (db == 0.0f ? 15.0f : 12.0f), ty + 0.5f);
            }
        }
        else
        {
            const float marks[] = { 6.0f, 0.0f, -12.0f, -24.0f, -40.0f };
            for (const float db : marks)
            {
                const float ty = bottom - 2.0f * k.r * levelToNorm(db);
                moveTo(k.cx - 12.0f, ty + 0.5f);
                lineTo(k.cx - (db == 0.0f ? 7.0f : 9.0f), ty + 0.5f);
            }
        }
        strokeWidth(1.0f);
        strokeColor(fT->grid);
        stroke();

        // cap
        beginPath();
        roundedRect(k.cx - 13.0f, y - 6.0f + 2.0f, 26.0f, 12.0f, 3.0f);
        fillColor(fT->knobShadow);
        fill();
        beginPath();
        roundedRect(k.cx - 13.0f, y - 6.0f, 26.0f, 12.0f, 3.0f);
        fillPaint(linearGradient(0, y - 6.0f, 0, y + 6.0f, fT->knob0, fT->knob1));
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->knobEdge);
        stroke();
        beginPath();
        moveTo(k.cx - 9.0f, y + 0.5f);
        lineTo(k.cx + 9.0f, y + 0.5f);
        strokeWidth(1.6f);
        strokeColor(active ? fT->gold : fT->pointer);
        stroke();

        char buf[48];
        valueText(p, buf, sizeof(buf), false);
        label(k.cx, bottom + 19.0f, 9.5f, fT->text, ALIGN_CENTER | ALIGN_MIDDLE, k.label, 1.0f);
        const float vy = bottom + 33.0f;
        if (i == fEdit)
        {
            const float bw = 46.0f, bx = k.cx - bw * 0.5f;
            beginPath();
            roundedRect(bx, vy - 9, bw, 18, 4);
            fillColor(fT->deepWell);
            fill();
            strokeWidth(1.0f);
            strokeColor(fT->gold);
            stroke();
            char e[40];
            std::snprintf(e, sizeof(e), "%s_", fEditText);
            label(k.cx, vy, 10.5f, fT->gold, ALIGN_CENTER | ALIGN_MIDDLE, e);
        }
        else
            label(k.cx, vy, 9.5f, active ? fT->gold : fT->dim, ALIGN_CENTER | ALIGN_MIDDLE, buf);
    }

    void drawKnob(const int i)
    {
        const KnobDef& k = kKnobs[i];
        const int p = k.param;
        if (isFader(p))
        {
            drawFader(i);
            return;
        }
        const bool active = i == fHover || i == fDrag || i == fEdit;
        const float n = toNorm(p, fValues[p]);
        const float a = kArcStart + n * kArcSweep;
        const bool big = k.r > 40.0f;
        const float track = k.r + (big ? 9.0f : 6.0f);
        const float tw = big ? 5.0f : 3.5f;

        beginPath();
        arc(k.cx, k.cy, track, kArcStart, kArcStart + kArcSweep, CW);
        lineCap(ROUND);
        strokeWidth(tw);
        strokeColor(fT->deepWell);
        stroke();

        if (n > 0.001f)
        {
            beginPath();
            arc(k.cx, k.cy, track, kArcStart, a, CW);
            strokeWidth(tw * 3.2f);
            strokeColor(active ? fT->arcGlowActive : fT->arcGlow);
            stroke();

            beginPath();
            arc(k.cx, k.cy, track, kArcStart, a, CW);
            strokeWidth(tw);
            strokePaint(linearGradient(k.cx - track, k.cy + track, k.cx + track, k.cy - track, fT->deep,
                                       active ? fT->gold : fT->hot));
            stroke();
        }

        beginPath();
        circle(k.cx, k.cy + 4, k.r + 2);
        fillPaint(radialGradient(k.cx, k.cy + 4, k.r - 4, k.r + 8, fT->knobShadow, transparent(fT->knobShadow)));
        fill();

        beginPath();
        circle(k.cx, k.cy, k.r);
        fillPaint(linearGradient(k.cx, k.cy - k.r, k.cx, k.cy + k.r, fT->knob0, fT->knob1));
        fill();
        beginPath();
        circle(k.cx, k.cy, k.r * 0.84f);
        fillPaint(radialGradient(k.cx - k.r * 0.3f, k.cy - k.r * 0.4f, 2, k.r * 1.1f,
                                 fT->knobIn0, fT->knobIn1));
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->knobEdge);
        stroke();

        const float ca = std::cos(a), sa = std::sin(a);
        beginPath();
        moveTo(k.cx + ca * k.r * 0.30f, k.cy + sa * k.r * 0.30f);
        lineTo(k.cx + ca * k.r * 0.78f, k.cy + sa * k.r * 0.78f);
        lineCap(ROUND);
        strokeWidth(big ? 4.0f : 3.0f);
        strokeColor(fT->pointerGlow);
        stroke();
        strokeWidth(big ? 2.2f : 1.8f);
        strokeColor(active ? fT->gold : fT->pointer);
        stroke();

        char buf[48];
        valueText(p, buf, sizeof(buf), false);
        const float gap = big ? 26.0f : 17.0f;
        label(k.cx, k.cy + k.r + gap, big ? 13 : 11, fT->text, ALIGN_CENTER | ALIGN_MIDDLE, k.label, big ? 3 : 1.5f);
        const float vx = k.cx, vy = k.cy + k.r + gap + (big ? 17.0f : 15.0f);

        if (i == fEdit)
        {
            const float bw = 76.0f, bx = vx - bw * 0.5f;
            beginPath();
            roundedRect(bx, vy - 10, bw, 20, 4);
            fillColor(fT->deepWell);
            fill();
            strokeWidth(1.0f);
            strokeColor(fT->gold);
            stroke();
            char e[40];
            std::snprintf(e, sizeof(e), "%s_", fEditText);
            label(bx + bw * 0.5f, vy, 12, fT->gold, ALIGN_CENTER | ALIGN_MIDDLE, e);
        }
        else
        {
            label(vx, vy, big ? 12 : 11, active ? fT->gold : fT->dim, ALIGN_CENTER | ALIGN_MIDDLE, buf);
        }
    }

    void meterBar(const int row, const char* name, const float db)
    {
        const float y = kMeterY[row] - kMeterH * 0.5f;
        label(kMeterX - 8, kMeterY[row], 9.5f, fT->text, ALIGN_RIGHT | ALIGN_MIDDLE, name, 1);
        beginPath();
        roundedRect(kMeterX, y, kMeterW, kMeterH, 2);
        fillColor(fT->deepWell);
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->meterEdge);
        stroke();

        // -60 .. +6 dBFS
        const float f = std::fmin(std::fmax((db + 60.0f) / 66.0f, 0.0f), 1.0f);
        if (f > 0.002f)
        {
            beginPath();
            roundedRect(kMeterX + 1.5f, y + 1.5f, (kMeterW - 3.0f) * f, kMeterH - 3.0f, 1.5f);
            fillPaint(linearGradient(kMeterX, 0, kMeterX + kMeterW, 0, fT->deep, row == 1 ? fT->hot : fT->gold));
            fill();
        }
        // 0 dBFS tick
        const float x0 = kMeterX + 1.5f + (kMeterW - 3.0f) * (60.0f / 66.0f);
        beginPath();
        moveTo(x0 + 0.5f, y - 2);
        lineTo(x0 + 0.5f, y + kMeterH + 2);
        strokeColor(fT->gridZero);
        stroke();

        char b[16];
        std::snprintf(b, sizeof(b), db <= -59.9f ? "-inf" : "%.1f", db);
        label(kMeterX + kMeterW + 8, kMeterY[row], 9.5f, fT->dim, ALIGN_LEFT | ALIGN_MIDDLE, b);
    }

    void drawMeters()
    {
        meterBar(0, "IN", fValues[kParamMeterIn]);
        meterBar(1, "REVERB", fValues[kParamMeterWet]);
        meterBar(2, "OUT", fValues[kParamMeterOut]);
    }

    // tone of the wet signal, dB: bandwidth low-pass and low cut (2nd-order Butterworth)
    double toneDb(const double hz) const
    {
        const double sr = sampleRate();
        double g = TinplateLaw::onePoleGain(static_cast<float>(TinplateLaw::bandwidthCoef(fValues[kParamBandwidth])),
                                            hz, sr);
        const float lc = fValues[kParamLowCut];
        if (lc > kLowCutOff)
        {
            const double r4 = std::pow(hz / lc, 4.0);
            g *= std::sqrt(r4 / (1.0 + r4));
        }
        return 20.0 * std::log10(std::fmax(g, 1e-6));
    }

    void drawDecayPlot()
    {
        const Rect& r = kPlot;
        const double sr = sampleRate();
        const double fmax = std::fmin(20000.0, 0.5 * sr);
        const auto fx = [&](const double hz) {
            return r.x + r.w * static_cast<float>(std::log(hz / 20.0) / std::log(1000.0));
        };

        // legend
        label(kDecay.x + 12, kDecay.y + 16, 10, fT->text, ALIGN_LEFT | ALIGN_MIDDLE, "DECAY TIME", 1);
        label(kDecay.x + 12, kDecay.y + 30, 10, fT->text, ALIGN_LEFT | ALIGN_MIDDLE, "AND TONE", 1);
        beginPath();
        moveTo(kDecay.x + 12, kDecay.y + 56.5f);
        lineTo(kDecay.x + 26, kDecay.y + 56.5f);
        lineCap(ROUND);
        strokeWidth(2.0f);
        strokeColor(fT->gold);
        stroke();
        label(kDecay.x + 32, kDecay.y + 56, 9.5f, fT->dim, ALIGN_LEFT | ALIGN_MIDDLE, "RT60 (s)", 0.3f);
        beginPath();
        for (float xx = kDecay.x + 12; xx < kDecay.x + 26; xx += 5.0f)
        {
            moveTo(xx, kDecay.y + 74.5f);
            lineTo(xx + 2.5f, kDecay.y + 74.5f);
        }
        strokeWidth(1.6f);
        strokeColor(fT->tone);
        stroke();
        label(kDecay.x + 32, kDecay.y + 74, 9.5f, fT->dim, ALIGN_LEFT | ALIGN_MIDDLE, "tone (dB)", 0.3f);

        beginPath();
        rect(r.x, r.y, r.w, r.h);
        fillColor(fT->plot);
        fill();

        // time scale: 2, 4, 6, 8, 10 or 12 s full scale
        double top = 0.0;
        for (int j = 0; j <= 60; ++j)
        {
            const double hz = 20.0 * std::pow(fmax / 20.0, j / 60.0);
            top = std::fmax(top, frozen() ? 0.0 : decaySeconds(hz));
        }
        const double full = frozen() ? 6.0 : top <= 1.9 ? 2.0 : top <= 3.8 ? 4.0 : top <= 5.7 ? 6.0
                          : top <= 7.6 ? 8.0 : top <= 9.5 ? 10.0 : 12.0;
        const auto ty = [&](const double s) {
            return r.y + r.h * (1.0f - static_cast<float>(std::fmin(s / full, 1.0)));
        };
        const auto dy = [&](const double d) {
            return r.y + r.h * static_cast<float>(std::fmin(std::fmax(-d / 30.0, 0.0), 1.0));
        };

        // grid: octaves, seconds
        strokeWidth(1.0f);
        beginPath();
        const double grid[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (const double hz : grid)
        {
            moveTo(fx(hz) + 0.5f, r.y);
            lineTo(fx(hz) + 0.5f, r.y + r.h);
        }
        for (double s = full / 4.0; s < full; s += full / 4.0)
        {
            moveTo(r.x, ty(s) + 0.5f);
            lineTo(r.x + r.w, ty(s) + 0.5f);
        }
        strokeColor(fT->grid);
        stroke();
        const char* const gridNames[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
        for (int j = 0; j < 8; ++j)
            label(fx(grid[j]), r.y + r.h + 8, 8, fT->faint, ALIGN_CENTER | ALIGN_MIDDLE, gridNames[j]);
        label(r.x + r.w, r.y + r.h + 8, 8, fT->faint, ALIGN_RIGHT | ALIGN_MIDDLE, "Hz");
        for (double s = 0.0; s <= full + 1e-9; s += full / 4.0)
        {
            char b[16];
            std::snprintf(b, sizeof(b), "%g", s);
            label(r.x - 5, ty(s), 8, Color(fT->gold, fT->faint, 0.4f), ALIGN_RIGHT | ALIGN_MIDDLE, b);
        }
        for (int d = 0; d <= 30; d += 10)
        {
            char b[8];
            std::snprintf(b, sizeof(b), "%d", -d);
            label(r.x + r.w + 5, dy(-d), 8, Color(fT->tone, fT->faint, 0.4f), ALIGN_LEFT | ALIGN_MIDDLE, b);
        }

        save();
        scissor(r.x, r.y - 2, r.w, r.h + 4);
        const int n = 120;
        const float xEnd = fx(fmax);
        // tone (dashed)
        {
            beginPath();
            for (int j = 0; j <= n; ++j)
            {
                const double hz = 20.0 * std::pow(fmax / 20.0, static_cast<double>(j) / n);
                const float x = fx(hz), y = dy(toneDb(hz));
                if (j % 2 == 0)
                    moveTo(x, y);
                else
                    lineTo(x, y);
            }
            lineCap(ROUND);
            strokeWidth(1.6f);
            strokeColor(fT->tone);
            stroke();
        }
        // decay time
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (int j = 0; j <= n; ++j)
            {
                const double hz = 20.0 * std::pow(fmax / 20.0, static_cast<double>(j) / n);
                const float y = frozen() ? r.y + 1.0f : ty(decaySeconds(hz));
                if (j == 0)
                    moveTo(fx(hz), y);
                else
                    lineTo(fx(hz), y);
            }
            lineCap(ROUND);
            lineJoin(ROUND);
            strokeWidth(pass == 0 ? 7.0f : 2.2f);
            strokeColor(pass == 0 ? fT->curveGlow : fT->gold);
            stroke();
        }
        restore();
        if (xEnd < r.x + r.w - 1.0f)
            label(xEnd + 6, r.y + r.h * 0.5f, 8.5f, fT->faint, ALIGN_LEFT | ALIGN_MIDDLE, "Nyquist");
        if (frozen())
            label(r.x + r.w * 0.5f, r.y + 16, 10, fT->gold, ALIGN_CENTER | ALIGN_MIDDLE, "FROZEN: infinite decay", 1);
    }

    // open preset list, drawn over everything: factory presets, a line, saved presets, a line,
    // "Save current settings..."; the current preset is marked with a dot
    void drawPresetMenu()
    {
        const float x = kPreset.x, w = kPreset.w, top = menuTop(), h = menuVisibleH();
        beginPath();
        rect(x - 10, top - 6, w + 20, h + 22);
        roundedRect(x, top, w, h, 6);
        pathWinding(CW);
        fillPaint(boxGradient(x, top + 3, w, h, 8, 12, fT->panelShadow, transparent(fT->panelShadow)));
        fill();
        beginPath();
        roundedRect(x, top, w, h, 6);
        fillColor(fT->tipBg);
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->tipEdge);
        stroke();

        save();
        scissor(x, top + 1, w, h - 2);
        for (int i = 0; i < static_cast<int>(fRows.size()); ++i)
        {
            const MenuRow& m = fRows[i];
            const Rect r = rowRect(i);
            if (r.y + r.h < top || r.y > top + h)
                continue;
            const float cy = r.y + r.h * 0.5f;
            const bool hover = i == fMenuHover;
            switch (m.kind)
            {
            case kRowHeader:
                if (m.index == 1)
                    menuLine(x, w, r.y + 5.0f);
                label(x + 12, r.y + r.h - 9.0f, 8.5f, fT->faint, ALIGN_LEFT | ALIGN_MIDDLE,
                      m.index == 0 ? "FACTORY" : "USER", 1.5f);
                break;
            case kRowDivider:
                menuLine(x, w, cy);
                break;
            case kRowNone:
                label(x + 22, cy, 11, fT->faint, ALIGN_LEFT | ALIGN_MIDDLE, "none saved yet", 0.3f);
                break;
            case kRowSave:
                if (hover)
                    menuHighlight(r);
                label(x + 12, cy, 12, hover ? fT->onAccent : fT->dim, ALIGN_LEFT | ALIGN_MIDDLE, "+", 0.0f);
                label(x + 22, cy, 12, hover ? fT->onAccent : fT->dim, ALIGN_LEFT | ALIGN_MIDDLE,
                      "Save current settings...", 0.3f);
                break;
            default:   // factory or saved preset
            {
                const bool user = m.kind == kRowUser;
                const bool armed = user && i == fDeleteArmed;
                if (hover)
                    menuHighlight(r);
                const Color c = hover ? fT->onAccent : isCurrentRow(m) ? fT->gold : fT->text;
                if (isCurrentRow(m))
                {
                    beginPath();
                    circle(x + 12, cy, 2.5f);
                    fillColor(c);
                    fill();
                }
                save();
                intersectScissor(x, r.y, w - (armed ? 70.0f : user && hover ? 28.0f : 6.0f), r.h);
                label(x + 22, cy, 12, c, ALIGN_LEFT | ALIGN_MIDDLE,
                      user ? fUserNames[m.index].c_str() : kTinplatePresets[m.index].name, 0.5f);
                restore();
                if (user && (hover || armed))
                {
                    // cross: delete (first click arms, the second deletes)
                    const Rect cr = crossRect(r);
                    const float ccx = cr.x + cr.w * 0.5f;
                    const Color xc = armed ? fT->ledOn : fMenuOnCross ? fT->onAccent : Color(fT->onAccent, fT->sel0, 0.45f);
                    if (armed)
                    {
                        beginPath();
                        roundedRect(cr.x + 3, r.y + 3, cr.w - 6, r.h - 6, 3);
                        fillColor(fT->deep);
                        fill();
                        label(cr.x - 4, cy, 10, hover ? fT->onAccent : fT->hot, ALIGN_RIGHT | ALIGN_MIDDLE, "delete?", 0.5f);
                    }
                    beginPath();
                    moveTo(ccx - 3.5f, cy - 3.5f);
                    lineTo(ccx + 3.5f, cy + 3.5f);
                    moveTo(ccx + 3.5f, cy - 3.5f);
                    lineTo(ccx - 3.5f, cy + 3.5f);
                    lineCap(ROUND);
                    strokeWidth(1.6f);
                    strokeColor(xc);
                    stroke();
                }
                break;
            }
            }
        }
        restore();

        // scroll bar when the list is longer than the window
        if (fMenuContentH > h)
        {
            const float most = fMenuContentH - h;
            const float bh = h * h / fMenuContentH;
            beginPath();
            roundedRect(x + w - 4, top + 2 + (h - 4 - bh) * (fMenuScroll / most), 2.5f, bh, 1.2f);
            fillColor(fT->dim);
            fill();
        }
    }

    void menuLine(const float x, const float w, const float y)
    {
        beginPath();
        moveTo(x + 8, y + 0.5f);
        lineTo(x + w - 8, y + 0.5f);
        strokeWidth(1.0f);
        strokeColor(fT->tipEdge);
        stroke();
    }

    void menuHighlight(const Rect& r)
    {
        beginPath();
        roundedRect(r.x + 2, r.y + 1, r.w - 4, r.h - 2, 4);
        fillPaint(linearGradient(0, r.y, 0, r.y + r.h, fT->sel0, fT->sel1));
        fill();
    }

    // under the box while a name is typed: what Enter will do
    void drawNamingHint()
    {
        bool error = false;
        const char* const hint = namingHint(error);
        const float w = 260.0f, pad = 10.0f, x = kPreset.x, y = kPreset.y + kPreset.h + 6.0f;
        fontFaceId(fFont);
        fontSize(10);
        textLetterSpacing(0);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        float bounds[4];
        textBoxBounds(0, 0, w - 2 * pad, hint, nullptr, bounds);
        const float h = 26.0f + (bounds[3] - bounds[1]) + 6.0f;
        beginPath();
        roundedRect(x, y, w, h, 6);
        fillColor(fT->tipBg);
        fill();
        strokeWidth(1.0f);
        strokeColor(error ? fT->hot : fT->tipEdge);
        stroke();
        label(x + pad, y + 13, 11, fT->gold, ALIGN_LEFT | ALIGN_MIDDLE, "SAVE PRESET", 0.5f);
        fontSize(10);
        textLetterSpacing(0);
        fillColor(error ? fT->hot : fT->text);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        textBox(x + pad, y + 24, w - 2 * pad, hint, nullptr);
    }

    void drawTooltip(const int hover)
    {
        char title[128];
        const char* help;
        Rect anchor;
        if (hover < kHoverZoneBase)
        {
            const KnobDef& d = kKnobs[hover];
            char v[80];
            valueText(d.param, v, sizeof(v), true);
            std::snprintf(title, sizeof(title), "%s  %s", d.label, v);
            help = d.help;
            const float r = d.r + 10;
            if (isFader(d.param))
                anchor = { d.cx - 20, d.cy - r, 40, 2 * r + 30 };
            else
                anchor = { d.cx - r, d.cy - r, 2 * r, 2 * r };
        }
        else
        {
            const Zone& z = kZones[hover - kHoverZoneBase];
            std::snprintf(title, sizeof(title), "%s", z.title);
            help = z.help;
            anchor = z.r;
        }

        const float w = 290.0f, pad = 10.0f;
        fontFaceId(fFont);
        fontSize(10);
        textLetterSpacing(0);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        float bounds[4];
        textBoxBounds(0, 0, w - 2 * pad, help, nullptr, bounds);
        const float h = 26.0f + (bounds[3] - bounds[1]) + 6.0f;

        float x = anchor.x + anchor.w * 0.5f - w * 0.5f;
        x = std::fmin(std::fmax(x, 6.0f), kW - w - 6.0f);
        float y = anchor.y + anchor.h + 6.0f;
        if (y + h > kH - 4.0f)
            y = anchor.y - h - 6.0f;

        beginPath();
        roundedRect(x, y, w, h, 6);
        fillColor(fT->tipBg);
        fill();
        strokeWidth(1.0f);
        strokeColor(fT->tipEdge);
        stroke();
        label(x + pad, y + 13, 11, fT->gold, ALIGN_LEFT | ALIGN_MIDDLE, title, 0.5f);
        fontSize(10);
        textLetterSpacing(0);
        fillColor(fT->text);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        textBox(x + pad, y + 24, w - 2 * pad, help, nullptr);
    }

    // ------------------------------------------------------------------ state

    float fValues[kParamCount];
    int fTheme = TinplatePrefs::kThemeSlate;
    const Theme* fT = &kThemes[TinplatePrefs::kThemeSlate];
    FontId fFont = 0;
    int fDrag = -1, fHover = -1, fEdit = -1;
    int fLastClickTarget = -1;
    uint fLastClickTime = 0;
    float fLastY = 0.0f;
    // current preset: factory (fPreset) or saved (fPresetUser), its values when loaded (for the "*")
    int fPreset = 0;
    bool fPresetUser = false;
    std::string fPresetName = kTinplatePresets[0].name;
    float fPresetValues[kPresetParamCount];
    // preset list
    bool fMenuOpen = false;
    std::vector<MenuRow> fRows;
    std::vector<std::string> fUserNames;   // saved presets, read when the list opens
    int fMenuHover = -1;                   // selectable row under the mouse (-1: none)
    bool fMenuOnCross = false;             // the mouse is on the cross of that row
    int fDeleteArmed = -1;                 // row whose cross was clicked once
    float fMenuScroll = 0.0f, fMenuContentH = 0.0f;
    // A / B sets: fSlot is live, the other is stored
    struct Slot {
        float values[kPresetParamCount];
        float presetValues[kPresetParamCount];
        int preset = 0;
        bool user = false;
        std::string name;
        bool set = false;
    };
    Slot fSlots[2];
    int fSlot = 0;
    bool fLoading = false;      // loadValues: no linking
    int fDragPartner = -1;      // linked fader moved with the one dragged
    // name of a preset being saved
    bool fNaming = false, fNameError = false;
    std::string fNameText;
    char fEditText[16] = { 0 };
    bool fEditFresh = false;
    // meters: repainted at most 30 times per second
    bool fMetersChanged = false;
    std::chrono::steady_clock::time_point fLastPaint;
#ifdef TINPLATE_UI_DEMO_PRESET
    bool fDemoPresetDone = false;
#endif
#ifdef TINPLATE_UI_DEMO_FREEZE
    bool fDemoFreezeDone = false;
#endif
#ifdef TINPLATE_UI_DEMO_CLEAR
    bool fDemoClearDone = false;
#endif
#ifdef TINPLATE_UI_DEMO_LINK
    bool fDemoLinkDone = false;
#endif
#ifdef TINPLATE_UI_DEMO_MENU
    bool fDemoMenuDone = false;
#endif
#ifdef TINPLATE_UI_DEMO_NAME
    bool fDemoNameDone = false;
#endif

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TinplateUI)
};

UI* createUI()
{
    return new TinplateUI();
}

END_NAMESPACE_DISTRHO
