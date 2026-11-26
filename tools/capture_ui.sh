#!/bin/bash
# Tinplate Reverb - screenshot of the standalone interface under Wine (test tool, not shipped).
#   tools/capture_ui.sh EXE OUT.png     (run from the root of the repository)
# EXE: standalone built with the demo flags (LISEZMOI.md, "Pour reprendre").  Waits until the
# 900x858 window is viewable and keeps the image only if it is 900x858: Wine first makes a
# temporary window of the same name, and import on a vanished window grabs the whole screen.
# Copyright (C) 2026 Tinplate authors - SPDX-License-Identifier: GPL-3.0-or-later
EXE=$1; OUT=$2
export WINEPREFIX="$PWD/build/wineprefix"
WINEDEBUG=-all wine "$EXE" >/dev/null 2>&1 &
sleep 4
OK=0
for i in $(seq 1 30); do
    W=$(xwininfo -root -tree | grep "900x858" | grep -i "tinplate" | tail -1 | awk '{print $1}')
    if [ -n "$W" ] && xwininfo -id "$W" 2>/dev/null | grep -q "IsViewable"; then
        sleep 2
        if import -window "$W" "$OUT.tmp.png" 2>/dev/null &&
           [ "$(identify -format %wx%h "$OUT.tmp.png")" = 900x858 ]; then
            mv "$OUT.tmp.png" "$OUT"; OK=1; break
        fi
    fi
    rm -f "$OUT.tmp.png"; sleep 1
done
wineserver -k 2>/dev/null; wineserver -w
[ $OK = 1 ] && echo "captured $OUT" || { echo "FAILED $OUT"; exit 1; }
