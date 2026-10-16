// Mz808 - an emulation of the TR-808 Rhythm Composer
// Copyright (C) 2026 Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#pragma once

#include "Dsp.h"

namespace tr808
{

//
// How a sound starts on the 808. The CPU raises a common trigger, whose height is the
// accent - ACCENT sets how much higher an accented step's is - and ANDs it with the
// instrument's own data line. Each voice gets a 1 ms pulse at that height and makes
// everything else from it: its envelope, its kick to the oscillator, its pitch sweep.
// So accent is not a gain at the output. It is a taller pulse into circuits that do not
// answer to it linearly, which is why an accented hit is a different sound, not just a
// louder one.
//
// Heights here are in units of the unaccented trigger. The cymbal analysis gives the
// accented common trigger as 7 to 14 V by the ACCENT pot; taking the unaccented one as the
// bottom of that, an accent is 1x to 2x. That the unaccented height equals the bottom of
// the ACCENT range is inferred, not stated.
//
inline float triggerHeight(bool accented, float accentPanel)
{
    return accented ? 1.0f + clamp(accentPanel, 0.0f, 10.0f) / 10.0f : 1.0f;
}

class TriggerPulse
{
public:
    static constexpr float width = 0.001f;   // seconds

    void fire(float h, float sampleRate)
    {
        height = h;
        remaining = (int) std::lround(width * sampleRate);
    }

    float next()
    {
        if (remaining <= 0) return 0.0f;
        --remaining;
        return height;
    }

    bool high() const { return remaining > 0; }
    float level() const { return height; }

private:
    float height = 0;
    int remaining = 0;
};

//
// The pulse shaper in front of each bridged-T: a high shelf that passes the pulse's
// rising edge whole and a fraction of its flat top, then a diode that shaves off most of
// the swing below ground on the falling edge. The edges are what kick the resonator, so
// shaving the falling one keeps it from cancelling the rising one's kick.
//
class PulseShaper
{
public:
    void prepare(float sampleRate, float tauSeconds = 0.0004f)
    {
        coef = onePoleCoef(tauSeconds, sampleRate);
    }

    float process(float p)
    {
        const float settled = rc.lowPass(p, coef);
        const float s = (p - settled) + flatTop * settled;
        return s >= 0.0f ? s : s * diodeLeak;
    }

    void reset() { rc.reset(); }

private:
    static constexpr float flatTop = 0.3f;     // how much of the top gets through
    static constexpr float diodeLeak = 0.15f;  // how much below ground does

    OnePole rc;
    float coef = 1;
};

//
// A trigger-driven envelope generator: a capacitor charged through a diode by the
// (slightly smoothed) pulse while it is high, then discharged through a resistor. So it
// rises over the pulse's millisecond, holds its top, and falls away as an RC curve -
// and a new hit charges it from wherever it had fallen to rather than from zero.
//
class Envelope
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        attackCoef = onePoleCoef(0.0001f, fs);   // the attack smoother, ~0.1 ms (cymbal analysis)
        setDecay(0.1f);
    }

    void setDecay(float tauSeconds) { fall = decayCoef(tauSeconds, fs); }

    float process(float pulse)
    {
        smoothed += (pulse - smoothed) * attackCoef;
        v = smoothed > v ? smoothed : v * fall;
        return v;
    }

    float value() const { return v; }
    void reset() { v = smoothed = 0.0f; }

private:
    float fs = 48000, attackCoef = 1, fall = 0;
    float smoothed = 0, v = 0;
};

} // namespace tr808
