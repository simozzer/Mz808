// Mz808 - an emulation of the TR-808 Rhythm Composer
// Created by Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#include "Engine.h"

namespace tr808
{

const VoiceInfo& voiceInfo(int voice)
{
    static const VoiceInfo table[Voice::count] = {
        { "BD", "Bass Drum",    36 },
        { "SD", "Snare Drum",   38 },
        { "LT", "Low Tom",      43 },
        { "LC", "Low Conga",    64 },
        { "MT", "Mid Tom",      47 },
        { "MC", "Mid Conga",    63 },
        { "HT", "Hi Tom",       50 },
        { "HC", "Hi Conga",     62 },
        { "RS", "Rim Shot",     37 },
        { "CL", "Claves",       75 },
        { "CP", "Hand Clap",    39 },
        { "MA", "Maracas",      70 },
        { "CB", "Cow Bell",     56 },
        { "CY", "Cymbal",       49 },
        { "OH", "Open Hihat",   46 },
        { "CH", "Closed Hihat", 42 },
    };
    return table[voice < 0 || voice >= Voice::count ? 0 : voice];
}

int voiceForNote(int note)
{
    switch (note)
    {
        case 35: case 36:                   return Voice::BD;
        case 37:                            return Voice::RS;
        case 38: case 40:                   return Voice::SD;
        case 39:                            return Voice::CP;
        case 41: case 43: case 45:          return Voice::LT;
        case 47: case 48:                   return Voice::MT;
        case 50:                            return Voice::HT;
        case 42: case 44:                   return Voice::CH;
        case 46:                            return Voice::OH;
        case 49: case 52: case 55: case 57: return Voice::CY;
        case 56:                            return Voice::CB;
        case 62:                            return Voice::HC;
        case 63:                            return Voice::MC;
        case 64:                            return Voice::LC;
        case 70:                            return Voice::MA;
        case 75:                            return Voice::CL;
        default:                            return -1;
    }
}

void Engine::prepare(double sampleRate)
{
    fs = (float) sampleRate;

    metal.prepare(fs, rng);
    bd.prepare(fs);
    sd.prepare(fs);
    low.prepare(fs);
    mid.prepare(fs);
    high.prepare(fs);
    rim.prepare(fs);
    clap.prepare(fs);
    bell.prepare(fs);
    cym.prepare(fs);
    hat.prepare(fs);

    for (auto& a : awake) a = false;

    gainCoef = onePoleCoef(0.005f, fs);
    dcCoef = 1.0f - 2.0f * (float) pi * 8.0f / fs;
    dcX1 = dcY1 = 0.0f;

    setSettings(settings);
    for (auto& g : level) g.now = g.target;
    master.now = master.target;
}

void Engine::setSettings(const Settings& s)
{
    settings = s;

    bd.set(s.bdDecay, s.bdTone);
    sd.set(s.sdTone, s.sdSnappy);
    low.set(s.ltTuning);
    mid.set(s.mtTuning);
    high.set(s.htTuning);
    cym.set(s.cyTone, s.cyDecay);
    hat.set(s.ohDecay);

    level[BdC].target   = taper(s.bdLevel);
    level[SdC].target   = taper(s.sdLevel);
    level[LowC].target  = taper(s.ltLevel);
    level[MidC].target  = taper(s.mtLevel);
    level[HighC].target = taper(s.htLevel);
    level[RimC].target  = taper(s.rsLevel);
    level[ClapC].target = taper(s.cpLevel);
    level[BellC].target = taper(s.cbLevel);
    level[CymC].target  = taper(s.cyLevel);
    level[HatC].target  = taper(hat.isOpen() ? s.ohLevel : s.chLevel);

    master.target = 1.4f * taper(s.volume);
}

void Engine::noteOn(int note, int velocity)
{
    const int v = voiceForNote(note);
    if (v >= 0 && velocity > 0)
        trigger(v, velocity >= accentVelocity);
}

void Engine::trigger(int voice, bool accented)
{
    const float h = triggerHeight(accented, settings.accent);

    switch (voice)
    {
        case Voice::BD: bd.trigger(h);                  awake[BdC] = true;   break;
        case Voice::SD: sd.trigger(h);                  awake[SdC] = true;   break;
        case Voice::LT: low.trigger(h, false);          awake[LowC] = true;  break;
        case Voice::LC: low.trigger(h, true);           awake[LowC] = true;  break;
        case Voice::MT: mid.trigger(h, false);          awake[MidC] = true;  break;
        case Voice::MC: mid.trigger(h, true);           awake[MidC] = true;  break;
        case Voice::HT: high.trigger(h, false);         awake[HighC] = true; break;
        case Voice::HC: high.trigger(h, true);          awake[HighC] = true; break;
        case Voice::RS: rim.trigger(h, false);          awake[RimC] = true;  break;
        case Voice::CL: rim.trigger(h, true);           awake[RimC] = true;  break;
        case Voice::CP: clap.trigger(h, false);         awake[ClapC] = true; break;
        case Voice::MA: clap.trigger(h, true);          awake[ClapC] = true; break;
        case Voice::CB: bell.trigger(h);                awake[BellC] = true; break;
        case Voice::CY: cym.trigger(h);                 awake[CymC] = true;  break;

        // the hats' LEVEL is whichever was struck last: CH chokes OH, and is then the sound
        case Voice::OH:
        case Voice::CH:
            hat.trigger(h, voice == Voice::OH);
            level[HatC].target = taper(voice == Voice::OH ? settings.ohLevel : settings.chLevel);
            awake[HatC] = true;
            break;

        default: return;
    }

    hits[voice].fetch_add(1, std::memory_order_relaxed);
}

void Engine::allSoundOff()
{
    bd.reset(); sd.reset(); low.reset(); mid.reset(); high.reset();
    rim.reset(); clap.reset(); bell.reset(); cym.reset(); hat.reset();
    for (auto& a : awake) a = false;
}

void Engine::render(float* out, int numSamples)
{
    for (int n = 0; n < numSamples; ++n)
    {
        metal.tick();

        float g[circuits];
        for (int c = 0; c < circuits; ++c) g[c] = level[c].next(gainCoef);

        float sum = 0.0f;
        if (awake[BdC])   sum += g[BdC]   * bd.process();
        if (awake[SdC])   sum += g[SdC]   * sd.process(rng);
        if (awake[LowC])  sum += g[LowC]  * low.process(rng);
        if (awake[MidC])  sum += g[MidC]  * mid.process(rng);
        if (awake[HighC]) sum += g[HighC] * high.process(rng);
        if (awake[RimC])  sum += g[RimC]  * rim.process();
        if (awake[ClapC]) sum += g[ClapC] * clap.process(rng);
        if (awake[BellC]) sum += g[BellC] * bell.process(metal);
        if (awake[CymC])  sum += g[CymC]  * cym.process(metal);
        if (awake[HatC])  sum += g[HatC]  * hat.process(metal);

        // Every so often, put to sleep whatever has rung out - from rest, so a sleeping
        // circuit wakes exactly as a fresh one would.
        if ((n & 31) == 0)
        {
            if (awake[BdC]   && bd.quiet())   { bd.reset();   awake[BdC] = false; }
            if (awake[SdC]   && sd.quiet())   { sd.reset();   awake[SdC] = false; }
            if (awake[LowC]  && low.quiet())  { low.reset();  awake[LowC] = false; }
            if (awake[MidC]  && mid.quiet())  { mid.reset();  awake[MidC] = false; }
            if (awake[HighC] && high.quiet()) { high.reset(); awake[HighC] = false; }
            if (awake[RimC]  && rim.quiet())  { rim.reset();  awake[RimC] = false; }
            if (awake[ClapC] && clap.quiet()) { clap.reset(); awake[ClapC] = false; }
            if (awake[BellC] && bell.quiet()) { bell.reset(); awake[BellC] = false; }
            if (awake[CymC]  && cym.quiet())  { cym.reset();  awake[CymC] = false; }
            if (awake[HatC]  && hat.quiet())  { hat.reset();  awake[HatC] = false; }
        }

        // the mixer's coupling capacitor
        const float y = sum - dcX1 + dcCoef * dcY1;
        dcX1 = sum;
        dcY1 = y;

        out[n] = y * master.next(gainCoef);
    }
}

} // namespace tr808
