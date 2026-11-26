#!/usr/bin/env python3
"""Tinplate Reverb - figures of the user manual (doc/manuel.tex).

python3 doc/figures.py  ->  doc/fig_*.pdf

Control laws and filters: the formulas of plugin/TinplateParams.hpp and TinplatePlugin.cpp,
written again here in NumPy.  Decay times: the table plugin/Rt60Table.hpp, read and interpolated
as plugin/Rt60.hpp does.  Impulse responses: the plate itself (plugin/PlateCore.hpp), through
tools/impulse.cpp, compiled into build/ by this script (g++).
Copyright (C) 2026 Tinplate authors - SPDX-License-Identifier: GPL-3.0-or-later
"""
import os
import re
import subprocess

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BRASS, STEEL, DARK, GREY, RUST = "#966919", "#196e96", "#1e262d", "#8c969e", "#b5532a"
plt.rcParams.update({"font.size": 9, "axes.grid": True, "grid.alpha": 0.3, "axes.spines.top": False,
                     "axes.spines.right": False, "legend.frameon": False, "figure.dpi": 150})


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def save(fig, name):
    fig.tight_layout()
    fig.savefig(os.path.join(HERE, name))
    plt.close(fig)


# ---------------------------------------------------------------------------------- control laws

def bandwidth_coef(p):
    return np.exp(-np.pi * (1.0 - (0.005 + 0.994 * np.asarray(p))))


def damping_coef(p):
    return np.exp(-np.pi * (0.0005 + 0.9995 * np.asarray(p)))


def wet(p):
    return np.asarray(p) ** 1.53


def one_pole_gain(a, f, fs):
    p, w = 1.0 - a, 2.0 * np.pi * np.asarray(f) / fs
    return a / np.sqrt(1.0 - 2.0 * p * np.cos(w) + p * p)


def one_pole_cutoff(a, fs):
    """-3 dB frequency; NaN when the gain stays above -3 dB up to Nyquist."""
    a = np.asarray(a, dtype=float)
    p = 1.0 - a
    with np.errstate(divide="ignore", invalid="ignore"):
        c = (1.0 + p * p - 2.0 * a * a) / (2.0 * p)
        f = np.arccos(np.minimum(c, 1.0)) * fs / (2.0 * np.pi)
    return np.where((p > 1e-12) & (c > -1.0), f, np.nan)


def low_cut_gain(hz, f, fs):
    """2nd-order Butterworth high-pass (RBJ) of the plugin, magnitude at f."""
    w0 = 2.0 * np.pi * min(hz, 0.45 * fs) / fs
    cw, alpha = np.cos(w0), np.sin(w0) / (2.0 * np.sqrt(0.5))
    a0 = 1.0 + alpha
    b = np.array([(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]) / a0
    a = np.array([1.0, -2 * cw / a0, (1 - alpha) / a0])
    z = np.exp(-1j * 2.0 * np.pi * np.asarray(f) / fs)
    return np.abs((b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z))


# ---------------------------------------------------------------------------------- RT60 table

def rt60_table():
    text = open(os.path.join(ROOT, "plugin", "Rt60Table.hpp")).read()
    body = text[text.index("kSeconds"):]
    values = [float(v) for v in re.findall(r"(\d+\.\d+)f", body)]
    sizes = int(re.search(r"kSizes = (\d+)", text).group(1))
    tails = int(re.search(r"kTails = (\d+)", text).group(1))
    step = float(re.search(r"kTailStep = ([0-9.]+)", text).group(1))
    first = float(re.search(r"kFirstSize = ([0-9.]+)", text).group(1))
    size_step = float(re.search(r"kSizeStep = ([0-9.]+)", text).group(1))
    return np.array(values).reshape(sizes, tails, 9, 9), step, first, size_step


TABLE, TAIL_STEP, FIRST_SIZE, SIZE_STEP = rt60_table()


def rt60(tail, damping, f, fs=44100.0, size=1.0):
    """plugin/Rt60.hpp: linear interpolation in Size, Tail, Damping, octaves from 62.5 Hz."""
    def split(x, last):
        x = np.clip(x, 0.0, last)
        i = np.minimum(np.floor(x).astype(int), last - 1)
        return i, x - i
    f = np.asarray(f, dtype=float)
    iz, wz = split(np.full_like(f, (size - FIRST_SIZE) / SIZE_STEP), TABLE.shape[0] - 1)
    it, wt = split(np.full_like(f, tail / TAIL_STEP), TABLE.shape[1] - 1)
    idd, wd = split(np.full_like(f, damping * 8), 8)
    jf, wf = split(np.log2(f * 44100.0 / fs / 62.5), 8)
    v = np.zeros_like(f)
    for e in (0, 1):
        for a in (0, 1):
            for b in (0, 1):
                for c in (0, 1):
                    v += ((wz if e else 1 - wz) * (wt if a else 1 - wt) * (wd if b else 1 - wd)
                          * (wf if c else 1 - wf) * TABLE[iz + e, it + a, idd + b, jf + c])
    return v


# ---------------------------------------------------------------------------------- the plate

def impulse(bandwidth, tail, damping, seconds, sr=44100.0):
    exe = os.path.join(ROOT, "build", "impulse")
    src = os.path.join(ROOT, "tools", "impulse.cpp")
    if not os.path.exists(exe) or os.path.getmtime(exe) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(exe), exist_ok=True)
        subprocess.run(["g++", "-O2", "-std=gnu++17", "-I" + os.path.join(ROOT, "plugin"), src, "-o", exe],
                       check=True)
    raw = subprocess.run([exe, "%g" % sr, "%g" % seconds, "%g" % bandwidth, "%g" % tail, "%g" % damping],
                         check=True, capture_output=True).stdout
    ir = np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(float)
    return ir[:, 0], ir[:, 1]


# ---------------------------------------------------------------------------------- figures

def fig_lois():
    p = np.linspace(0.0, 1.0, 401)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.6, 3.0))
    for fs, ls in ((44100.0, "-"), (96000.0, "--")):
        k = "%g kHz" % (fs / 1000)
        ax1.semilogy(p, one_pole_cutoff(bandwidth_coef(p), fs), ls, color=STEEL, label="Bandwidth, " + k)
        ax1.semilogy(p, one_pole_cutoff(damping_coef(p), fs), ls, color=BRASS, label="Damping, " + k)
    ax1.set_xlabel("valeur du bouton")
    ax1.set_ylabel("fréquence à -3 dB (Hz)")
    ax1.set_ylim(5, 50000)
    ax1.legend(fontsize=7, loc="lower center", ncol=2)
    ax1.set_title("Passe-bas d'entrée et d'amortissement", fontsize=9)

    # faders Dry and Reverb: piecewise linear law of the interface (plugin/TinplateUI.cpp)
    law_db, law_pos = [-60, -40, -24, -12, 0, 6], [0, 0.15, 0.35, 0.55, 0.82, 1.0]
    ax2.plot(p, np.interp(p, law_pos, law_db), color=BRASS)
    ax2.set_xlabel("position du fader (0 en bas, 1 en haut)")
    ax2.set_ylabel("niveau (dB)")
    ax2.set_ylim(-62, 8)
    ax2.set_yticks([-60, -40, -24, -12, 0, 6])
    ax2.set_yticklabels(["off", "-40", "-24", "-12", "0", "+6"])
    ax2.set_title("Faders Dry et Reverb", fontsize=9)
    save(fig, "fig_lois.pdf")


def fig_rt60():
    f = np.geomspace(62.5, 16000.0, 200)
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(9.6, 3.0), sharey=True)
    cmap = plt.get_cmap("copper")
    for i, t in enumerate((0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.16)):
        ax1.semilogx(f, rt60(t, 0.4, f), color=cmap(0.15 + 0.7 * i / 6), label="Tail %g" % t)
    ax1.set_title("Damping 0,4", fontsize=9)
    for i, d in enumerate((0.0, 0.25, 0.5, 0.75, 1.0)):
        ax2.semilogx(f, rt60(0.6, d, f), color=plt.get_cmap("Blues")(0.35 + 0.6 * i / 4), label="Damping %.2f" % d)
    ax2.set_title("Tail 0,6", fontsize=9)
    for i, z in enumerate((0.5, 1.0, 1.5, 2.0)):
        ax3.semilogx(f, rt60(0.6, 0.4, f, size=z), color=plt.get_cmap("Greens")(0.35 + 0.6 * i / 3),
                     label="Size %d %%" % round(100 * z))
    ax3.set_title("Tail 0,6, Damping 0,4", fontsize=9)
    for ax in (ax1, ax2, ax3):
        ax.set_xlabel("fréquence (Hz), à 44,1 kHz")
        ax.set_xticks([62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000])
        ax.set_xticklabels(["62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"])
        ax.legend(fontsize=7, ncol=2)
    ax1.set_ylabel("RT60 (s)")
    save(fig, "fig_rt60.pdf")


def fig_reponse():
    sr = 44100.0
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7.2, 4.6))
    l, r = impulse(0.8, 0.55, 0.4, 0.25, sr)
    t = np.arange(len(l)) / sr * 1000.0
    ax1.plot(t, l, color=STEEL, lw=0.6, label="gauche")
    ax1.plot(t, r - 0.3, color=BRASS, lw=0.6, label="droite (décalée de -0,3)")
    ax1.set_xlim(0, 250)
    ax1.set_ylim(-0.48, 0.2)
    ax1.set_xlabel("temps (ms)")
    ax1.set_ylabel("amplitude")
    ax1.legend(fontsize=7.5, loc="upper right", ncol=2)
    ax1.set_title("Début de la réponse impulsionnelle (préréglage Default, sans pré-retard)", fontsize=9)

    for tail, col in ((0.25, STEEL), (0.55, BRASS), (0.85, RUST)):
        l, r = impulse(0.8, tail, 0.4, 8.0, sr)
        e = (l + r) ** 2
        edc = np.cumsum(e[::-1])[::-1]
        edc = 10.0 * np.log10(edc / edc[0] + 1e-30)
        tt = np.arange(len(e)) / sr
        ax2.plot(tt, edc, color=col, label="Tail %.2f (RT60 à 500 Hz : %.1f s)" % (tail, rt60(tail, 0.4, 500.0)))
    ax2.set_xlim(0, 6)
    ax2.set_ylim(-70, 2)
    ax2.set_xlabel("temps (s)")
    ax2.set_ylabel("énergie restante (dB)")
    ax2.legend(fontsize=7.5)
    ax2.set_title("Décroissance de l'énergie (intégrale de Schroeder, L + R ; Bandwidth 0,8, Damping 0,4)",
                  fontsize=9)
    save(fig, "fig_reponse.pdf")


def duck_cut(x, fs, threshold, depth, attack_ms, release_ms):
    """Cut of the reverb (dB) by the ducking, sample by sample, as TinplatePlugin.cpp."""
    aa = np.exp(-1000.0 / (attack_ms * fs))
    ar = np.exp(-1000.0 / (release_ms * fs))
    level = db(x)
    target = np.clip(level - threshold, 0.0, depth)
    y, out = 0.0, np.zeros_like(x)
    for i, t in enumerate(target):
        y = t + (aa if t > y else ar) * (y - t)
        if t == 0.0 and y < 1e-3:
            y = 0.0
        out[i] = y
    return out


def fig_duck():
    fs = 44100.0
    t = np.arange(int(3.0 * fs)) / fs
    # three phrases of a 220 Hz tone at -6 dBFS peak, with gaps of 0.1, 0.4 and 1 s
    on = ((t >= 0.1) & (t < 0.7)) | ((t >= 0.8) & (t < 1.4)) | ((t >= 1.8) & (t < 2.0))
    x = np.where(on, 0.5 * np.sin(2.0 * np.pi * 220.0 * t), 0.0)
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7.0, 3.8), sharex=True, gridspec_kw={"height_ratios": [1, 1.6]})
    ax1.plot(t, x, color=GREY, lw=0.4)
    ax1.set_ylabel("entrée")
    ax1.set_ylim(-0.6, 0.6)
    for rel, col in ((100.0, STEEL), (400.0, BRASS), (1500.0, RUST)):
        ax2.plot(t, -duck_cut(x, fs, -30.0, 12.0, 10.0, rel), color=col, label="Release %g ms" % rel)
    ax2.set_ylabel("gain de la réverbération (dB)")
    ax2.set_xlabel("temps (s)")
    ax2.set_ylim(-13, 1)
    ax2.legend(fontsize=7.5, loc="lower right")
    ax2.set_title("Duck 12 dB, Threshold -30 dBFS, Attack 10 ms ; entrée à -6 dBFS", fontsize=9)
    save(fig, "fig_duck.pdf")


def fig_ton():
    fs = 44100.0
    f = np.geomspace(20.0, 20000.0, 400)
    fig, ax = plt.subplots(figsize=(6.0, 3.0))
    for p, ls in ((1.0, "-"), (0.8, "--"), (0.5, ":")):
        ax.semilogx(f, db(one_pole_gain(bandwidth_coef(p), f, fs)), ls, color=STEEL, label="Bandwidth %.1f" % p)
    for hz, ls in ((60.0, "-"), (150.0, "--"), (500.0, ":")):
        ax.semilogx(f, db(low_cut_gain(hz, f, fs)), ls, color=BRASS, label="Low cut %g Hz" % hz)
    ax.set_ylim(-30, 2)
    ax.set_xlim(20, 20000)
    ax.set_xlabel("fréquence (Hz), à 44,1 kHz")
    ax.set_ylabel("gain (dB)")
    ax.legend(fontsize=7.5, ncol=2, loc="lower center")
    save(fig, "fig_ton.pdf")


if __name__ == "__main__":
    fig_lois()
    fig_rt60()
    fig_reponse()
    fig_ton()
    fig_duck()
