/*
 * Tinplate Reverb - tests of the preferences file (theme of the interface).
 * Run with HOME / XDG_CONFIG_HOME (Linux) or APPDATA (Windows) pointing to an empty folder.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "TinplatePrefs.hpp"

#include <cstdio>

using namespace TinplatePrefs;

static int failures = 0;

static void check(const bool ok, const char* const what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}

int main()
{
    check(loadTheme() == kThemeSlate, "no file: Slate");
    check(saveTheme(kThemeTin), "save Tin");
    check(loadTheme() == kThemeTin, "read back Tin");
    check(saveTheme(kThemeSlate) && loadTheme() == kThemeSlate, "save and read back Slate");
    check(!saveTheme(kThemeCount), "invalid theme refused");

    // unknown keys and values are ignored, the last valid line wins
    std::FILE* const f = openFile("w");
    check(f != nullptr, "open for writing");
    if (f != nullptr)
    {
        std::fputs("# comment\r\nvolume=11\r\ntheme=tin\r\ntheme=purple\r\n", f);
        std::fclose(f);
    }
    check(loadTheme() == kThemeTin, "unknown lines ignored (CRLF)");

    std::printf("%s\n", failures == 0 ? "all prefs tests passed" : "PREFS TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
