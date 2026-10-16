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
// BASS DRUM: LEVEL, TONE, DECAY.
//
// After Werner, Abel and Smith's analysis of the circuit (DAFx-14):
//
//  - The 1 ms trigger goes through the pulse shaper into a bridged-T whose centre, with the
//    attack circuit idle, is Roland's typical 56 Hz.
//  - The trigger also fires an envelope generator (Q41, Q42), which for the length of the
//    pulse and about 5 ms after it grounds part of the bridged-T (Q43), raising its centre
//    by more than an octave and its Q with it. That is too brief to hear as pitch - under
//    one cycle at the higher frequency - but it is the punch and crispness of the attack.
//  - As that envelope falls, C39, R161 and D52 high-pass it into a retriggering pulse that
//    kicks the bridged-T again at its normal centre, so the note does not drop in level
//    when the centre comes down.
//  - Leakage through R161 holds the centre a little high for a while after: the 808's pitch
//    "sigh", which a long DECAY lets you hear.
//  - DECAY is the resistance in the feedback buffer that keeps the bridged-T ringing. Its
//    diodes clip the feedback when the ring is large, so a loud (accented) hit loses its
//    top faster and settles into the same tail - the bloom of an accented 808 kick.
//  - TONE is a passive low pass on the way out: the click, not the boom.
//
class BassDrum
{
public:
    void prepare(float sampleRate)
    {
        fs = sampleRate;
        res.prepare(fs);
        shaper.prepare(fs);
        envUp = onePoleCoef(0.0001f, fs);
        envDown = onePoleCoef(0.0017f, fs);   // back to ground ~5 ms after the pulse
        rpCoef = onePoleCoef(0.002f, fs);
        sighFall = decayCoef(0.11f, fs);
        set(5.0f, 5.0f);
        reset();
    }

    void set(float decayPanel, float tonePanel)
    {
        const float tau = expLaw(decayPanel / 10.0f, 0.05f, 0.75f);
        res.set(centre, tau);
        kHeld = res.damping();
        kShort = 1.0f / ((float) pi * centre * 0.03f);
        toneCoef = onePoleCoef(tauOf(expLaw(tonePanel / 10.0f, 180.0f, 6000.0f)), fs);
    }

    void trigger(float height)
    {
        pulse.fire(height, fs);
        sigh = height;
    }

    float process()
    {
        const float p = pulse.next();
        const float s = shaper.process(p);

        // Q41/Q42, up with the pulse and back down a few milliseconds after it
        env += (p - env) * (p > env ? envUp : envDown);

        // C39/R161/D52: the retriggering pulse, while the envelope falls
        rpLag += (env - rpLag) * rpCoef;
        const float rp = rpLag > env ? rpLag - env : 0.0f;

        sigh *= sighFall;

        const float shift = clamp(env, 0.0f, 1.0f);
        const float hz = centre * (1.0f + attackShift * shift) * (1.0f + sighDepth * sigh);

        // the feedback buffer's diodes: a big ring is fed back less, so it decays faster
        const float clipped = 1.0f - softClipGain(res.svf.bp / feedbackKnee);
        float k = kHeld + (kShort - kHeld) * clipped;
        k *= 1.0f - attackQ * shift;

        res.svf.tune(hz, k);
        const float y = res.process(drive * s + retrigger * rp);

        return out.lowPass(y + clickLeak * s, toneCoef) * gain;
    }

    bool quiet() const { return ! pulse.high() && res.energy() < 1.0e-6f && env < 1.0e-6f && std::fabs(out.z) < 1.0e-6f; }

    void reset()
    {
        res.reset();
        shaper.reset();
        out.reset();
        env = rpLag = sigh = 0.0f;
    }

private:
    static constexpr float centre = 56.0f;        // Roland's typical tuning
    static constexpr float attackShift = 1.3f;    // 2.3x the centre at the top of the attack
    static constexpr float attackQ = 0.4f;        // and less damping with it
    static constexpr float sighDepth = 0.045f;    // per unit of trigger: under a semitone
    static constexpr float feedbackKnee = 2.0f;
    static constexpr float drive = 2.2f;
    static constexpr float retrigger = 2.0f;
    static constexpr float clickLeak = 0.3f;
    static constexpr float gain = 1.75f;

    float fs = 48000;
    TriggerPulse pulse;
    PulseShaper shaper;
    BridgedT res;
    OnePole out;

    float envUp = 1, envDown = 1, rpCoef = 1, sighFall = 0, toneCoef = 1;
    float kHeld = 0.01f, kShort = 0.2f;
    float env = 0, rpLag = 0, sigh = 0;
};

} // namespace tr808
