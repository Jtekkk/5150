#pragma once

// ============================================================================
//  SpeakerVoicing.h — built-in analytic 4x12 "V30-style" voicing (§8).
//
//  A convolution cab needs an impulse response, and we deliberately ship NO
//  captured IRs (the legal note at the top of the spec: only distribute IRs you
//  have rights to). This is an *original, synthesised* speaker voicing — a short
//  cascade of biquads shaped like a close-mic'd 4x12 loaded with V30-style
//  speakers — used as the default when no user IR is loaded. It is a designed
//  filter, not a capture of anyone's cabinet.
//
//  Users can load their own WAV IRs (Cabinet) to replace it entirely.
// ============================================================================

#include "Biquad.h"

namespace tekk
{

class SpeakerVoicing
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        lowCut.setHighPass (sr, 85.0, 0.72);       // cab low-frequency cutoff
        body.setLowShelf   (sr, 130.0, 2.0, 0.7);  // low-end body/thump
        scoop.setPeak      (sr, 480.0, -2.5, 1.1);  // gentle low-mid scoop
        presence.setPeak   (sr, 2400.0, 4.0, 1.6);  // upper-mid presence bump
        topCut.setLowPass  (sr, 5000.0, 1.05);      // speaker HF roll-off + resonance
        fizzCut.setLowPass (sr, 8500.0, 0.5);       // tame remaining fizz
        reset();
    }

    void reset() noexcept
    {
        lowCut.reset(); body.reset(); scoop.reset();
        presence.reset(); topCut.reset(); fizzCut.reset();
    }

    inline float process (float x) noexcept
    {
        x = lowCut.process (x);
        x = body.process (x);
        x = scoop.process (x);
        x = presence.process (x);
        x = topCut.process (x);
        x = fizzCut.process (x);
        return x;
    }

    // For tests / analyser.
    double magnitude (double w) const noexcept
    {
        return lowCut.magnitude (w) * body.magnitude (w) * scoop.magnitude (w)
             * presence.magnitude (w) * topCut.magnitude (w) * fizzCut.magnitude (w);
    }

private:
    double sampleRate = 44100.0;
    Biquad lowCut, body, scoop, presence, topCut, fizzCut;
};

} // namespace tekk
