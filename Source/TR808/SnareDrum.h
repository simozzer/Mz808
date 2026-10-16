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
// SNARE DRUM: LEVEL, TONE, SNAPPY.
//
// The drum head is two bridged-Ts, about 238 Hz and 476 Hz, kicked together by the shaped
// trigger; the upper one dies sooner. TONE crossfades between them - low and hollow at 0,
// higher and tighter at 10. The snares are the machine's white noise, high-passed and
// through a VCA on its own envelope; SNAPPY is how much of that goes in.
//
// The two centres are published estimates; the ring times, the noise filter and its
// envelope are reasoned from the sound and the circuit's structure, not measured.
//
class SnareDrum
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        low.prepare(fs);
        high.prepare(fs);
        low.set(238.0f, 0.045f);
        high.set(476.0f, 0.025f);
        shaper.prepare(fs, 0.0003f);
        noiseHp.prepare(fs);
        noiseHp.setup(1800.0f, 0.7f);
        noiseLpCoef = onePoleCoef(tauOf(9000.0f), fs);
        noiseEnv.prepare(fs);
        noiseEnv.setDecay(0.055f);
        set(5.0f, 5.0f);
        reset();
    }

    void set(float tonePanel, float snappyPanel)
    {
        const float t = clamp(tonePanel / 10.0f, 0.0f, 1.0f);
        lowMix = 1.0f - 0.75f * t;
        highMix = 0.25f + 0.75f * t;
        snappy = clamp(snappyPanel / 10.0f, 0.0f, 1.0f);
    }

    void trigger(float height) { pulse.fire(height, fs); }

    float process(Random& rng)
    {
        const float p = pulse.next();
        const float s = shaper.process(p);

        const float head = lowMix * low.process(drive * s) + highMix * high.process(drive * s);

        noiseHp.process(rng.white());
        const float hiss = noiseLp.lowPass(noiseHp.hp, noiseLpCoef);
        const float e = noiseEnv.process(p);
        const float snares = softClip(hiss * e * 0.9f);   // the VCA, pushed by an accent

        return (head + snappy * snareGain * snares) * gain;
    }

    bool quiet() const
    {
        return ! pulse.high() && low.energy() + high.energy() < 1.0e-6f && noiseEnv.value() < 1.0e-5f;
    }

    void reset()
    {
        low.reset();
        high.reset();
        shaper.reset();
        noiseHp.reset();
        noiseLp.reset();
        noiseEnv.reset();
    }

private:
    static constexpr float drive = 1.0f;
    static constexpr float snareGain = 0.55f;
    static constexpr float gain = 0.9f;

    float fs = 48000;
    TriggerPulse pulse;
    PulseShaper shaper;
    BridgedT low, high;
    Svf noiseHp;
    OnePole noiseLp;
    float noiseLpCoef = 1;
    Envelope noiseEnv;

    float lowMix = 1, highMix = 0, snappy = 0.5f;
};

} // namespace tr808
