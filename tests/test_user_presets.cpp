/*
 * Tinplate Reverb - tests of the presets saved by the user.
 * Run with HOME / XDG_CONFIG_HOME (Linux) or APPDATA (Windows) pointing to an empty folder.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "TinplateUserPresets.hpp"

#include <clocale>
#include <cstdio>

using namespace TinplateUserPresets;

static int failures = 0;

static void check(const bool ok, const char* const what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}

static bool sameValues(const float* const a, const float* const b)
{
    for (int i = 0; i < kPresetParamCount; ++i)
        if (a[i] != b[i])
            return false;
    return true;
}

int main()
{
    // a host may set a locale with a decimal comma: the files must not depend on it
    const bool comma = std::setlocale(LC_ALL, "fr_FR.UTF-8") != nullptr;
    std::printf("locale with a decimal comma: %s\n", comma ? "yes" : "not available");

    check(list().empty(), "no preset at first");

    check(validName("My master 2"), "valid name");
    check(!validName("") && !validName(" lead") && !validName("lead ") && !validName("lead."),
          "empty name, leading or trailing space or dot refused");
    check(!validName("a/b") && !validName("a:b") && !validName("a?") && !validName("tab\tname"),
          "forbidden characters refused");
    check(!validName("CON") && !validName("com1") && !validName("nul.txt") && validName("Console"),
          "reserved Windows names refused");
    check(!validName("abcdefghijklmnopqrstuvwxy"), "name longer than 24 characters refused");
    check(!save("Default", kTinplatePresets[0].values) && !save("vocal PLATE", kTinplatePresets[0].values),
          "factory names refused");

    const float a[kPresetParamCount] = { 0.8123457f, 0.55f, 0.3f, 137.5f, 150.0f, 2.5f, -16.25f, -3.5f, 1.0f, 0.375f, 37.5f, 143.7f, 8500.0f, 125.0f,
                                         -1.75f, 9.5f, -27.25f, 3.3f, 812.0f, 1.0f, -55.5f, -12.0f, 80.0f,
                                         35.0f, 1.0f, 120.0f, 1.0f, 9.0f,
                                         62.5f, 0.375f, 0.8125f, 0.6f, 175.0f };
    check(save("Bright", a), "save");
    float r[kPresetParamCount] = { 0 };
    check(load("Bright", r) && sameValues(a, r), "read back exactly");

    const float b[kPresetParamCount] = { 0, 0, 0, 50, 0, 0.1f, -60, -60, 0, 0, 0, 20, 1000, 0, -24, 0, -60, 1, 50, 0, -80, -60, 10, 1, 0, 20, 0, 0,
                                         0, 0, 0, 0, 50 };
    check(save("aggressive", b) && save("Calm", kTinplatePresets[2].values), "save two more");
    const std::vector<std::string> names = list();
    check(names.size() == 3 && names[0] == "aggressive" && names[1] == "Bright" && names[2] == "Calm",
          "list sorted without case");

    check(save("bright", b) && list().size() == 3 && list()[1] == "bright" && load("bright", r)
          && sameValues(b, r), "same name with another case replaces the preset");

    check(erase("Calm") && list().size() == 2 && !load("Calm", r), "delete");
    check(!erase("Calm"), "delete a missing preset fails");

    // hand-written file: missing keys take the default, unknown lines are ignored, values are
    // clamped, CRLF ends of line
    std::FILE* const f = openPreset("Hand", true);
    check(f != nullptr, "open for writing");
    if (f != nullptr)
    {
        std::fputs("# comment\r\nvolume=11\r\nlowcut=5000\r\nwidth=40.5\r\ntail=x\r\n", f);
        std::fclose(f);
    }
    bool ok = load("Hand", r) && r[kParamLowCut] == 1000.0f && r[kParamWidth] == 40.5f;
    for (int i = 0; i < kPresetParamCount; ++i)
        if (i != kParamLowCut && i != kParamWidth)
            ok = ok && r[i] == kTinplateRanges[i].def;
    check(ok, "missing keys: default, unknown lines ignored, clamped");


    // files that are not presets are not listed
#ifdef _WIN32
    std::FILE* const g = _wfopen((folder() + L"\\notes.txt").c_str(), L"wb");
#else
    std::FILE* const g = std::fopen((folder() + "/notes.txt").c_str(), "wb");
#endif
    if (g != nullptr)
        std::fclose(g);
    check(list().size() == 3, "other files ignored");

    std::printf("%s\n", failures == 0 ? "all user preset tests passed" : "USER PRESET TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
