#pragma once

// ============================================================================
//  ScreamerBoost.h — optional TS-style overdrive in front of the amp (§10).
//
//  A Tube-Screamer in front is the single most common way people record this
//  kind of amp: it barely distorts on its own, but the mid hump (~720 Hz) and
//  the low-cut tighten the bass BEFORE it hits the cascaded gain, which is what
//  stops palm mutes turning to mush. Signal path:
//     low-cut → mid-focused pre-emphasis → asymmetric soft clip → level → tone LPF
// ============================================================================

#include "ADAA.h"
#include "Shapers.h"
#include "OnePole.h"
#include "Biquad.h"

namespace tekk
{

class ScreamerBoost
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        lowCut.prepare (sr);
        lowCut.setCutoff (720.0);          // classic TS clipping-stage high-pass
        midHump.setPeak (sr, 720.0, 5.0, 0.7);
        toneLP.setLowPass (sr, 4200.0, 0.6);
        reset();
        setDrive (driveKnob);
    }

    void reset() noexcept
    {
        lowCut.reset(); midHump.reset(); toneLP.reset(); adaa.reset();
    }

    void setEnabled (bool e) noexcept { enabled = e; }
    void setDrive   (float knob0to10) noexcept
    {
        driveKnob = knob0to10;
        drive = 1.0f + 2.5f * (knob0to10 / 10.0f);
        adaa.nonlinearity().k = 2.0f + 3.0f * (knob0to10 / 10.0f);
    }
    void setLevel (float knob0to10) noexcept { level = 0.3f + 0.7f * (knob0to10 / 10.0f); }

    inline float process (float x) noexcept
    {
        if (! enabled) return x;
        float y = lowCut.process (x);
        y = midHump.process (y);
        // Slight DC bias before the clip → asymmetry, like a TS's diode pair.
        y = adaa.process (drive * y + 0.08f);
        y = toneLP.process (y);
        return level * y;
    }

private:
    double sampleRate = 44100.0;
    bool  enabled = false;
    float driveKnob = 5.0f, drive = 1.6f, level = 0.7f;

    OnePoleHP lowCut;
    Biquad    midHump, toneLP;
    ADAA1<TanhShaper> adaa;
};

} // namespace tekk
