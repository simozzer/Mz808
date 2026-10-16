// Mz808 - an emulation of the TR-808 Rhythm Composer
// Copyright (C) 2026 Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#pragma once

namespace tr808
{

//
// The front panel. Every knob is in panel units, 0 to 10, as printed round it on the
// original. The engine turns these into times, frequencies and levels itself, so the
// plugin layer never has to know the curves.
//
// Pairs sharing one circuit - the toms and congas, rim shot and claves, hand clap and
// maracas - share their knobs, as on the hardware.
//
struct Settings
{
    float accent    = 5.0f;

    float bdLevel   = 8.0f;
    float bdTone    = 5.0f;
    float bdDecay   = 5.0f;

    float sdLevel   = 7.0f;
    float sdTone    = 5.0f;
    float sdSnappy  = 5.0f;

    float ltLevel   = 7.0f;
    float ltTuning  = 5.0f;
    float mtLevel   = 7.0f;
    float mtTuning  = 5.0f;
    float htLevel   = 7.0f;
    float htTuning  = 5.0f;

    float rsLevel   = 6.0f;
    float cpLevel   = 7.0f;
    float cbLevel   = 6.0f;

    float cyLevel   = 6.0f;
    float cyTone    = 5.0f;
    float cyDecay   = 5.0f;

    float ohLevel   = 6.0f;
    float ohDecay   = 4.0f;
    float chLevel   = 6.0f;

    float volume    = 7.0f;
};

} // namespace tr808
