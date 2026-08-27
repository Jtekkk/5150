#pragma once

// ============================================================================
//  NegativeFeedback.h — Presence / Resonance shaping (§7), v0.2.
//
//  In the real amp these controls live inside the global negative-feedback loop
//  around the power stage: Presence reduces HF NFB (more top-end AND more HF
//  drive/grit into the tubes), Resonance shapes LF NFB (low-end thump, damping,
//  and more LF hitting the power stage → more sag interaction).
//
//  A full delay-free loop is still the ultimate model, but v0.1's pure post-EQ
//  shelves missed the *feel*: the controls only re-EQ'd the output, they didn't
//  change what the power tubes saw. v0.2 splits the shaping into a PRE stage
//  (emphasis into the power amp, so more presence/resonance = more HF/LF
//  saturation and sag) and a POST stage (the output shelving). That reproduces
//  the interaction between these controls and power-amp drive that a real NFB
//  loop gives, while staying unconditionally stable (no feedback solve).
// ============================================================================

#include "Biquad.h"

namespace tekk
{

class NegativeFeedback
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        setPresence (5.0f);
        setResonance (5.0f);
        reset();
    }

    void reset() noexcept
    {
        presencePre.reset();  resonancePre.reset();
        presencePost.reset(); resonancePost.reset();
    }

    void setPresence (float knob0to10) noexcept
    {
        const double n = (double) knob0to10 / 10.0;             // 0..1
        // Pre-emphasis into the power tubes: brighter drive → more HF grit.
        presencePre.setHighShelf (sampleRate, 2200.0, -1.0 + 5.0 * n, 0.6);
        // Post edge on the output.
        presencePost.setHighShelf (sampleRate, 3200.0, -1.5 + 4.5 * n, 0.6);
    }

    void setResonance (float knob0to10) noexcept
    {
        const double n = (double) knob0to10 / 10.0;
        // Pre-emphasis: more low end into the power stage → more thump / sag.
        resonancePre.setLowShelf (sampleRate, 110.0, -0.5 + 3.0 * n, 0.7);
        // Post low-end body / damping.
        resonancePost.setLowShelf (sampleRate, 100.0, -2.0 + 7.0 * n, 0.7);
    }

    // Applied BEFORE the power amp (emphasis into the tubes).
    inline float processPre (float x) noexcept
    {
        return resonancePre.process (presencePre.process (x));
    }

    // Applied AFTER the output transformer (output shelving).
    inline float processPost (float x) noexcept
    {
        return resonancePost.process (presencePost.process (x));
    }

private:
    double sampleRate = 44100.0;
    Biquad presencePre, resonancePre, presencePost, resonancePost;
};

} // namespace tekk
