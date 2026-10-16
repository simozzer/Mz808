// Mz808 - an emulation of the TR-808 Rhythm Composer
// Copyright (C) 2026 Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#include "Presets.h"

#include "Params.h"

namespace presets
{

const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = {
        { "Init", {} },

        { "Long Boom", {
            { "bdDecay", 10 }, { "bdTone", 3 }, { "bdLevel", 9 },
            { "sdTone", 4 }, { "sdSnappy", 6 }, { "cpLevel", 8 }, { "ohDecay", 3 } } },

        { "Tight", {
            { "bdDecay", 2.5f }, { "bdTone", 7 }, { "sdTone", 7 }, { "sdSnappy", 7 },
            { "ohDecay", 1.5f }, { "cyDecay", 2 }, { "accent", 6 } } },

        { "Electro", {
            { "bdDecay", 6.5f }, { "bdTone", 8 }, { "sdTone", 8 }, { "sdSnappy", 8 },
            { "rsLevel", 8 }, { "cbLevel", 7 }, { "chLevel", 7 }, { "accent", 8 } } },

        { "Deep Toms", {
            { "ltTuning", 1 }, { "mtTuning", 2 }, { "htTuning", 3 },
            { "ltLevel", 8 }, { "mtLevel", 8 }, { "htLevel", 8 }, { "bdDecay", 7 }, { "bdTone", 2 } } },

        { "Latin", {
            { "ltTuning", 6 }, { "mtTuning", 6 }, { "htTuning", 6 },
            { "rsLevel", 7 }, { "cpLevel", 6 }, { "cbLevel", 7 }, { "bdDecay", 4 }, { "bdLevel", 7 } } },

        { "Bright Metal", {
            { "cyTone", 9 }, { "cyDecay", 8 }, { "cyLevel", 7 }, { "ohDecay", 6 }, { "ohLevel", 7 },
            { "chLevel", 7 } } },

        { "Soft Accents", {
            { "accent", 2 }, { "bdDecay", 5.5f }, { "sdSnappy", 4 }, { "volume", 7.5f } } },

        { "Hard Accents", {
            { "accent", 10 }, { "bdDecay", 6 }, { "sdSnappy", 6 } } },
    };

    return list;
}

void apply(const Preset& p, juce::AudioProcessorValueTreeState& state)
{
    for (const auto& spec : params::specs())
        if (auto* param = state.getParameter(spec.id))
            param->setValueNotifyingHost(param->getDefaultValue());

    for (const auto& [id, value] : p.values)
        if (auto* param = state.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1((float) value));
}

} // namespace presets
