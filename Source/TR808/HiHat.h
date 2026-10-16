// Mz808 - an emulation of the TR-808 Rhythm Composer
// Copyright (C) 2026 Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#pragma once

#include "Metal.h"
#include "Trigger.h"

namespace tr808
{

//
// OPEN HIHAT: LEVEL, DECAY. CLOSED HIHAT: LEVEL.
//
// The six oscillators again, through a band pass high in the cymbal's range, a swing-type
// VCA and a high pass. Open and closed share the metal and the VCA, and differ in how fast
// the VCA's envelope is let go: the open hat's through DECAY, the closed hat's through a
// short fixed resistor. So a closed hat on top of an open one chokes it, as on the
// hardware - one envelope, discharged the fast way.
//
// Each has its own LEVEL after the VCA; the closed hat's sets the level of the whole sound
// from the moment it chokes. The structure is the circuit's; the filters and times are
// reasoned.
//
class HiHat
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        bp.prepare(fs);
        bp.setup(7100.0f, 1.4f);
        hp.prepare(fs);
        hp.setup(7000.0f, 0.9f);
        env.prepare(fs);
        set(4.0f);
        reset();
    }

    void set(float openDecayPanel)
    {
        openTau = expLaw(openDecayPanel / 10.0f, 0.045f, 0.45f);
        if (open) env.setDecay(openTau);
    }

    void trigger(float height, bool asOpen)
    {
        open = asOpen;
        env.setDecay(open ? openTau : closedTau);
        pulse.fire(height, fs);
    }

    bool isOpen() const { return open; }

    float process(const MetalBank& bank)
    {
        const float p = pulse.next();
        bp.process(bank.sum);
        hp.process(swingVca(bp.bandNorm(), env.process(p), 1.8f));
        return hp.hp * gain;
    }

    bool quiet() const { return ! pulse.high() && env.value() < 1.0e-5f; }

    void reset()
    {
        bp.reset();
        hp.reset();
        env.reset();
    }

private:
    static constexpr float closedTau = 0.013f;
    static constexpr float gain = 1.3f;

    float fs = 48000;
    TriggerPulse pulse;
    Svf bp, hp;
    Envelope env;
    float openTau = 0.15f;
    bool open = false;
};

} // namespace tr808
