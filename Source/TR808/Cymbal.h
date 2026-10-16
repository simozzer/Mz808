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
// CYMBAL: LEVEL, TONE, DECAY.
//
// After Werner, Abel and Smith's analysis (ICMC 2014):
//
//  - The six oscillators, summed, into two band passes, centred near 3440 Hz and 7100 Hz.
//    Both throw away the fundamentals and keep the clangorous upper partials.
//  - Three bands, each through a swing-type VCA on its own envelope, then a high pass:
//    the low band rings longest, and DECAY sets how long; the middle band is medium; the
//    top band is a short splash, through a resonant third-order high pass near 10.5 kHz.
//  - TONE is a passive network whose main effect is how much of the top band gets
//    through; it also leans a little on the other two.
//
// Band-pass centres, the top band's high pass, the attack smoother and the structure are
// from the analysis. The envelope times, the other two high-pass corners, Qs and the band
// levels are reasoned from it and from the sound.
//
class Cymbal
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        bp1.prepare(fs);
        bp1.setup(3440.0f, 1.3f);
        bp2.prepare(fs);
        bp2.setup(7100.0f, 1.3f);
        hp1.prepare(fs);
        hp1.setup(2200.0f, 0.7f);
        hp2.prepare(fs);
        hp2.setup(4600.0f, 0.7f);
        hp3.prepare(fs);
        hp3.setup(10500.0f, 1.6f);
        hp3PoleCoef = onePoleCoef(tauOf(10500.0f), fs);
        env1.prepare(fs);
        env2.prepare(fs);
        env2.setDecay(0.085f);
        env3.prepare(fs);
        env3.setDecay(0.028f);
        set(5.0f, 5.0f);
        reset();
    }

    void set(float tonePanel, float decayPanel)
    {
        const float t = clamp(tonePanel / 10.0f, 0.0f, 1.0f);
        low = 1.0f - 0.55f * t;
        mid = 0.9f - 0.2f * t;
        top = 0.05f + 2.6f * t;
        env1.setDecay(expLaw(decayPanel / 10.0f, 0.12f, 0.75f));
    }

    void trigger(float height) { pulse.fire(height, fs); }

    float process(const MetalBank& bank)
    {
        const float p = pulse.next();
        const float x = bank.sum;

        bp1.process(x);
        bp2.process(x);
        const float a = bp1.bandNorm(), b = bp2.bandNorm();

        hp1.process(swingVca(a, env1.process(p), drive));
        hp2.process(swingVca(b, env2.process(p), drive));
        hp3.process(swingVca(b, env3.process(p), drive));
        const float splash = hp3Pole.highPass(hp3.hp, hp3PoleCoef);

        return (low * hp1.hp + mid * hp2.hp + top * splash) * gain;
    }

    bool quiet() const { return ! pulse.high() && env1.value() + env2.value() + env3.value() < 1.0e-5f; }

    void reset()
    {
        env1.reset();
        env2.reset();
        env3.reset();
        hp1.reset();
        hp2.reset();
        hp3.reset();
        hp3Pole.reset();
    }

private:
    static constexpr float drive = 1.6f;
    static constexpr float gain = 0.75f;

    float fs = 48000;
    TriggerPulse pulse;
    Svf bp1, bp2, hp1, hp2, hp3;
    OnePole hp3Pole;
    float hp3PoleCoef = 1;
    Envelope env1, env2, env3;
    float low = 1, mid = 1, top = 1;
};

} // namespace tr808
