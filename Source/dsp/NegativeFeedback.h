#pragma once

// ============================================================================
//  NegativeFeedback.h — Presence / Resonance shaping (§7).
//
//  In the real amp, Presence and Resonance live inside the global negative-
//  feedback loop around the power stage:
//     * Presence   reduces NFB at high frequencies → HF gain/edge increases.
//     * Resonance  shapes LF NFB → low-end thump / speaker damping.
//
//  A true model is a frequency-shaped feedback path wrapped around the power
//  amp (a delay-free loop). That is the intended upgrade; for v1 this is the
//  honest approximation the spec sanctions: a post-power-amp high-shelf
//  (Presence) and low-shelf (Resonance) driven by the same controls. Behaviour
//  is right (turn presence up → more top end); the interaction with power-amp
//  gain that a real loop gives is what a later revision buys.
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

    void reset() noexcept { presenceShelf.reset(); resonanceShelf.reset(); }

    void setPresence (float knob0to10) noexcept
    {
        // 0..10 → roughly -3..+9 dB high shelf around 2.2 kHz.
        const double db = -3.0 + 1.2 * knob0to10;
        presenceShelf.setHighShelf (sampleRate, 2200.0, db, 0.6);
    }

    void setResonance (float knob0to10) noexcept
    {
        // 0..10 → roughly -2..+8 dB low shelf around 110 Hz (LF thump/damping).
        const double db = -2.0 + 1.0 * knob0to10;
        resonanceShelf.setLowShelf (sampleRate, 110.0, db, 0.7);
    }

    inline float process (float x) noexcept
    {
        return resonanceShelf.process (presenceShelf.process (x));
    }

private:
    double sampleRate = 44100.0;
    Biquad presenceShelf, resonanceShelf;
};

} // namespace tekk
