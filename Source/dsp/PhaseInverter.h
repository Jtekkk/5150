#pragma once

// ============================================================================
//  PhaseInverter.h — long-tailed-pair (LTP) phase inverter (§7).
//
//  The LTP splits the signal into two anti-phase drives for the push-pull pair
//  and contributes its own mild, slightly asymmetric distortion as it is pushed.
//  In a mono model we don't need the two physical phases; what matters tonally
//  is that gentle asymmetric shaping ahead of the power tubes, so we model it as
//  a low-drive asymmetric soft clip (ADAA1) with a small fixed bias.
// ============================================================================

#include "ADAA.h"
#include "Shapers.h"

namespace tekk
{

class PhaseInverter
{
public:
    void prepare (double) noexcept { reset(); adaa.nonlinearity().k = 1.1f; }
    void reset() noexcept { adaa.reset(); }

    void setDrive (float d) noexcept { drive = d; }

    inline float process (float x) noexcept
    {
        // Light asymmetry (bias) → a touch of even harmonics feeding the PP stage.
        return adaa.process (drive * x + 0.03f) - biasComp;
    }

private:
    float drive = 1.0f;
    // tanh(0.03*k)/... offset removed so the PI doesn't inject DC downstream.
    static constexpr float biasComp = 0.033f; // ≈ tanh(1.1*0.03)
    ADAA1<TanhShaper> adaa;
};

} // namespace tekk
