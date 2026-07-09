#pragma once

// ============================================================================
//  ParameterIDs.h — string IDs and ranges for the AudioProcessorValueTreeState
//  (§9). Kept in one place so the processor and editor never disagree.
// ============================================================================

#include <juce_audio_processors/juce_audio_processors.h>

namespace tekk::params
{
    // --- IDs -----------------------------------------------------------------
    inline constexpr const char* inputGain  = "inputGain";
    inline constexpr const char* channel    = "channel";     // 0 = Rhythm, 1 = Lead
    inline constexpr const char* bright      = "bright";
    inline constexpr const char* crunch      = "crunch";
    inline constexpr const char* preGain     = "preGain";
    inline constexpr const char* bass         = "bass";
    inline constexpr const char* mid          = "mid";
    inline constexpr const char* treble       = "treble";
    inline constexpr const char* resonance    = "resonance";
    inline constexpr const char* presence     = "presence";
    inline constexpr const char* postGain     = "postGain";
    inline constexpr const char* sag          = "sag";
    inline constexpr const char* boostOn      = "boostOn";
    inline constexpr const char* boostDrive   = "boostDrive";
    inline constexpr const char* boostLevel   = "boostLevel";
    inline constexpr const char* gateOn       = "gateOn";
    inline constexpr const char* gateThresh   = "gateThresh";
    inline constexpr const char* gateRelease  = "gateRelease";
    inline constexpr const char* gatePosition = "gatePosition"; // 0 = pre, 1 = post
    inline constexpr const char* cabOn        = "cabOn";
    inline constexpr const char* useIR        = "useIR";
    inline constexpr const char* osQuality    = "osQuality";    // 0=2x 1=4x 2=8x 3=16x
    inline constexpr const char* output       = "output";

    // Current save-state version for the APVTS ValueTree.
    inline constexpr int stateVersion = 1;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Maps the osQuality choice index to an oversampling order (log2 of factor).
    inline int osChoiceToOrder (int choice) noexcept
    {
        switch (choice) { case 0: return 1; case 1: return 2; case 2: return 3; default: return 4; }
    }
    inline int osChoiceToFactor (int choice) noexcept { return 1 << osChoiceToOrder (choice); }
}
