# Redline 120

A real-time, physically-informed **high-gain 6L6 amp simulator** — VST3 / AU /
Standalone — voiced after a classic American 120 W 6L6 high-gain head.

> **Naming / legal:** amp circuits and topologies aren't copyrightable and
> behavioural emulation is standard practice, but trademarked names/logos and
> third-party captured impulse responses are off-limits. This project ships
> under an **original name** ("Redline 120", TEKK Audio Labs) and bundles **no**
> captured IRs — only an original, synthesised speaker voicing. Any reference to
> a specific amp in the source/docs is an internal modelling target only.

Built with [JUCE](https://juce.com) 8. The DSP core is deliberately
JUCE-independent so it can be validated with a bare compiler (see
[Testing](#testing)).

---

## What it models

The whole guitar-amp signal chain, not just a distortion curve (§ references are
to the design spec in [`docs/DESIGN.md`](docs/DESIGN.md)):

| Block | What it does |
|---|---|
| **TS Boost** (optional) | Tube-Screamer-style mid hump (~720 Hz) + low-cut + asymmetric soft clip in front of the amp — tightens the low end before the gain. |
| **Preamp cascade** | 5 cascaded 12AX7 gain stages on Lead / 3 on Rhythm, each an asymmetric ADAA triode stage with its own coupling-cap **bias shift** (blocking distortion), cathode-bypass shaping, and Bright / Crunch voicing. |
| **Tone stack** | The interactive passive TMB network as an analog-prototype transfer function (Yeh/DAFx discretisation), Peavey-family component values. The controls *interact* — it is not three independent EQs. |
| **Phase inverter** | LTP-style mild asymmetric drive into the power section. |
| **Power amp** | Push-pull class-AB 6L6 nonlinearity with a crossover region and **power-supply sag** (droop then bloom) — the single biggest "feel" element. |
| **Output transformer** | Saturating core (hysteresis) + LF/HF bandwidth limiting. |
| **Presence / Resonance** | HF / LF negative-feedback shaping around the power stage. |
| **Cabinet** | Partitioned IR convolution (load your own WAVs) or a built-in analytic 4×12 "V30-style" voicing. Bypassable. |
| **Noise gate** | Hysteretic gate, selectable pre-preamp (tight) or post-cab (natural). |

**Anti-aliasing** is treated as the make-or-break issue (§5): every static
nonlinearity is wrapped in **antiderivative anti-aliasing (ADAA)**, and the whole
nonlinear core additionally runs inside **2×/4×/8×/16× oversampling** (default
4×). See [`docs/DESIGN.md`](docs/DESIGN.md) and
[`docs/VALIDATION.md`](docs/VALIDATION.md).

## Building

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE is fetched automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Artefacts land in `build/Redline120_artefacts/`. On **Linux** you also need the
usual JUCE dev packages:

```sh
sudo apt-get install libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxcomposite-dev libasound2-dev libfreetype-dev \
  libfontconfig1-dev libgl1-mesa-dev
```

Build **only the DSP tests** (no JUCE / no network needed):

```sh
cmake -B build -DREDLINE_BUILD_PLUGIN=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## Testing

The pure-C++ DSP core (`Source/dsp/`) is exercised by
[`tests/dsp_tests.cpp`](tests/dsp_tests.cpp), which asserts the spec's validation
priorities (§13): ADAA actually suppresses aliasing, shaper antiderivatives are
continuous, the tone stack is stable and moves the right way per control,
supply sag droops-then-blooms, the gate mutes/passes/doesn't chatter, and the
transformer saturates. Quick run:

```sh
c++ -std=c++17 -O2 -ISource tests/dsp_tests.cpp -o dsp_tests && ./dsp_tests
```

## Parameters

Input Gain · Channel (Rhythm/Lead) · Bright · Crunch · Pre Gain · Bass · Mid ·
Treble · Resonance · Presence · Post Gain · Sag · Boost (+drive/level) · Gate
(+threshold/release/position) · Cab · Use IR · OS Quality · Output. All
continuous controls are parameter-smoothed to avoid zipper noise, and everything
is exposed through the `AudioProcessorValueTreeState` for automation and preset
recall.

## Layout

```
Source/
  PluginProcessor.*     signal-chain assembly, oversampling, latency, state
  PluginEditor.*        control-panel GUI
  ParameterIDs.h        APVTS parameter layout
  dsp/                  JUCE-independent DSP core (+ Cabinet, the one JUCE wrapper)
tests/dsp_tests.cpp     standalone validation harness
docs/                   DESIGN.md (spec→code map) · VALIDATION.md (test results)
```

## Status

Draft v0.1 — implements the full v1 signal chain from the design spec. Known
follow-ups are tracked in [`docs/DESIGN.md`](docs/DESIGN.md) (e.g. a true
delay-free NFB loop and a full Jiles-Atherton transformer solve).

## License

Original code © TEKK Audio Labs. JUCE is used under its own license terms
(GP/commercial); see the JUCE repository. No third-party trademarks or captured
impulse responses are included.
