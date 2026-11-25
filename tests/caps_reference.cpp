/*
 * Tinplate Reverb - see caps_reference.hpp.
 *
 * Copyright (C) 2026 Tinplate authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "caps/Reverb.cc"
#include "caps_reference.hpp"

struct CapsPlate
{
    Descriptor<PlateX2> descriptor { 1795 };
    LADSPA_Handle handle = nullptr;
    float controls[4] = { 0, 0, 0, 0 };
};

CapsPlate* capsPlateNew(const unsigned long fs)
{
    CapsPlate* const p = new CapsPlate;
    p->handle = p->descriptor.instantiate(&p->descriptor, fs);
    for (unsigned long i = 0; i < 4; ++i)
        p->descriptor.connect_port(p->handle, i, &p->controls[i]);
    p->descriptor.activate(p->handle);
    return p;
}

void capsPlateRun(CapsPlate* const p, const float* inL, const float* inR, float* outL, float* outR,
                  const unsigned long frames, const float controls[4])
{
    for (int i = 0; i < 4; ++i)
        p->controls[i] = controls[i];
    p->descriptor.connect_port(p->handle, 4, const_cast<float*>(inL));
    p->descriptor.connect_port(p->handle, 5, const_cast<float*>(inR));
    p->descriptor.connect_port(p->handle, 6, outL);
    p->descriptor.connect_port(p->handle, 7, outR);
    p->descriptor.run(p->handle, frames);
}

void capsPlateDelete(CapsPlate* const p)
{
    p->descriptor.cleanup(p->handle);
    delete p;
}
