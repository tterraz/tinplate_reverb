/*
 * Tinplate Reverb - the original CAPS PlateX2 (tests/caps/, unmodified CAPS 0.9.26 sources)
 * driven through its LADSPA descriptor, like a host does.  Compiled in its own translation
 * unit: the global names of CAPS (Plugin, Delay...) must not meet those of DPF.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef CAPS_REFERENCE_HPP_INCLUDED
#define CAPS_REFERENCE_HPP_INCLUDED

struct CapsPlate;

// new instance at sample rate fs (instantiate + connect_port + activate)
CapsPlate* capsPlateNew(unsigned long fs);
// one block; controls: bandwidth, tail, damping, blend
void capsPlateRun(CapsPlate* p, const float* inL, const float* inR, float* outL, float* outR,
                  unsigned long frames, const float controls[4]);
void capsPlateDelete(CapsPlate* p);

#endif
