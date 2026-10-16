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

namespace TomSize { enum { Low, Mid, High }; }

//
// LOW, MID and HI TOM / CONGA: LEVEL and TUNING for each pair.
//
// Each pair is one circuit: on the hardware a switch changes the bridged-T's values and
// drops the noise to turn the tom into the conga. So the two cannot sound at once - a
// conga cuts off its tom - and here, played from MIDI, the note chooses which the circuit
// is when it is struck.
//
//  - A bridged-T, kicked by the shaped trigger, whose centre starts high and sweeps down
//    as the attack's charge drains away: the 808 tom's pitch drop.
//  - Toms only: a touch of pink noise on a short envelope, which smears the attack into
//    something like a drum shell.
//  - TUNING moves the centre about a quarter of an octave either way of nominal.
//
// Nominal centres are published estimates: toms 90, 135, 185 Hz; congas 185, 250, 400 Hz.
// Ring times, sweep depth and the noise layer are reasoned, not measured.
//
class TomPair
{
public:
    explicit TomPair(int size) : size(size) {}

    void prepare(float sampleRate)
    {
        fs = sampleRate;
        res.prepare(fs);
        shaper.prepare(fs, 0.0003f);
        noiseEnv.prepare(fs);
        noiseEnv.setDecay(0.05f);
        noiseLpCoef = onePoleCoef(tauOf(1800.0f), fs);
        set(5.0f);
        reset();
        configure(false);
    }

    void set(float tuningPanel)
    {
        tune = std::pow(2.0f, (clamp(tuningPanel, 0.0f, 10.0f) - 5.0f) / 5.0f * 0.25f);
        configure(conga);
    }

    void trigger(float height, bool asConga)
    {
        if (asConga != conga) configure(asConga);
        pulse.fire(height, fs);
        sweep = height;
    }

    float process(Random& rng)
    {
        const float p = pulse.next();
        const float s = shaper.process(p);

        sweep *= sweepFall;
        const float y = res.process(drive * s, centre * (1.0f + sweepDepth * sweep));

        float shell = 0.0f;
        if (! conga)
        {
            // Paul Kellet's economy pink filter on the white noise
            const float w = rng.white();
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            const float pink = (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
            shell = noiseLp.lowPass(pink, noiseLpCoef) * noiseEnv.process(p) * shellGain;
        }

        // a kick rings louder the higher the circuit is tuned; the mixer evens them out
        static constexpr float tomGain[]   = { 1.0f, 0.72f, 0.62f };
        static constexpr float congaGain[] = { 1.15f, 0.95f, 0.75f };
        return (y + shell) * (conga ? congaGain[size] : tomGain[size]);
    }

    bool quiet() const { return ! pulse.high() && res.energy() < 1.0e-6f && noiseEnv.value() < 1.0e-5f; }

    void reset()
    {
        res.reset();
        shaper.reset();
        noiseEnv.reset();
        noiseLp.reset();
        b0 = b1 = b2 = sweep = 0.0f;
    }

private:
    void configure(bool asConga)
    {
        static constexpr float tomHz[]   = { 90.0f, 135.0f, 185.0f };
        static constexpr float congaHz[] = { 185.0f, 250.0f, 400.0f };
        static constexpr float tomTau[]   = { 0.20f, 0.16f, 0.13f };
        static constexpr float congaTau[] = { 0.09f, 0.075f, 0.06f };

        conga = asConga;
        centre = (conga ? congaHz[size] : tomHz[size]) * tune;
        res.set(centre, conga ? congaTau[size] : tomTau[size]);
        sweepDepth = conga ? 0.12f : 0.25f;
        sweepFall = decayCoef(conga ? 0.008f : 0.03f, fs);
    }

    static constexpr float drive = 1.0f;
    static constexpr float shellGain = 0.5f;

    const int size;
    float fs = 48000;
    TriggerPulse pulse;
    PulseShaper shaper;
    BridgedT res;
    Envelope noiseEnv;
    OnePole noiseLp;
    float noiseLpCoef = 1;

    bool conga = false;
    float tune = 1, centre = 100, sweep = 0, sweepDepth = 0.25f, sweepFall = 0;
    float b0 = 0, b1 = 0, b2 = 0;
};

} // namespace tr808
