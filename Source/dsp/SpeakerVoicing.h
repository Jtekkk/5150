#pragma once

// ============================================================================
//  SpeakerVoicing.h — built-in analytic cab voicings (§8), v0.2.
//
//  Original, synthesised speaker/cab responses (NOT captures of anyone's
//  cabinet — the spec's legal note: ship no IRs you don't have rights to).
//  Several selectable voicings, each a short cascade of biquads shaped like a
//  close-mic'd guitar cab. Used as the default when no user IR is loaded, and
//  blendable against a loaded IR (see Cabinet).
// ============================================================================

#include "Biquad.h"

namespace tekk
{

enum class CabModel
{
    V30_412 = 0,     // 4x12 "V30-style": scooped low-mids, 2.4 kHz presence, 5 kHz cut
    Greenback_412,   // 4x12 vintage: warmer, earlier top rolloff, fatter low-mids
    Modern_212,      // 2x12 modern/tight: firmer lows, brighter, later cut
    Vintage_112      // 1x12 combo: smaller box, less low end, gentle top
};

class SpeakerVoicing
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        setModel (model);
        reset();
    }

    void reset() noexcept
    {
        lowCut.reset(); body.reset(); scoop.reset();
        presence.reset(); topCut.reset(); fizzCut.reset();
    }

    void setModel (CabModel m) noexcept
    {
        model = m;
        const double sr = sampleRate;
        switch (m)
        {
            case CabModel::V30_412:
                lowCut.setHighPass (sr, 85.0, 0.72);
                body.setLowShelf   (sr, 130.0, 2.0, 0.7);
                scoop.setPeak      (sr, 480.0, -2.5, 1.1);
                presence.setPeak   (sr, 2400.0, 4.0, 1.6);
                topCut.setLowPass  (sr, 5000.0, 1.05);
                fizzCut.setLowPass (sr, 8500.0, 0.5);
                break;
            case CabModel::Greenback_412:
                lowCut.setHighPass (sr, 90.0, 0.72);
                body.setLowShelf   (sr, 160.0, 3.0, 0.7);
                scoop.setPeak      (sr, 600.0, -1.5, 1.0);
                presence.setPeak   (sr, 1900.0, 3.0, 1.4);
                topCut.setLowPass  (sr, 4200.0, 1.0);
                fizzCut.setLowPass (sr, 7000.0, 0.5);
                break;
            case CabModel::Modern_212:
                lowCut.setHighPass (sr, 75.0, 0.8);
                body.setLowShelf   (sr, 110.0, 1.5, 0.7);
                scoop.setPeak      (sr, 450.0, -3.0, 1.3);
                presence.setPeak   (sr, 3000.0, 5.0, 1.7);
                topCut.setLowPass  (sr, 6000.0, 1.1);
                fizzCut.setLowPass (sr, 10000.0, 0.5);
                break;
            case CabModel::Vintage_112:
                lowCut.setHighPass (sr, 110.0, 0.7);
                body.setLowShelf   (sr, 180.0, 1.0, 0.7);
                scoop.setPeak      (sr, 700.0, -1.0, 1.0);
                presence.setPeak   (sr, 2100.0, 2.5, 1.3);
                topCut.setLowPass  (sr, 4600.0, 0.95);
                fizzCut.setLowPass (sr, 7500.0, 0.5);
                break;
        }
    }

    CabModel getModel() const noexcept { return model; }

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
    CabModel model = CabModel::V30_412;
    Biquad lowCut, body, scoop, presence, topCut, fizzCut;
};

} // namespace tekk
