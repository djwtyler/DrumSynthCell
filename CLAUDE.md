# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Configure (first time only)
cmake . -B build -DCMAKE_BUILD_TYPE=Debug

# Build the Standalone app
cmake --build build --target DrumSynthCell_Standalone

# Build VST3 and AU plugin formats
cmake --build build --target DrumSynthCell_VST3
cmake --build build --target DrumSynthCell_AU

# Run the standalone
open build/Source/DrumSynthCell_artefacts/Debug/Standalone/DrumSynthCell.app
```

The JUCE framework is resolved from `../JUCE-framework` relative to this repo. IDE/LSP errors about missing JUCE headers are expected until CMake runs — the actual compiler always has the right include paths.

## Project Status

Phase 1 in progress. Core file structure and DSP skeleton are in place. The `Drum_Synth_PRD.md` documents the full product requirements.

## Project Goal

A hybrid drum synthesizer audio plugin capable of emulating classic analog drum machines (TR-808, TR-909, LinnDrum, Drumulator) and modern software synths (FXpansion Tremor). It must produce all sounds via synthesis — no relying on prepackaged sample packs.

## Architecture Defined in PRD

### Voice Engine (8 independent channels)

Each channel has three synthesis layers mixed together:

1. **Drum Skin Oscillator** — analog-modeled oscillator (Sine/Tri/Square/Saw + 6-detuned-square "Metallic Cluster" for 808-style hats) with a harmonic partial shaper for inharmonic membrane tuning and a hardwired ultrafast pitch envelope.

2. **Noise/Transient Generator** — white/pink noise + synthetic click/impulse generator for the initial beater transient.

3. **Vintage PCM Layer (optional)** — 8-bit/12-bit sample playback with variable-clock-rate pitch shifting (emulates Linn/E-mu anti-aliasing artifacts, not modern interpolation).

Layers feed into a shared **Mixer → Pre-filter Overdrive → Analog-modeled Multimode Filter (LP/HP/BP/Peak)**.

### Modulation

- 3 specialized envelopes per voice: Pitch/Transient Env (exponential, ultrafast), Filter Env (ADSR/AHD), Amp Env (AHDSR, hardwired to VCA).
- 2 LFOs per voice, supporting audio-rate FM (up to ~1 kHz) for metallic/bell tones.
- Visual drag-and-drop mod routing (Tremor TransMod / Serum mod-ring style): depth rings dragged directly onto destination knobs.

### Polyphony & Output

- Choke groups (A/B/C) for closed/open hat muting.
- Optional polyphony per voice (crash overlaps).
- Stereo master out + 8 individual DAW stereo aux outputs.

### Effects

- Per-voice: 2 FX slots (Distortion — Soft Clip/Hard Clip/Bitcrusher/Wavefolder; Transient Shaper; 2-band parametric EQ).
- Master bus: SSL-style bus compressor, short room/plate reverb.

### Sequencer (target for Phase 4)

- 16–32 steps per track, polyrhythmic (independent track lengths).
- Per-step parameter locks (P-Locks, Elektron-style).
- Per-step probability (0–100%) and ratchets/flams (repeats).

## Development Phases (from PRD)

1. Core DSP Engine — oscillators, harmonic partial generator, metallic noise cluster.
2. Hybrid Integration — PCM playback with vintage variable-clock-rate interpolation.
3. Modulation & UI — visual modulation system, envelope generators.
4. Sequencing & FX — step sequencer, probability, ratchets, built-in effects.
5. Presets & QA — factory sound library.

## Key DSP Reference Points

- **808 kick**: Bridged-T resonant filter circuit excited below self-oscillation; decay controls ring length.
- **808 cymbals/hats**: Six detuned square-wave oscillators at inharmonic high frequencies mixed together, then HP/BP filtered.
- **909 hybrid**: Analog circuits for kick/snare; 6-bit ~32 kHz PCM for hats/cymbals pushed through analog VCAs.
- **Linn pitch shifting**: Clock-rate alteration (not interpolation) — lower pitch = slower playback = aliasing artifacts.
- **Tremor harmonics**: `Peak` (emphasized partial), `Space` (partial interval), `Roll` (HF rolloff), `Decay` (differential decay per partial).
