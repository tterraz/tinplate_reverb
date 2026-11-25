#!/bin/sh
# Tinplate Reverb - Steinberg VST3 validator (test tool, not shipped).
#   tools/validate_vst3.sh WORKDIR          Linux validator on bin/linux/*.vst3
#   tools/validate_vst3.sh WORKDIR windows  MinGW validator, run with wine on bin/windows/*.vst3
# WORKDIR receives the VST3 SDK (MIT) and its builds.  Expected: "47 tests passed, 0 tests failed".
# Copyright (C) 2026 Tinplate authors - SPDX-License-Identifier: GPL-3.0-or-later
set -e
W=${1:?usage: validate_vst3.sh WORKDIR [windows]}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$W"
W=$(cd "$W" && pwd)
cd "$W"
if [ ! -d sdk ]; then
    git clone -q --depth 1 https://github.com/steinbergmedia/vst3sdk.git sdk
    git -C sdk submodule update -q --init --depth 1 base pluginterfaces public.sdk cmake
fi
OPTS="-DCMAKE_BUILD_TYPE=Release -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ADD_VSTGUI=OFF \
      -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF \
      -DSMTG_RUN_VST_VALIDATOR=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF"

if [ "$2" = windows ]; then
    # the SDK detects the platform before project(): force Windows when cross-compiling
    F=sdk/cmake/modules/SMTG_DetectPlatform.cmake
    grep -q 'CMAKE_SYSTEM_NAME STREQUAL "Windows"' $F || python3 - "$F" <<'PY'
import sys
p = sys.argv[1]; s = open(p, newline='').read(); i = s.index('if(APPLE)')
s = s[:i] + 'if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")\n        set(SMTG_WIN TRUE CACHE INTERNAL ${SMTG_PLATFORM_DETECTION_COMMENT})\n    else' + s[i:]
open(p, 'w', newline='').write(s)
PY
    cmake -S sdk -B build-win $OPTS -DCMAKE_SYSTEM_NAME=Windows \
          -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix \
          -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres -DCMAKE_EXE_LINKER_FLAGS="-static -municode" >/dev/null
    cmake --build build-win --target validator -j"${JOBS:-7}" >/dev/null
    # own wine prefix in WORKDIR: never the user's ~/.wine
    WINEDEBUG=-all WINEPREFIX="${WINEPREFIX:-$W/wineprefix}" wine build-win/bin/validator.exe "Z:$HERE/bin/windows/tinplate_reverb.vst3" | grep -a "Result:\|ERROR"
else
    cmake -S sdk -B build-linux $OPTS >/dev/null
    cmake --build build-linux --target validator -j"${JOBS:-7}" >/dev/null
    build-linux/bin/Release/validator "$HERE/bin/linux/tinplate_reverb.vst3" | grep "Result:\|ERROR"
fi
