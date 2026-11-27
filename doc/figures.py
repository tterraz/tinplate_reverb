#!/usr/bin/env python3
"""Tinplate Reverb - figures of the user manual (doc/manuel.tex).

python3 doc/figures.py  ->  doc/fig_*.pdf (French, manuel.tex) and doc/fig_*_en.pdf (English, manual.tex)

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


LANG = "fr"   # set by main: "fr" or "en"


def T(fr, en):
    """Text of the figures in the language being drawn."""
    return fr if LANG == "fr" else en


def num(x, fmt="%g"):
    """A number with the decimal separator of the language."""
    text = fmt % x
    return text.replace(".", ",") if LANG == "fr" else text


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def save(fig, name):
    if LANG != "fr":
        name = name.replace(".pdf", "_%s.pdf" % LANG)
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

def impulse(bandwidth, tail, damping, seconds, sr=44100.0, left=False, smooth=0.0):
    exe = os.path.join(ROOT, "build", "impulse")
    src = os.path.join(ROOT, "tools", "impulse.cpp")
    deps = [src] + [os.path.join(ROOT, "plugin", h) for h in ("PlateCore.hpp", "Smooth.hpp", "Rt60SmoothTable.hpp")]
    if not os.path.exists(exe) or os.path.getmtime(exe) < max(os.path.getmtime(d) for d in deps):
        os.makedirs(os.path.dirname(exe), exist_ok=True)
        subprocess.run(["g++", "-O2", "-std=gnu++17", "-I" + os.path.join(ROOT, "plugin"), src, "-o", exe],
                       check=True)
    args = [exe, "%g" % sr, "%g" % seconds, "%g" % bandwidth, "%g" % tail, "%g" % damping] + (["left"] if left else [])
    args += ["smooth=%g" % smooth] if smooth > 0.0 else []
    raw = subprocess.run(args, check=True, capture_output=True).stdout
    ir = np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(float)
    return ir[:, 0], ir[:, 1]


# ---------------------------------------------------------------------------------- figures

def fig_lois():
    p = np.linspace(0.0, 1.0, 401)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.6, 3.0))
    for fs, ls in ((44100.0, "-"), (96000.0, "--")):
        k = num(fs / 1000) + " kHz"
        ax1.semilogy(p, one_pole_cutoff(bandwidth_coef(p), fs), ls, color=STEEL, label="Bandwidth, " + k)
        ax1.semilogy(p, one_pole_cutoff(damping_coef(p), fs), ls, color=BRASS, label="Damping, " + k)
    ax1.set_xlabel(T("valeur du bouton", "knob value"))
    ax1.set_ylabel(T("fréquence à -3 dB (Hz)", "-3 dB frequency (Hz)"))
    ax1.set_ylim(5, 50000)
    ax1.legend(fontsize=7, loc="lower center", ncol=2)
    ax1.set_title(T("Passe-bas d'entrée et d'amortissement", "Input and damping low-passes"), fontsize=9)

    # faders Dry and Reverb: piecewise linear law of the interface (plugin/TinplateUI.cpp)
    law_db, law_pos = [-60, -40, -24, -12, 0, 6], [0, 0.15, 0.35, 0.55, 0.82, 1.0]
    ax2.plot(p, np.interp(p, law_pos, law_db), color=BRASS)
    ax2.set_xlabel(T("position du fader (0 en bas, 1 en haut)", "fader position (0 bottom, 1 top)"))
    ax2.set_ylabel(T("niveau (dB)", "level (dB)"))
    ax2.set_ylim(-62, 8)
    ax2.set_yticks([-60, -40, -24, -12, 0, 6])
    ax2.set_yticklabels(["off", "-40", "-24", "-12", "0", "+6"])
    ax2.set_title(T("Faders Dry et Reverb", "Dry and Reverb faders"), fontsize=9)
    save(fig, "fig_lois.pdf")


def fig_rt60():
    f = np.geomspace(62.5, 16000.0, 200)
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(9.6, 3.0), sharey=True)
    cmap = plt.get_cmap("copper")
    for i, t in enumerate((0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.16)):
        ax1.semilogx(f, rt60(t, 0.4, f), color=cmap(0.15 + 0.7 * i / 6), label="Tail " + num(t))
    ax1.set_title("Damping " + num(0.4), fontsize=9)
    for i, d in enumerate((0.0, 0.25, 0.5, 0.75, 1.0)):
        ax2.semilogx(f, rt60(0.6, d, f), color=plt.get_cmap("Blues")(0.35 + 0.6 * i / 4), label="Damping " + num(d, "%.2f"))
    ax2.set_title("Tail " + num(0.6), fontsize=9)
    for i, z in enumerate((0.5, 1.0, 1.5, 2.0)):
        ax3.semilogx(f, rt60(0.6, 0.4, f, size=z), color=plt.get_cmap("Greens")(0.35 + 0.6 * i / 3),
                     label="Size %d %%" % round(100 * z))
    ax3.set_title("Tail %s, Damping %s" % (num(0.6), num(0.4)), fontsize=9)
    for ax in (ax1, ax2, ax3):
        ax.set_xlabel(T("fréquence (Hz), à 44,1 kHz", "frequency (Hz), at 44.1 kHz"))
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
    ax1.plot(t, l, color=STEEL, lw=0.6, label=T("gauche", "left"))
    ax1.plot(t, r - 0.3, color=BRASS, lw=0.6, label=T("droite (décalée de -0,3)", "right (offset by -0.3)"))
    ax1.set_xlim(0, 250)
    ax1.set_ylim(-0.48, 0.2)
    ax1.set_xlabel(T("temps (ms)", "time (ms)"))
    ax1.set_ylabel("amplitude")
    ax1.legend(fontsize=7.5, loc="upper right", ncol=2)
    ax1.set_title(T("Début de la réponse impulsionnelle (préréglage Default, sans pré-retard)",
                    "Start of the impulse response (Default preset, no pre-delay)"), fontsize=9)

    for tail, col in ((0.25, STEEL), (0.55, BRASS), (0.85, RUST)):
        l, r = impulse(0.8, tail, 0.4, 8.0, sr)
        e = (l + r) ** 2
        edc = np.cumsum(e[::-1])[::-1]
        edc = 10.0 * np.log10(edc / edc[0] + 1e-30)
        tt = np.arange(len(e)) / sr
        ax2.plot(tt, edc, color=col, label=T("Tail %s (RT60 à 500 Hz : %s s)", "Tail %s (RT60 at 500 Hz: %s s)")
                 % (num(tail, "%.2f"), num(rt60(tail, 0.4, 500.0), "%.1f")))
    ax2.set_xlim(0, 6)
    ax2.set_ylim(-70, 2)
    ax2.set_xlabel(T("temps (s)", "time (s)"))
    ax2.set_ylabel(T("énergie restante (dB)", "remaining energy (dB)"))
    ax2.legend(fontsize=7.5)
    ax2.set_title(T("Décroissance de l'énergie (intégrale de Schroeder, L + R ; Bandwidth 0,8, Damping 0,4)",
                    "Energy decay (Schroeder integral, L + R; Bandwidth 0.8, Damping 0.4)"), fontsize=9)
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
    ax1.set_ylabel(T("entrée", "input"))
    ax1.set_ylim(-0.6, 0.6)
    for rel, col in ((100.0, STEEL), (400.0, BRASS), (1500.0, RUST)):
        ax2.plot(t, -duck_cut(x, fs, -30.0, 12.0, 10.0, rel), color=col, label="Release %g ms" % rel)
    ax2.set_ylabel(T("gain de la réverbération (dB)", "gain of the reverb (dB)"))
    ax2.set_xlabel(T("temps (s)", "time (s)"))
    ax2.set_ylim(-13, 1)
    ax2.legend(fontsize=7.5, loc="lower right")
    ax2.set_title(T("Duck 12 dB, Threshold -30 dBFS, Attack 10 ms ; entrée à -6 dBFS",
                    "Duck 12 dB, Threshold -30 dBFS, Attack 10 ms; input at -6 dBFS"), fontsize=9)
    save(fig, "fig_duck.pdf")


def high_cut_gain(hz, f, fs):
    """2nd-order Butterworth low-pass (RBJ) of the plugin, magnitude at f."""
    w0 = 2.0 * np.pi * min(hz, 0.45 * fs) / fs
    cw, alpha = np.cos(w0), np.sin(w0) / (2.0 * np.sqrt(0.5))
    a0 = 1.0 + alpha
    b = np.array([(1 - cw) / 2, 1 - cw, (1 - cw) / 2]) / a0
    a = np.array([1.0, -2 * cw / a0, (1 - alpha) / a0])
    z = np.exp(-1j * 2.0 * np.pi * np.asarray(f) / fs)
    return np.abs((b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z))


def fig_ton():
    fs = 44100.0
    f = np.geomspace(20.0, 20000.0, 400)
    fig, ax = plt.subplots(figsize=(6.0, 3.0))
    for p, ls in ((1.0, "-"), (0.8, "--"), (0.5, ":")):
        ax.semilogx(f, db(one_pole_gain(bandwidth_coef(p), f, fs)), ls, color=STEEL, label="Bandwidth " + num(p, "%.1f"))
    for hz, ls in ((60.0, "-"), (150.0, "--"), (500.0, ":")):
        ax.semilogx(f, db(low_cut_gain(hz, f, fs)), ls, color=BRASS, label="Low cut %g Hz" % hz)
    for hz, ls in ((8000.0, "-"), (3000.0, "--")):
        ax.semilogx(f, db(high_cut_gain(hz, f, fs)), ls, color=RUST, label="High cut %s kHz" % num(hz / 1000))
    ax.set_ylim(-30, 2)
    ax.set_xlim(20, 20000)
    ax.set_xlabel(T("fréquence (Hz), à 44,1 kHz", "frequency (Hz), at 44.1 kHz"))
    ax.set_ylabel("gain (dB)")
    ax.legend(fontsize=7.5, ncol=2, loc="lower center")
    save(fig, "fig_ton.pdf")


def fig_stereo():
    """Left-right balance of the reverb of an impulse on the left: mono input, stereo input."""
    sr = 44100.0
    win = int(0.02 * sr)
    fig, ax = plt.subplots(figsize=(6.4, 2.8))
    for left, col, name in ((False, GREY, T("entrée mono (l'original)", "mono input (the original)")),
                            (True, STEEL, T("entrée stéréo", "stereo input"))):
        l, r = impulse(0.8, 0.55, 0.4, 1.5, sr, left)
        n = len(l) // win
        el = (l[:n * win] ** 2).reshape(n, win).sum(axis=1)
        er = (r[:n * win] ** 2).reshape(n, win).sum(axis=1)
        t = (np.arange(n) + 0.5) * win / sr
        bal = 10.0 * np.log10((el + 1e-30) / (er + 1e-30))
        ax.plot(t, np.clip(bal, -15, 15), color=col, marker="o", ms=2.5, lw=1.2, label=name)
    ax.axhline(0.0, color=DARK, lw=0.6)
    ax.set_xlim(0, 1.5)
    ax.set_ylim(-6, 15.5)
    ax.set_xlabel(T("temps (s)", "time (s)"))
    ax.set_ylabel(T("gauche - droite (dB)", "left - right (dB)"))
    ax.set_title(T("Impulsion à gauche : équilibre de la réverbération par tranches de 20 ms",
                   "Impulse on the left: balance of the reverb in slices of 20 ms"), fontsize=9)
    ax.legend(fontsize=7.5, loc="upper right")
    save(fig, "fig_stereo.pdf")


def fig_smooth():
    """Level of the tail, Smooth 0 and 100 %, and the spectrum of the waves of its level."""
    sr = 44100.0
    win = int(0.005 * sr)
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7.2, 4.6))
    for smooth, col, name in ((0.0, GREY, T("Smooth 0 (l'original)", "Smooth 0 (the original)")),
                              (1.0, STEEL, "Smooth 100 %")):
        l, r = impulse(0.8, 0.9, 0.2, 10.0, sr, smooth=smooth)
        s = l + r
        n = len(s) // win
        lev = 10.0 * np.log10((s[:n * win] ** 2).reshape(n, win).mean(axis=1) + 1e-300)
        lev -= lev[20:40].mean()
        t = (np.arange(n) + 0.5) * win / sr
        ax1.plot(t, lev, color=col, lw=0.6, label=name)
        # waves of the level, as tools/ripple_measure.hpp: segments of 1 s between -40 and -80 dB,
        # level minus a quadratic fit, Hann window; mean power spectrum
        seg, spec = 200, []
        for a in range(50, n - seg, 50):
            m = lev[a:a + seg].mean()
            if m > -40.0 or m < -80.0:
                continue
            tt = np.linspace(-1.0, 1.0, seg)
            y = lev[a:a + seg] - np.polyval(np.polyfit(tt, lev[a:a + seg], 2), tt)
            spec.append(np.abs(np.fft.rfft(y * np.hanning(seg))) ** 2)
        f = np.fft.rfftfreq(seg, 0.005)
        ax2.plot(f, 10.0 * np.log10(np.mean(spec, axis=0)), color=col, lw=1.0, label=name)
    ax1.set_xlim(0, 6)
    ax1.set_ylim(-100, 5)
    ax1.set_xlabel(T("temps (s)", "time (s)"))
    ax1.set_ylabel(T("niveau (dB)", "level (dB)"))
    ax1.legend(fontsize=7.5)
    ax1.set_title(T("Niveau de la réponse impulsionnelle par tranches de 5 ms (Bandwidth 0,8, Tail 0,9, Damping 0,2)",
                    "Level of the impulse response in slices of 5 ms (Bandwidth 0.8, Tail 0.9, Damping 0.2)"),
                  fontsize=9)
    ax2.set_xlim(0, 90)
    ax2.set_xlabel(T("fréquence de l'ondulation du niveau (Hz)", "frequency of the waves of the level (Hz)"))
    ax2.set_ylabel(T("puissance (dB)", "power (dB)"))
    ax2.legend(fontsize=7.5, loc="lower left", ncol=2)
    ax2.set_title(T("Ondulation du niveau entre -40 et -80 dB : spectre moyen", "Waves of the level between -40 and "
                    "-80 dB: mean spectrum"), fontsize=9)
    save(fig, "fig_smooth.pdf")


if __name__ == "__main__":
    for LANG in ("fr", "en"):
        fig_lois()
        fig_rt60()
        fig_reponse()
        fig_ton()
        fig_duck()
        fig_stereo()
        fig_smooth()
