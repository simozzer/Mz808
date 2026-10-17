// Mz808 - an emulation of the TR-808 Rhythm Composer
// Created by Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

#include "Params.h"

namespace params
{

using S = tr808::Settings;

const std::vector<Spec>& specs()
{
    //
    // CCs use the numbers MIDI leaves undefined, 14-31 and 102-119, in panel order, left
    // to right and top to bottom - so a controller's row of knobs lands on them in order.
    // VOLUME is General MIDI's, 7.
    //
    static const std::vector<Spec> table = {
        { "accent",   "Accent",            14, "How much harder an accented hit strikes: velocity above 100 is an accent. It does not just turn the sound up - every circuit answers a taller trigger in its own way, a kick blooming, a snare cracking.",
          &S::accent },

        { "bdLevel",  "Bass Drum Level",   15, "Level of the bass drum.",                                                            &S::bdLevel },
        { "bdTone",   "Bass Drum Tone",    16, "The bass drum's click: dark and round at 0, a sharp attack at 10.",                  &S::bdTone },
        { "bdDecay",  "Bass Drum Decay",   17, "How long the bass drum rings: a short thud at 0, a long, sighing boom at 10.",       &S::bdDecay },

        { "sdLevel",  "Snare Drum Level",  18, "Level of the snare drum.",                                                           &S::sdLevel },
        { "sdTone",   "Snare Drum Tone",   19, "The drum head: crossfades its two oscillators, low and hollow at 0, high and tight at 10.", &S::sdTone },
        { "sdSnappy", "Snare Drum Snappy", 20, "How much of the snares - the noise - goes in.",                                      &S::sdSnappy },

        { "ltLevel",  "Low Tom Level",     21, "Level of the low tom and the low conga, which share a circuit.",                     &S::ltLevel },
        { "ltTuning", "Low Tom Tuning",    22, "Pitch of the low tom and low conga, a quarter-octave either way.",                   &S::ltTuning },
        { "mtLevel",  "Mid Tom Level",     23, "Level of the mid tom and the mid conga, which share a circuit.",                     &S::mtLevel },
        { "mtTuning", "Mid Tom Tuning",    24, "Pitch of the mid tom and mid conga, a quarter-octave either way.",                   &S::mtTuning },
        { "htLevel",  "Hi Tom Level",      25, "Level of the hi tom and the hi conga, which share a circuit.",                       &S::htLevel },
        { "htTuning", "Hi Tom Tuning",     26, "Pitch of the hi tom and hi conga, a quarter-octave either way.",                     &S::htTuning },

        { "rsLevel",  "Rim Shot Level",    27, "Level of the rim shot and the claves, which share a circuit.",                       &S::rsLevel },
        { "cpLevel",  "Hand Clap Level",   28, "Level of the hand clap and the maracas, which share a circuit.",                     &S::cpLevel },
        { "cbLevel",  "Cow Bell Level",    29, "Level of the cow bell.",                                                             &S::cbLevel },

        { "cyLevel",  "Cymbal Level",      30, "Level of the cymbal.",                                                               &S::cyLevel },
        { "cyTone",   "Cymbal Tone",       31, "How much of the cymbal's top band - the splash - comes through.",                    &S::cyTone },
        { "cyDecay",  "Cymbal Decay",     102, "How long the cymbal's body rings.",                                                  &S::cyDecay },

        { "ohLevel",  "Open Hihat Level", 103, "Level of the open hihat.",                                                           &S::ohLevel },
        { "ohDecay",  "Open Hihat Decay", 104, "How long the open hihat rings. A closed hihat cuts it off.",                          &S::ohDecay },
        { "chLevel",  "Closed Hihat Level", 105, "Level of the closed hihat.",                                                       &S::chLevel },

        { "volume",   "Volume",             7, "Master output level.",                                                               &S::volume },
    };

    return table;
}

int indexOf(const juce::String& id)
{
    const auto& t = specs();
    for (size_t i = 0; i < t.size(); ++i)
        if (id == t[i].id) return (int) i;
    return -1;
}

juce::String hintFor(int index, int cc, bool learning)
{
    if (index < 0 || index >= (int) specs().size()) return {};
    const auto& p = specs()[(size_t) index];

    juce::String midi;
    if (learning)          midi = "MIDI LEARN: move a knob on your controller. Right-click to cancel.";
    else if (cc < 0)       midi = "No MIDI CC (default " + juce::String(p.cc) + "). Right-click to learn one.";
    else if (cc != p.cc)   midi = "MIDI CC " + juce::String(cc) + ", learned (default " + juce::String(p.cc) + "). Right-click to change.";
    else                   midi = "MIDI CC " + juce::String(cc) + ". Right-click to learn another.";

    return juce::String(p.name).toUpperCase() + "\n" + p.hint + "\n" + midi;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const S defaults;

    for (const auto& p : specs())
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { p.id, 1 }, p.name, juce::NormalisableRange<float> (0.0f, 10.0f, 0.0f), defaults.*(p.f),
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [] (float v, int) { return juce::String (v, 1); })));
    }

    return layout;
}

Reader::Reader(juce::AudioProcessorValueTreeState& state)
{
    for (const auto& p : specs())
    {
        auto* v = state.getRawParameterValue(p.id);
        jassert(v != nullptr);
        values.push_back(v);
    }
}

tr808::Settings Reader::read() const
{
    S s;
    const auto& table = specs();

    for (size_t n = 0; n < table.size(); ++n)
        s.*(table[n].f) = values[n]->load(std::memory_order_relaxed);

    return s;
}

} // namespace params
