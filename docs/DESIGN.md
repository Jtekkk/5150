# Redline 120 — Design Notes

This maps the design spec to the implementation and records the modelling
choices (and honest approximations) in each block. Section numbers (§) refer to
the original technical spec.

## Signal chain (§3)

```
input gain → DC block → [gate: pre] → [TS boost]
   →‖ OVERSAMPLED (2/4/8/16×):
        preamp cascade → tone stack → phase inverter
        → power amp (sag) → output transformer → presence/resonance NFB ‖
   → cabinet (IR / built-in voicing) → [gate: post] → output gain
```

The nonlinear core runs inside oversampling; the cab convolution and the gate
run at host rate. Order is faithful to the spec, including the pre/post gate
placement switch.

## Anti-aliasing (§5) — the make-or-break issue

Two layers, as the spec recommends:

1. **ADAA** on every static nonlinearity. Triode and phase-inverter/boost
   shapers use **first-order ADAA** (`ADAA1`); the push-pull power stage uses
   **second-order ADAA** (`ADAA2`) since its cubic soft-clip has a closed-form
   second antiderivative and the crossover region benefits from the extra
   suppression.
   - Ill-conditioning near equal successive samples → guarded with midpoint /
     nested fallbacks (`ADAA.h`). `ADAA2` uses a deliberately looser tolerance
     (1e-3) because at signal extrema the taps bunch up and a tight tolerance
     leaves audible spikes at slope reversals — see the comment in `ADAA.h` and
     the sweep in `docs/VALIDATION.md`.
   - The half-sample group delay per ADAA order is small and lives inside the
     oversampled region; the dominant, integer latency (the oversampling
     half-band filters) is reported to the host for PDC.
2. **Oversampling** (`juce::dsp::Oversampling`, IIR half-band for low latency),
   default 4×, selectable 2/4/8/16×. ADAA + 4× is the balance the spec calls a
   strong compromise.

Why a shaper choice matters for ADAA: `ADAA1` only needs the first
antiderivative `F1`. The asymmetric triode uses `tanh(kx)/k` split at zero, whose
`F1 = logcosh(kx)/k²` is continuous everywhere (both pieces → 0 at the origin),
so the difference quotient is valid even across the sign change. A numerically
stable `logcosh` avoids `cosh` overflow when stages are driven hard.

## Tube stages (§4)

Each `TubeStage` = grid-stopper LPF → drive + bias → ADAA triode saturation →
coupling HPF → cathode-bypass low shelf → makeup.

- **Asymmetric clipping** via different positive/negative hardness (`kp`/`kn`) —
  the positive grid-conduction knee clips earlier/harder, generating the
  even-harmonic character. Asymmetry increases down the cascade.
- **Bias shift / blocking distortion**: a slow envelope of the driven signal
  offsets the operating point toward cutoff, so sustained heavy signal makes the
  stage tighten/gate ("farting out" on palm mutes). Depth grows in later stages.
  This is the parameter to tune against a palm-mute reference (§15).
- **Cathode-bypass** modelled as an RC-corner low shelf; **coupling caps** as
  interstage high-passes that tighten the low end as gain accumulates.

`PreampCascade` runs 5 stages (Lead) or 3 (Rhythm) with per-stage voicing
templates; Bright adds an HF shelf around the first gain pot, Crunch raises the
rhythm gain structure.

## Tone stack (§6)

Passive TMB modelled as the **analog-prototype transfer function** discretised
with the bilinear transform (Yeh, *Discretization of the '59 Fender Bassman Tone
Stack*, DAFx-06; coefficient algebra as in the Guitarix/Faust `tonestacks.lib`).
Component values are the Peavey ("classic American 120 W") voicing: R1 250k
(treble), R2 250k (bass), R3 20k (mid), R4 68k (slope), C1 270 pF, C2 = C3 22 nF.
The bass pot uses a log taper. Coefficients are recomputed at control rate from
parameter-smoothed pot values (no per-sample recompute, no zipper). The controls
interact and load each other exactly as the passive network does — verified
stable and directionally correct in the tests.

## Power amp (§7)

- **Push-pull class-AB** as a symmetric cubic soft-clip (ADAA2) with a smooth
  **crossover deadzone** near zero (both tubes idling at cutoff).
- **Power-supply sag**: an envelope of the driven signal droops the rail
  (fast-ish attack, slow release), compressing transient attack then blooming as
  the reservoir recharges — the biggest feel element. Verified: rail droops
  under load and recovers after.
- **Output transformer** (`OutputTransformer`): saturating core + a backlash
  "play" operator that opens a B-H loop, bracketed by LF (finite primary
  inductance) and HF (leakage + winding capacitance) roll-offs. This is a
  lightweight stand-in for a full Jiles-Atherton ODE solve — see *Follow-ups*.
- **Presence / Resonance** (`NegativeFeedback`): HF and LF shelves driven by the
  same controls. The spec-sanctioned v1 approximation of the frequency-shaped
  NFB loop; behaviour is correct, a true delay-free loop is a follow-up.

## Cabinet (§8)

Partitioned convolution (`juce::dsp::Convolution`) for user WAV IRs (normalised,
trimmed, resampled on load). With no IR loaded, a built-in **original** analytic
4×12 "V30-style" voicing (`SpeakerVoicing`, a biquad cascade) is used. **No
captured IRs are shipped** (legal note). Bypassable.

## Noise gate (§10)

Hysteretic (separate open/close thresholds) with attack/hold/release, so a
decaying note doesn't chatter. Selectable pre-preamp (tight chug) or post-cab
(natural).

## Parameters, denormals, latency (§9, §15)

- All continuous controls are `SmoothedValue`-smoothed (~20 ms) and managed by
  `AudioProcessorValueTreeState`.
- `ScopedNoDenormals` (FTZ/DAZ) guards the reactive/feedback states.
- Reported latency = oversampling latency + convolution latency (rounded).

## Follow-ups (post-v1)

- True **delay-free NFB loop** around the power stage (implicit/iterative solve)
  in place of the shelf approximation.
- Full **Jiles-Atherton** transformer (Newton per sample) in place of the
  play-operator stand-in.
- Optional **neural (NAM/RTNeural) preamp block** dropped in place of the
  cascade nonlinearity when a real head is captured, keeping the tone stack,
  power amp and cab as designed DSP (§12).
- Reference-reamp null testing and tuning pass (§13/§14 milestone 8).
