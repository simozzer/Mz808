// Mz808 - an emulation of the TR-808 Rhythm Composer
// Copyright (C) 2026 Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#pragma once

#include "Trigger.h"

namespace tr808
{

//
// HAND CLAP / MARACAS: one LEVEL.
//
// One circuit, switched, so each cuts the other off.
//
//  - HAND CLAP: white noise through a band pass near 1 kHz, into a VCA driven by two
//    envelopes at once. A sawtooth generator gives a burst of quick decays a few
//    milliseconds apart - several hands, not quite together - and a slower envelope gives
//    the tail after them, the "reverb".
//  - MARACAS: white noise, high-passed, with an envelope that rises over a couple of
//    milliseconds before it falls - a shake rather than a hit.
//
// The structure is the circuit's; the number of claps, their spacing, the filter and the
// times are reasoned from the sound, not measured.
//
class HandClap
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        clapBp.prepare(fs);
        clapBp.setup(1000.0f, 2.2f);
        maracasHp.prepare(fs);
        maracasHp.setup(5000.0f, 0.7f);
        toothFall = decayCoef(0.0028f, fs);
        tailRise = decayCoef(0.006f, fs);
        tailFall = decayCoef(0.075f, fs);
        shakeRise = decayCoef(0.0015f, fs);
        shakeFall = decayCoef(0.012f, fs);
        spacing = (int) std::lround(0.0105f * fs);
        reset();
    }

    void trigger(float height, bool asMaracas)
    {
        maracas = asMaracas;
        h = height;
        if (maracas)
        {
            fast = slow = height;
            tooth = 0.0f;
            teethLeft = 0;
        }
        else
        {
            tooth = height;
            teethLeft = claps - 1;
            timer = spacing;
            fast = slow = height;
        }
    }

    float process(Random& rng)
    {
        const float w = rng.white();

        if (maracas)
        {
            fast *= shakeRise;
            slow *= shakeFall;
            maracasHp.process(w);
            clapBp.process(0.0f);
            return maracasHp.hp * (slow - fast) * maracasGain;
        }

        tooth *= toothFall;
        if (teethLeft > 0 && --timer <= 0)
        {
            tooth = h;
            --teethLeft;
            timer = spacing;
        }

        fast *= tailRise;
        slow *= tailFall;
        const float tail = (slow - fast) * tailLevel;

        clapBp.process(w);
        maracasHp.process(0.0f);
        return softClip(clapBp.bandNorm() * (tooth + tail) * clapDrive) * clapGain;
    }

    bool quiet() const { return teethLeft == 0 && tooth < 1.0e-5f && slow < 1.0e-5f; }

    void reset()
    {
        clapBp.reset();
        maracasHp.reset();
        tooth = fast = slow = 0.0f;
        teethLeft = 0;
    }

private:
    static constexpr int claps = 4;           // three quick, and the one the tail grows from
    static constexpr float tailLevel = 0.75f;
    static constexpr float clapDrive = 1.1f;
    static constexpr float clapGain = 1.25f;
    static constexpr float maracasGain = 0.45f;

    float fs = 48000;
    bool maracas = false;
    Svf clapBp, maracasHp;

    float h = 1;
    float tooth = 0, toothFall = 0;
    int teethLeft = 0, timer = 0, spacing = 500;
    float fast = 0, slow = 0;
    float tailRise = 0, tailFall = 0, shakeRise = 0, shakeFall = 0;
};

} // namespace tr808
