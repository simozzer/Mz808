// Mz808 - an emulation of the TR-808 Rhythm Composer
// Copyright (C) 2026 Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#pragma once

#include "BridgedT.h"
#include "Trigger.h"

namespace tr808
{

//
// RIM SHOT / CLAVES: one LEVEL.
//
// One circuit, switched, so each cuts the other off.
//
//  - CLAVES: a single bridged-T at about 2.5 kHz, kicked and left to ring, and passed to
//    the output unchanged.
//  - RIM SHOT: two bridged-Ts, about 455 Hz and 1667 Hz, kicked together. Their sum goes
//    through a transistor VCA on a very short envelope, overdriven, then a high pass. The
//    hard edge where the VCA clips is the crack.
//
// The three centres are published estimates; ring times, the VCA's drive and the high
// pass are reasoned.
//
class RimShot
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        shaper.prepare(fs, 0.0002f);
        rimLow.prepare(fs);
        rimHigh.prepare(fs);
        clave.prepare(fs);
        rimLow.set(455.0f, 0.008f);
        rimHigh.set(1667.0f, 0.005f);
        clave.set(2500.0f, 0.011f);
        vcaEnv.prepare(fs);
        vcaEnv.setDecay(0.0045f);
        hp.prepare(fs);
        hp.setup(320.0f, 0.7f);
        reset();
    }

    void trigger(float height, bool asClaves)
    {
        claves = asClaves;
        pulse.fire(height, fs);
    }

    float process()
    {
        const float p = pulse.next();
        const float s = shaper.process(p);

        if (claves)
        {
            rimLow.process(0.0f);
            rimHigh.process(0.0f);
            return clave.process(claveDrive * s) * claveGain;
        }

        clave.process(0.0f);
        const float ring = rimLow.process(s) + 1.4f * rimHigh.process(s);
        const float e = vcaEnv.process(p);
        const float cracked = e * softClip(ring * rimDrive);
        hp.process(cracked);
        return hp.hp * rimGain;
    }

    bool quiet() const
    {
        return ! pulse.high() && rimLow.energy() + rimHigh.energy() + clave.energy() < 1.0e-6f
            && vcaEnv.value() < 1.0e-5f;
    }

    void reset()
    {
        shaper.reset();
        rimLow.reset();
        rimHigh.reset();
        clave.reset();
        vcaEnv.reset();
        hp.reset();
    }

private:
    static constexpr float rimDrive = 2.5f;
    static constexpr float rimGain = 0.75f;
    static constexpr float claveDrive = 1.0f;
    static constexpr float claveGain = 0.85f;

    float fs = 48000;
    bool claves = false;
    TriggerPulse pulse;
    PulseShaper shaper;
    BridgedT rimLow, rimHigh, clave;
    Envelope vcaEnv;
    Svf hp;
};

} // namespace tr808
