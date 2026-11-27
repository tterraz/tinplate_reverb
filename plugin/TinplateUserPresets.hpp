/*
 * Tinplate Reverb - presets saved by the user, shared by all instances, projects and hosts
 * (a setting of the computer, like the theme; the host only sees the factory presets).
 *
 * Folder: presets/ next to tinplate.conf (TinplatePrefs.hpp), one file "<name>.preset" per
 * preset, "key=value" lines (keys of kUserPresetKeys).  Numbers are written and read by streams
 * in the classic locale: the same file whatever the locale of the host (decimal point).  Missing keys take the
 * default value, unknown lines are ignored, values are clamped to their range.
 * Names: printable ASCII, no character forbidden in a Windows file name, 1..kMaxNameLength
 * characters, no leading or trailing space or dot, not a reserved Windows device name.
 * Only the UI reads and writes these files.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TINPLATE_USER_PRESETS_HPP_INCLUDED
#define TINPLATE_USER_PRESETS_HPP_INCLUDED

#include "TinplateParams.hpp"
#include "TinplatePrefs.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
# include <io.h>   // _wfindfirst
#else
# include <dirent.h>
#endif

namespace TinplateUserPresets {

static const size_t kMaxNameLength = 24;

// file keys of the preset parameters kParamBandwidth .. kParamSmooth (symbols of the parameters)
static const char* const kUserPresetKeys[kPresetParamCount] = {
    "bandwidth", "tail", "damping", "size", "depth", "rate", "wet", "dry", "link", "mix", "predelay", "lowcut", "highcut", "width", "output",
    "duck", "threshold", "attack", "release", "clear", "floor", "trigger", "fade", "window",
    "stereo", "monobass", "sync", "note",
    "split", "bandwidth_b", "tail_b", "damping_b", "size_b", "smooth",
};

static inline bool validName(const std::string& name)
{
    if (name.empty() || name.size() > kMaxNameLength)
        return false;
    for (const char c : name)
        if (c < 32 || c > 126 || std::strchr("<>:\"/\\|?*", c) != nullptr)
            return false;
    if (name.front() == ' ' || name.back() == ' ' || name.front() == '.' || name.back() == '.')
        return false;
    // reserved device names of Windows (also with an extension, case-insensitive)
    std::string base = name.substr(0, name.find('.'));
    while (!base.empty() && base.back() == ' ')
        base.pop_back();
    for (char& c : base)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL")
        return false;
    if (base.size() == 4 && (base.compare(0, 3, "COM") == 0 || base.compare(0, 3, "LPT") == 0)
        && base[3] >= '0' && base[3] <= '9')
        return false;
    return true;
}

// names are compared without case: a file system may not tell "Loud" from "loud"
static inline bool sameName(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

static inline bool isFactoryName(const std::string& name)
{
    for (int i = 0; i < kTinplatePresetCount; ++i)
        if (sameName(name, kTinplatePresets[i].name))
            return true;
    return false;
}

static const char kExtension[] = ".preset";
static const size_t kExtensionLength = sizeof(kExtension) - 1;

#ifdef _WIN32
// names are ASCII: widened character by character
static inline TinplatePrefs::Path widen(const std::string& s)
{
    return TinplatePrefs::Path(s.begin(), s.end());
}

// folder of the presets, created if needed; empty when there is no place for it
static inline TinplatePrefs::Path folder()
{
    const TinplatePrefs::Path base = TinplatePrefs::folder();
    if (base.empty())
        return base;
    const TinplatePrefs::Path dir = base + L"\\presets";
    _wmkdir(dir.c_str());
    return dir;
}

static inline std::FILE* openPreset(const std::string& name, const bool write)
{
    const TinplatePrefs::Path dir = folder();
    if (dir.empty())
        return nullptr;
    return _wfopen((dir + L"\\" + widen(name) + L".preset").c_str(), write ? L"wb" : L"rb");
}

static inline bool removeFile(const std::string& name)
{
    const TinplatePrefs::Path dir = folder();
    return !dir.empty() && _wremove((dir + L"\\" + widen(name) + L".preset").c_str()) == 0;
}

static inline std::vector<std::string> fileNames()
{
    std::vector<std::string> names;
    const TinplatePrefs::Path dir = folder();
    if (dir.empty())
        return names;
    _wfinddata_t data;
    const intptr_t h = _wfindfirst((dir + L"\\*.preset").c_str(), &data);
    if (h == -1)
        return names;
    do
    {
        std::string name;
        bool ascii = true;
        for (const wchar_t* c = data.name; *c != L'\0'; ++c)
        {
            if (*c > 126)
                ascii = false;
            name.push_back(static_cast<char>(*c));
        }
        if (ascii && !(data.attrib & _A_SUBDIR))
            names.push_back(name);
    } while (_wfindnext(h, &data) == 0);
    _findclose(h);
    return names;
}
#else
static inline TinplatePrefs::Path folder()
{
    const TinplatePrefs::Path base = TinplatePrefs::folder();
    if (base.empty())
        return base;
    const TinplatePrefs::Path dir = base + "/presets";
    mkdir(dir.c_str(), 0755);
    return dir;
}

static inline std::FILE* openPreset(const std::string& name, const bool write)
{
    const TinplatePrefs::Path dir = folder();
    if (dir.empty())
        return nullptr;
    return std::fopen((dir + "/" + name + kExtension).c_str(), write ? "wb" : "rb");
}

static inline bool removeFile(const std::string& name)
{
    const TinplatePrefs::Path dir = folder();
    return !dir.empty() && std::remove((dir + "/" + name + kExtension).c_str()) == 0;
}

static inline std::vector<std::string> fileNames()
{
    std::vector<std::string> names;
    const TinplatePrefs::Path dir = folder();
    if (dir.empty())
        return names;
    DIR* const d = opendir(dir.c_str());
    if (d == nullptr)
        return names;
    while (const dirent* const e = readdir(d))
        names.push_back(e->d_name);
    closedir(d);
    return names;
}
#endif

// names of the saved presets, sorted without case; files with an invalid name are skipped
static inline std::vector<std::string> list()
{
    std::vector<std::string> names;
    for (const std::string& file : fileNames())
    {
        if (file.size() <= kExtensionLength
            || file.compare(file.size() - kExtensionLength, kExtensionLength, kExtension) != 0)
            continue;
        const std::string name = file.substr(0, file.size() - kExtensionLength);
        if (validName(name))
            names.push_back(name);
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        {
            const int ca = std::tolower(static_cast<unsigned char>(a[i]));
            const int cb = std::tolower(static_cast<unsigned char>(b[i]));
            if (ca != cb)
                return ca < cb;
        }
        return a.size() < b.size();
    });
    return names;
}

// reads a preset; false if the file cannot be read (values then unchanged)
static inline bool load(const std::string& name, float values[kPresetParamCount])
{
    if (!validName(name))
        return false;
    std::FILE* const f = openPreset(name, false);
    if (f == nullptr)
        return false;
    for (int i = 0; i < kPresetParamCount; ++i)
        values[i] = kTinplateRanges[i].def;
    char line[256];
    while (std::fgets(line, sizeof(line), f) != nullptr)
    {
        line[std::strcspn(line, "\r\n")] = '\0';
        const char* const eq = std::strchr(line, '=');
        if (eq == nullptr)
            continue;
        const size_t keyLength = static_cast<size_t>(eq - line);
        for (int i = 0; i < kPresetParamCount; ++i)
        {
            if (std::strlen(kUserPresetKeys[i]) != keyLength || std::strncmp(line, kUserPresetKeys[i], keyLength) != 0)
                continue;
            std::istringstream in(eq + 1);
            in.imbue(std::locale::classic());
            float v = 0.0f;
            in >> v;
            if (!in.fail() && (in.eof() || in.peek() == std::char_traits<char>::eof()) && std::isfinite(v))
            {
                const TinplateRange& r = kTinplateRanges[i];
                values[i] = std::fmin(std::fmax(v, r.min), r.max);
            }
        }
    }
    std::fclose(f);
    return true;
}

// fewest digits (6 to 9) that read back to the same float, in the classic locale
static inline std::string number(const float v)
{
    std::string text;
    for (int digits = 6; digits <= std::numeric_limits<float>::max_digits10; ++digits)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out.precision(digits);
        out << v;
        text = out.str();
        std::istringstream in(text);
        in.imbue(std::locale::classic());
        float back = 0.0f;
        in >> back;
        if (back == v)
            break;
    }
    return text;
}

// writes a preset (replaces a preset of the same name); false if the name is invalid, is the
// name of a factory preset, or the file cannot be written
static inline bool save(const std::string& name, const float values[kPresetParamCount])
{
    if (!validName(name) || isFactoryName(name))
        return false;
    // a preset whose name differs only by case is replaced, not duplicated
    for (const std::string& other : list())
        if (sameName(other, name) && other != name)
            removeFile(other);
    std::FILE* const f = openPreset(name, true);
    if (f == nullptr)
        return false;
    bool ok = std::fputs("# Tinplate Reverb preset\nversion=1\n", f) >= 0;
    for (int i = 0; i < kPresetParamCount; ++i)
    {
        ok = ok && std::fprintf(f, "%s=%s\n", kUserPresetKeys[i], number(values[i]).c_str()) > 0;
    }
    return std::fclose(f) == 0 && ok;
}

static inline bool erase(const std::string& name)
{
    return validName(name) && removeFile(name);
}

}   // namespace TinplateUserPresets

#endif
