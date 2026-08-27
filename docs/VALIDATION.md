# Redline 120 — Validation

The spec (§13) makes validation a first-class concern: measure the DSP, don't
trust it by ear alone. Two automated suites do that, both wired into CTest:

```sh
ctest --test-dir build --output-on-failure
```

## 1. DSP core (`tests/dsp_tests.cpp`)

Pure C++, no JUCE — builds and runs anywhere. Representative run:

```
Shaper antiderivative continuity (ADAA precondition):
  [PASS] TriodeShaper f continuous at 0
  [PASS] TriodeShaper F1(0)=0
  [PASS] CubicClip F1(1) value / F2(1) inner value
  [PASS] CubicClip F2 continuous at ±1 / F1 continuous at +1
  [PASS] CubicClip saturates to 2/3

ADAA anti-aliasing (§5):
     alias energy: naive 69.7 dB | ADAA1 60.3 dB | 16x ref 35.8 dB
     ADAA1 improvement over naive: 9.4 dB
  [PASS] ADAA1 reduces aliasing >3 dB vs naive
  [PASS] 16x reference cleaner than ADAA1 alone

ADAA2 (cubic soft-clip) sanity:
  [PASS] ADAA2 tracks f in linear region  — maxErr=0.001227

Tone stack (§6):
  [PASS] all poles inside unit circle over control grid — max|pole|=0.997
  [PASS] treble up raises 4 kHz / bass up raises 90 Hz / mid up raises 650 Hz
  [PASS] DC blocked by the stack

Power-supply sag (§7):
     supply: start 0.997 | under load 0.404 | after release 0.988
  [PASS] rail droops under load / recovers (bloom) after

Noise gate (§10):
  [PASS] sub-threshold gated / above-threshold passes / no chatter (1 transition)

Output transformer (§7):
  [PASS] core saturates at high level (smallGain 0.98 → largeGain 0.41)

Preamp cascade (§4):
  [PASS] stage counts (Lead 5 / Rhythm 3) / output finite & non-zero

ALL TESTS PASSED
```

### What each result means

- **ADAA anti-aliasing.** A hard-clipped ~10 kHz tone (integer-periodic, so zero
  spectral leakage) is analysed for inharmonic energy. First-order ADAA at 1×
  already removes ~9 dB of aliasing vs. naive point-wise waveshaping; the 16×
  oversampled reference sits well below both, confirming the layered ADAA + OS
  strategy behaves as intended. (In the plugin, ADAA runs *inside* 4× OS, so the
  residual is lower still.)
- **ADAA2 conditioning.** A tolerance sweep (documented in the test and
  `ADAA.h`) shows the second-order scheme needs a looser ill-conditioning
  tolerance than the first-order one: at signal extrema the three taps bunch up,
  and a tight tolerance leaves ~0.19 spikes at slope reversals. 1e-3 caps the
  worst-case reconstruction error at ~1.2e-3.
- **Tone stack.** Poles stay inside the unit circle across a full grid of
  control settings, DC is blocked (AC-coupled network), and each control moves
  its band the correct direction — i.e. the discretised passive network is
  stable and behaves like the analog prototype.
- **Sag.** The rail state drops from ~1.0 to ~0.4 under a sustained loud tone
  and recovers to ~0.99 after — droop-then-bloom, the key feel element.

## 2. End-to-end processor (`tests/processor_smoke.cpp`)

Instantiates the full `RedlineAudioProcessor` and runs a palm-mute-style signal
through the whole chain (oversampling + every block) at several settings,
asserting:

- output stays **finite, bounded and non-silent** at default and max-gain
  settings,
- **all four oversampling qualities** (2/4/8/16×) process cleanly,
- **latency** is reported for host PDC,
- **state save/load round-trips** (parameters restore exactly),
- both **Lead and Rhythm** channels and **pre/post gate** placements run,
- **every factory preset** produces finite, bounded audio, and switching presets
  actually moves parameters.

This catches integration faults (NaNs, denormal blow-ups, wiring mistakes) that
unit tests on individual blocks can't.

## v0.2 additions

- `tests/transformer_ja_tests.cpp` (CTest: `transformer_ja_tests`) validates the
  **Jiles-Atherton** transformer: finite/bounded, ~unity small-signal gain,
  core saturation at high level, an **actually-open B-H loop** whose width tracks
  the Hysteresis control monotonically, and LF/HF bandwidth roll-off.
- `dsp_tests.cpp` gained checks for the **selectable cab voicings** (Modern
  brighter than Greenback; sub-low roll-off) and the **feedback-style
  Presence/Resonance** (presence raises HF pre- and post-power-amp; resonance
  raises LF).

## Still to do (reference-based, needs hardware — §13/§14)

The remaining spec validations require a real reference head and reamps:

- Null / A-B against reference reamps at matched settings.
- THD-vs-level curves per stage compared to the reference.
- Tone-stack sweeps vs. a SPICE model of the same network across pot positions.
- Aliasing spectrograms at extreme settings across OS ratios.
- CPU/latency profiling per OS setting.
