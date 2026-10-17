// Mz808 - an emulation of the TR-808 Rhythm Composer
// Created by Simon Moscrop
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
// COW BELL: LEVEL.
//
// The bank's 540 Hz and 800 Hz squares, mixed, through a VCA and a band pass. The
// envelope has two parts: a short, loud strike, and a longer, quieter ring after it.
//
// The pair of frequencies is the circuit's; the band pass and the two decays are reasoned.
//
class Cowbell
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        strike.prepare(fs);
        strike.setDecay(0.009f);
        ring.prepare(fs);
        ring.setDecay(0.11f);
        bp.prepare(fs);
        bp.setup(2000.0f, 0.9f);
        reset();
    }

    void trigger(float height) { pulse.fire(height, fs); }

    float process(const MetalBank& bank)
    {
        const float p = pulse.next();
        const float e = 0.75f * strike.process(p) + 0.25f * ring.process(p);
        const float x = 0.5f * (bank.out[4] + bank.out[5]);
        bp.process(swingVca(x, e, 1.6f));
        return bp.bandNorm() * gain;
    }

    bool quiet() const { return ! pulse.high() && ring.value() < 1.0e-5f && bp.energy() < 1.0e-6f; }

    void reset()
    {
        strike.reset();
        ring.reset();
        bp.reset();
    }

private:
    static constexpr float gain = 1.3f;

    float fs = 48000;
    TriggerPulse pulse;
    Envelope strike, ring;
    Svf bp;
};

} // namespace tr808
