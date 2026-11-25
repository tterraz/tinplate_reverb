/*
 * Tinplate Reverb - preferences of the user, shared by all instances and all projects
 * (not saved with the project): the theme of the interface.
 *
 * File: Windows %APPDATA%\Tinplate\tinplate.conf, otherwise $XDG_CONFIG_HOME/tinplate/tinplate.conf
 * (~/.config/tinplate/tinplate.conf).  One "key=value" per line; unknown lines are ignored,
 * a missing or unreadable file gives the defaults.  Only the UI reads and writes it.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TINPLATE_PREFS_HPP_INCLUDED
#define TINPLATE_PREFS_HPP_INCLUDED

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
# include <direct.h>   // _wmkdir (no windows.h: keeps the UI free of its macros)
#else
# include <sys/stat.h>
#endif

namespace TinplatePrefs {

enum Theme { kThemeSlate = 0, kThemeTin = 1, kThemeCount };

static const char* const kThemeKeys[kThemeCount] = { "slate", "tin" };

#ifdef _WIN32
typedef std::wstring Path;

// folder of the file, created if needed; empty when there is no place for it
static inline Path folder()
{
    const wchar_t* const appdata = _wgetenv(L"APPDATA");
    if (appdata == nullptr || appdata[0] == L'\0')
        return Path();
    const Path dir = Path(appdata) + L"\\Tinplate";
    _wmkdir(dir.c_str());   // fails harmlessly if it exists
    return dir;
}

static inline std::FILE* openFile(const char* const mode)
{
    const Path dir = folder();
    if (dir.empty())
        return nullptr;
    const Path file = dir + L"\\tinplate.conf";
    return _wfopen(file.c_str(), mode[0] == 'w' ? L"wb" : L"rb");
}
#else
typedef std::string Path;

static inline Path folder()
{
    Path base;
    const char* const xdg = std::getenv("XDG_CONFIG_HOME");
    const char* const home = std::getenv("HOME");
    if (xdg != nullptr && xdg[0] == '/')
        base = xdg;
    else if (home != nullptr && home[0] != '\0')
    {
        base = Path(home) + "/.config";
        mkdir(base.c_str(), 0755);
    }
    else
        return Path();
    const Path dir = base + "/tinplate";
    mkdir(dir.c_str(), 0755);   // fails harmlessly if it exists
    return dir;
}

static inline std::FILE* openFile(const char* const mode)
{
    const Path dir = folder();
    if (dir.empty())
        return nullptr;
    return std::fopen((dir + "/tinplate.conf").c_str(), mode[0] == 'w' ? "wb" : "rb");
}
#endif

// theme saved by the user (kThemeSlate when there is none)
static inline int loadTheme()
{
    int theme = kThemeSlate;
    std::FILE* const f = openFile("r");
    if (f == nullptr)
        return theme;
    char line[256];
    while (std::fgets(line, sizeof(line), f) != nullptr)
    {
        line[std::strcspn(line, "\r\n")] = '\0';
        if (std::strncmp(line, "theme=", 6) != 0)
            continue;
        for (int t = 0; t < kThemeCount; ++t)
            if (std::strcmp(line + 6, kThemeKeys[t]) == 0)
                theme = t;
    }
    std::fclose(f);
    return theme;
}

// saves the theme; returns false if the file cannot be written (the choice then lasts until
// the window is closed)
static inline bool saveTheme(const int theme)
{
    if (theme < 0 || theme >= kThemeCount)
        return false;
    std::FILE* const f = openFile("w");
    if (f == nullptr)
        return false;
    const bool ok = std::fprintf(f, "theme=%s\n", kThemeKeys[theme]) > 0;
    return std::fclose(f) == 0 && ok;
}

}   // namespace TinplatePrefs

#endif
