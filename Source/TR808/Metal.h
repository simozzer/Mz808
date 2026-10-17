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
// The six square-wave oscillators the cowbell, cymbal and both hi-hats share: one HD14584
// hex Schmitt-trigger inverter, each gate an RC astable. Their frequencies are from Werner,
// Abel and Smith's cymbal analysis (ICMC 2014): 205.3, 304.4, 369.6 and 522.7 Hz fixed by
// their parts, and two trimmed at the factory to 540 and 800 Hz - the cowbell's pair. The
// duty cycle comes from the inverter's thresholds alone: 47.98% on 5 V.
//
// They never stop. A hit catches them wherever they are, so no two cymbals are the same.
// Each edge is band-limited (polyBLEP) at its exact sub-sample position.
//
class MetalBank
{
public:
    static constexpr int count = 6;
    static constexpr float hz[count] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    static constexpr float duty = 0.4798f;

    void prepare(float sampleRate, Random& rng)
    {
        for (int i = 0; i < count; ++i)
        {
            inc[i] = hz[i] / sampleRate;
            phase[i] = rng.uniform();
        }
    }

    void tick()
    {
        float total = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            float t = phase[i] + inc[i];
            if (t >= 1.0f) t -= 1.0f;
            phase[i] = t;

            const float dt = inc[i];
            float v = t < duty ? 0.5f : -0.5f;
            v += 0.5f * blep(t, dt);
            float tf = t - duty;
            if (tf < 0.0f) tf += 1.0f;
            v -= 0.5f * blep(tf, dt);

            out[i] = v;
            total += v;
        }
        sum = total;
    }

    float out[count] {};
    float sum = 0;

private:
    // the residual of a unit step at t = 0, spread over the samples either side
    static float blep(float t, float dt)
    {
        if (t < dt)
        {
            const float x = t / dt;
            return x + x - x * x - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            const float x = (t - 1.0f) / dt;
            return x * x + x + x + 1.0f;
        }
        return 0.0f;
    }

    float phase[count] {}, inc[count] {};
};

//
// The "swing-type" VCA of the metal voices: a common-emitter stage with its collector
// supply replaced by the envelope. Its gain follows the envelope, and it is biased so it
// swings hard into clipping - the 808 cymbal's grit, which stays as the sound decays.
//
inline float swingVca(float x, float envelope, float drive)
{
    return envelope * softClip(x * drive);
}

} // namespace tr808
