// Mz808 - an emulation of the TR-808 Rhythm Composer
// Created by Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#pragma once

#include "Dsp.h"

namespace tr808
{

//
// The 808's drum oscillator. A bridged-T network in an op-amp's feedback path is a band
// pass with a high Q; kicked by a pulse it rings as a decaying, nearly-sine wave at its
// centre. The bass drum, snare, toms, congas, rim shot and claves are all built from one or
// two of these, with nothing but the kick to start them - which is why a fast repeat adds
// to a ring still going rather than restarting it, and the 808 has no machine-gun effect.
//
// Modelled as the band pass it is: centre f, and a Q that sets how long it rings. An
// amplitude time constant tau needs Q = pi f tau.
//
class BridgedT
{
public:
    void prepare(float sampleRate) { svf.prepare(sampleRate); }

    // centre in hertz, ring time constant in seconds
    void set(float hz, float tauSeconds)
    {
        f = hz;
        k = 1.0f / ((float) pi * hz * tauSeconds);
        svf.setup(f, 1.0f / k);
    }

    // with the centre moved, for this sample - a pitch sweep
    float process(float x, float hz)
    {
        svf.tune(hz, k);
        return svf.process(x);
    }

    float process(float x) { return svf.process(x); }

    float damping() const { return k; }
    float centre() const  { return f; }
    float energy() const  { return svf.energy(); }
    void reset() { svf.reset(); }

    Svf svf;

private:
    float f = 100, k = 0.01f;
};

} // namespace tr808
