#pragma once

// ============================================================================
//  Presets.h — factory presets (v0.2).
//
//  Each preset is a list of {parameter id, raw value} pairs. The processor
//  applies one by converting each raw value to the parameter's normalised range
//  and notifying the host. Only voice-defining parameters are set; utility
//  controls (input/output gain, OS quality) are left as the user has them.
// ============================================================================

#include <vector>
#include <utility>
#include "ParameterIDs.h"

namespace tekk
{

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& factoryPresets()
{
    namespace P = params;
    static const std::vector<Preset> presets =
    {
        { "Init", {
            { P::channel, 1 }, { P::bright, 0 }, { P::crunch, 0 },
            { P::preGain, 5 }, { P::bass, 5 }, { P::mid, 5 }, { P::treble, 6 },
            { P::resonance, 5 }, { P::presence, 5 }, { P::postGain, 5 },
            { P::sag, 4 }, { P::depth, 4 }, { P::tightness, 5 },
            { P::boostOn, 0 }, { P::gateOn, 1 }, { P::gateThresh, -60 },
            { P::gatePosition, 0 }, { P::cabModel, 0 }, { P::mix, 100 } } },

        { "Lead Scream", {
            { P::channel, 1 }, { P::bright, 0 }, { P::crunch, 0 },
            { P::preGain, 8 }, { P::bass, 5 }, { P::mid, 6 }, { P::treble, 6.5f },
            { P::resonance, 6 }, { P::presence, 6.5f }, { P::postGain, 6 },
            { P::sag, 6 }, { P::depth, 5 }, { P::tightness, 5 },
            { P::boostOn, 1 }, { P::boostDrive, 2 }, { P::boostLevel, 7 },
            { P::gateOn, 1 }, { P::gateThresh, -58 }, { P::gatePosition, 0 },
            { P::cabModel, 0 }, { P::mix, 100 } } },

        { "Djent Chug", {
            { P::channel, 1 }, { P::bright, 0 }, { P::crunch, 0 },
            { P::preGain, 7 }, { P::bass, 6 }, { P::mid, 3.5f }, { P::treble, 7 },
            { P::resonance, 6.5f }, { P::presence, 7 }, { P::postGain, 5.5f },
            { P::sag, 3 }, { P::depth, 6.5f }, { P::tightness, 9 },
            { P::boostOn, 1 }, { P::boostDrive, 4 }, { P::boostLevel, 7.5f },
            { P::gateOn, 1 }, { P::gateThresh, -48 }, { P::gatePosition, 0 },
            { P::cabModel, 2 }, { P::mix, 100 } } },

        { "Rhythm Crunch", {
            { P::channel, 0 }, { P::bright, 1 }, { P::crunch, 1 },
            { P::preGain, 6 }, { P::bass, 5.5f }, { P::mid, 5.5f }, { P::treble, 6 },
            { P::resonance, 5 }, { P::presence, 5.5f }, { P::postGain, 5.5f },
            { P::sag, 5 }, { P::depth, 4.5f }, { P::tightness, 6 },
            { P::boostOn, 0 }, { P::gateOn, 1 }, { P::gateThresh, -60 },
            { P::gatePosition, 0 }, { P::cabModel, 1 }, { P::mix, 100 } } },

        { "Classic Rock", {
            { P::channel, 0 }, { P::bright, 1 }, { P::crunch, 0 },
            { P::preGain, 4 }, { P::bass, 5 }, { P::mid, 6 }, { P::treble, 6 },
            { P::resonance, 4 }, { P::presence, 5 }, { P::postGain, 6.5f },
            { P::sag, 6.5f }, { P::depth, 4 }, { P::tightness, 4 },
            { P::boostOn, 0 }, { P::gateOn, 1 }, { P::gateThresh, -64 },
            { P::gatePosition, 0 }, { P::cabModel, 1 }, { P::mix, 100 } } },

        { "Boosted Lead", {
            { P::channel, 1 }, { P::bright, 0 }, { P::crunch, 0 },
            { P::preGain, 7.5f }, { P::bass, 4.5f }, { P::mid, 6.5f }, { P::treble, 6 },
            { P::resonance, 5.5f }, { P::presence, 7 }, { P::postGain, 6 },
            { P::sag, 6 }, { P::depth, 5 }, { P::tightness, 6 },
            { P::boostOn, 1 }, { P::boostDrive, 3 }, { P::boostLevel, 7 },
            { P::gateOn, 1 }, { P::gateThresh, -55 }, { P::gatePosition, 0 },
            { P::cabModel, 0 }, { P::mix, 100 } } },

        { "Edge of Breakup", {
            { P::channel, 0 }, { P::bright, 1 }, { P::crunch, 0 },
            { P::preGain, 2.5f }, { P::bass, 5 }, { P::mid, 5.5f }, { P::treble, 6.5f },
            { P::resonance, 3.5f }, { P::presence, 5.5f }, { P::postGain, 7 },
            { P::sag, 7 }, { P::depth, 3.5f }, { P::tightness, 4 },
            { P::boostOn, 0 }, { P::gateOn, 0 }, { P::gateThresh, -70 },
            { P::gatePosition, 0 }, { P::cabModel, 3 }, { P::mix, 100 } } },
    };
    return presets;
}

} // namespace tekk
