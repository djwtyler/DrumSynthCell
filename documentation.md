  # DrumSynthCell — User Guide

DrumSynthCell is a hybrid drum synthesizer audio plugin built with the JUCE framework. It generates all sounds via synthesis — no sample packs. Eight independent channels cover the full classic drum machine palette: kick, snare, clap, toms, hats, and cymbal.

---

## Building and Running

```bash
# Configure (first time only — resolves JUCE from ../JUCE-framework)
cmake . -B build -DCMAKE_BUILD_TYPE=Debug

# Standalone app
cmake --build build --target DrumSynthCell_Standalone

# Plugin formats
cmake --build build --target DrumSynthCell_VST3
cmake --build build --target DrumSynthCell_AU

# Launch the standalone
open build/Source/DrumSynthCell_artefacts/Debug/Standalone/DrumSynthCell.app
```

---

## MIDI Note Mapping

Each channel is triggered by a dedicated MIDI note. Velocity is passed through to the voice engine.

| Channel     | MIDI Note | Note Name |
|-------------|-----------|-----------|
| Kick        | 36        | C2        |
| Snare       | 38        | D2        |
| Clap        | 39        | D#2       |
| Tom 1       | 41        | F2        |
| Tom 2       | 43        | G2        |
| Closed Hat  | 42        | F#2       |
| Open Hat    | 46        | A#2       |
| Cymbal      | 49        | C#3       |

---

## Choke Groups

Channels can be assigned to choke groups A, B, or C. When a channel in group X is triggered, all other active channels in group X are immediately silenced with a 2 ms fade. This is how a closed hi-hat cuts off an open hi-hat.

---

## Views

The plugin has two views toggled with the **Basic / Advanced** button in the header.

---

## Basic View

The basic view gives fast access to the most important parameters for each channel.

### Pad Grid

Eight pads arranged in a 4×2 grid (80×80 px each). Click a pad to select that channel. The selected pad highlights in teal. Pads also respond to the active MIDI note — they light up on incoming hits.

Pad order (left to right, top to bottom):
Kick → Snare → Clap → Tom 1 → Tom 2 → Closed Hat → Open Hat → Cymbal

### Macro Knobs

Eight macro knobs below the pads, each controlling the most significant parameter for the selected channel.

| Knob       | Parameter         | Range                         |
|------------|-------------------|--------------------------------|
| Tune       | Pitch (Hz)        | 20 – 20 000 Hz (300Hz @ 12 o'clock) |
| Env 1 Dec  | Env 1 decay       | 0.001 – 2 s                    |
| Attack     | Amp attack        | 0.001 – 1 s                    |
| Decay      | Amp decay         | 0.001 – 4 s                    |
| Volume     | Output gain       | 0 – 1                           |
| Noise      | Noise level       | 0 – 1                           |
| Flt Cut    | Filter cutoff     | 20 – 20 000 Hz (1000Hz @ 12 o'clock) |
| Resonance  | Filter resonance  | 0 – 1                           |

---

## Advanced View

The advanced view exposes every synthesis parameter, laid out in labeled sections across the full window width.

Selecting a pad in the advanced view switches the channel the same as in basic view — all controls update to reflect the selected channel's current state.

The TransMod source buttons appear to the right of the pad grid.

---

### OSC Section

Controls the main oscillator. A single **Oscillator Mode** dropdown selects
one of four mutually exclusive modes; only the controls relevant to the
selected mode are shown.

| Control         | Applies to       | Description                                                                 |
|-----------------|------------------|------------------------------------------------------------------------------|
| Oscillator Mode | All              | Single / Metallic Cluster / Partial Shaper / Resonator — selects the tone source |
| Shape           | Single only      | Continuous morph: Sine → Triangle → Saw → Square                            |
| Peak            | Partial Shaper   | Which partial (1–8) receives maximum spectral weight                        |
| Space           | Partial Shaper   | Stretches partial frequency spacing (0 = unison, 0.5 = natural, 1 = stretched) |
| Roll            | Partial Shaper   | Spectral rolloff rate away from the peak partial                            |
| Decay           | Partial Shaper   | Differential decay — higher partials fade faster; 0 = equal decay, 1 = 5× faster per octave |
| Membrane        | Partial Shaper   | Selects inharmonic Bessel ratios (drumskin physics) vs integer harmonics — not applicable to Single or Metallic Cluster |
| Ring            | Resonator only   | How long the impulse-excited ring persists (TransMod target), 0.01 – 2 s, 0.3s at 12 o'clock |

#### Oscillator Modes (mutually exclusive)

- **Single** (default): Shape knob morphs the waveform continuously, 0=Sine, 1/3=Triangle, 2/3=Saw, 1.0=Square.
- **Metallic Cluster**: Six detuned square oscillators at ratios 1.000, 1.483, 2.000, 2.501, 2.999, 3.501 relative to the base pitch. No other OSC controls apply. Used for 808 hats and cymbals.
- **Partial Shaper**: Eight-partial additive engine. The spectral envelope is set by Peak + Roll; partials decay differentially using the Decay control; Membrane selects the frequency grid (only meaningful in this mode).
- **Resonator**: A 2-pole digital filter excited by an impulse, then left to ring on its own poles — the DSP equivalent of an analog bridged-T feedback network biased just below self-oscillation (the actual TR-808 bass drum tone circuit). Pitch sets the inherent ring frequency; Ring sets how long it persists before decaying away (hard-gated below ~-80dB so it actually stops rather than trailing off forever — a single-pole exponential's time constant is not its audible duration).

  Per the TR-808 service notes (confirmed against the original circuit description, not just inferred): immediately after trigger, the network's time constant is halved and it rings at **twice** its inherent frequency for exactly one quarter of the normal period, then a retriggering pulse drops it straight to the inherent frequency to decay normally from there. This is implemented natively in `computeResonatorSample()` as a discrete frequency step + re-excitation — not a smooth pitch glide. The same general principle (start high, settle to the fundamental as resonance damps) is documented for the toms too, via a different circuit (diode-based time-constant reduction that fades as the diodes' internal resistance increases). Deliberately **not** paired with an external Env1→Pitch sweep on top — that would just fight the resonator's own built-in onset behaviour. Pitch should stay fixed for this mode.

  **Important — decays multiply, don't just pick the larger one**: the Resonator's own ring decay and the Amp envelope's decay both apply to the same signal (`sig * amp`), so they compound — the audible decay time is `1 / (1/Ring + 1/AmpDecay)`, always *shorter* than either alone. With Ring=0.35s and AmpDecay=0.6s the kick actually only rings for ~0.22s, not 0.35s. To let Ring be the dominant, audible control, set AmpDecay near its max (e.g. 3-4s) so it gets out of the way; use Ring to set the actual perceived length.

  A pure ring alone tends to sound like a clean test tone rather than a drum. The Kick preset layers in a small noise click (Level ~0.12, Decay ~0.03s, bandpassed around 600Hz) for the attack transient, and light Drive (~0.18, soft clip) for harmonic grit — both are part of what makes it read as a "kick" rather than a sine ping.

---

### NOISE Section

An independent noise generator mixed with the oscillator signal.

| Control   | Description                                              | Range          |
|-----------|----------------------------------------------------------|----------------|
| Level     | Noise generator level (TransMod target)                  | 0 – 1          |
| Decay     | Exponential noise envelope decay time                    | 0.001 – 2 s    |
| BP Freq   | Bandpass centre frequency for noise colouring            | 20 – 18 000 Hz |
| BP Q      | Bandpass Q (width) — higher values narrow the band       | 0.1 – 20       |
| Pink      | Switches noise colour from white to pink (–3 dB/octave) |                |

The noise always has its own fast exponential envelope (instant attack). Level is also a TransMod target; the Mixer section's separate Noise fader applies an additional, non-modulatable final mix gain on top.

---

### DRIVE Section

Pre-filter saturation/distortion applied to the mixed osc + noise signal.

| Control | Description                          | Range |
|---------|--------------------------------------|-------|
| Amount  | Drive amount (TransMod target)       | 0 – 1 |
| Type    | Distortion algorithm                 |       |

Drive types:
- **Soft Clip** — tanh saturation; smooth harmonic warmth
- **Hard Clip** — brickwall limiter; aggressive edge
- **Wavefold** — signal folds back on itself; complex harmonic content at high amounts

---

### FILTER Section

A two-stage state-variable filter (SVF, Chamberlin topology) placed after the drive.

| Control   | Description                                         | Range          |
|-----------|-----------------------------------------------------|----------------|
| Mode      | LP / HP / BP / Peak / Notch                         |                |
| Model     | Clean or Fat (reserved for future alternate topology) |               |
| 4-pole    | Chains two SVF stages for steeper 4-pole response   |                |
| Cutoff    | Filter cutoff frequency (TransMod target), 1000Hz at 12 o'clock | 20 – 20 000 Hz |
| Resonance | Filter Q (TransMod target)                          | 0 – 1          |

Cutoff (and Pitch, Noise BP Freq) display in kHz above 1000Hz rather than Hz.
Cutoff is internally clamped to 45% of the sample rate (not the full Nyquist)
since the Chamberlin SVF becomes numerically unstable as cutoff approaches
Nyquist, especially at high resonance — the filter's internal state is also
guarded against any resulting Inf/NaN.

Factory presets typically route Env 2 → Filter Cutoff via TransMod for the
classic filter sweep.

---

### MIXER Section

A totally separate section from Filter (same column, divided by a horizontal
line rather than the vertical lines used between adjacent panels). Three
vertical faders set the final mix balance of each synthesis layer. **None of
the three are TransMod destinations** — they're static gain stages, separate
from any modulatable level control the layer's own section may have.

| Control | Description                                                | Range |
|---------|-------------------------------------------------------------|-------|
| Osc     | Oscillator level — the only level control for the oscillator layer | 0 – 1 |
| Noise   | Final noise mix gain — separate from the Noise section's Level knob, which remains the generator's own modulatable level (TransMod target) | 0 – 1 |
| PCM     | Reserved for the PRD's planned Vintage PCM Layer; disabled until that synthesis layer is implemented | 0 – 1 |

---

### ENVELOPES Section

Three AHD (Attack – Hold – Decay) envelopes per voice. Env 1 and Env 2 are
general-purpose — they have no hardwired audio role; each one only affects
the sound once assigned to a target via TransMod (focus the source, drag a
knob). Amp is hardwired to the VCA.

#### Env 1 / Env 2 (general purpose — TransMod sources only)

| Control | Description            | Range (Env 1) | Range (Env 2) |
|---------|-------------------------|---------------|---------------|
| Attack  | Attack time             | 0.001 – 1 s   | 0.001 – 2 s   |
| Hold    | Hold time at peak       | 0 – 1 s       | 0 – 2 s       |
| Decay   | Exponential decay time (TransMod target) | 0.001 – 2 s | 0.001 – 4 s |

Factory presets typically route Env 1 → Pitch (the classic kick/tom/snare
pitch drop) and Env 2 → Filter Cutoff.

#### Amp Envelope (VCA)

| Control | Description                      | Range       |
|---------|-----------------------------------|-------------|
| Attack  | VCA attack time                   | 0.001 – 1 s |
| Hold    | VCA hold time at peak              | 0 – 1 s     |
| Decay   | VCA decay time (TransMod target)  | 0.001 – 4 s |

The voice stays active until the amp envelope reaches silence. The `isActive()` state is driven entirely by this envelope.

---

### LFO Section

Two LFOs per voice, running continuously and reset to phase 0 on each trigger.

#### LFO 1 → Pitch FM

| Control | Description              | Range        |
|---------|--------------------------|--------------|
| Rate    | LFO frequency            | 0.01 – 20 Hz |
| Depth   | FM depth in semitones    | 0 – 24       |
| Wave    | Sine / Triangle / Square / Saw Up / Saw Down |  |

#### LFO 2 → Filter cutoff

| Control | Description              | Range        |
|---------|--------------------------|--------------|
| Rate    | LFO frequency            | 0.01 – 20 Hz |
| Depth   | Cutoff modulation depth in semitones | 0 – 96 |
| Wave    | Sine / Triangle / Square / Saw Up / Saw Down |  |

Both LFOs contribute to the TransMod snapshot at each block boundary (see TransMod section).

---

### FX Section

A post-filter distortion/effect slot with selectable algorithm.

| Control   | Description                                      | Range    |
|-----------|--------------------------------------------------|----------|
| Type      | Off / Soft Clip / Hard Clip / Bitcrusher / Wavefold |       |
| Amount    | Drive gain for clip/fold algorithms              | 0 – 1    |
| Bit Depth | Quantisation depth for Bitcrusher                | 1 – 24 bits |

When Type is Off, the section has no effect on the signal. The Bitcrusher reduces bit depth without resampling — it quantises the amplitude at the current sample rate.

---

## TransMod System

TransMod is a visual modulation routing system inspired by FXpansion Tremor. It lets you assign modulation depth from any source to any target knob directly in the UI, without a separate modulation matrix panel.

### Sources

Three modulation sources are available per voice:

| Button | Source   | Colour | Signal range |
|--------|----------|--------|--------------|
| LFO 1  | LFO 1 output | Orange | –1 to +1 |
| LFO 2  | LFO 2 output | Purple | –1 to +1 |
| Vel    | MIDI velocity | Green | 0 to 1   |

Velocity is captured at note-on and held constant for the duration of the voice. LFO values are snapshotted once per audio block (at block start) and applied to all voices simultaneously.

### Targets

Five parameters can be modulated:

| Target            | Physical range      |
|-------------------|---------------------|
| Filter Cutoff     | 20 – 20 000 Hz      |
| Filter Resonance  | 0 – 1               |
| Amp Decay         | 0.001 – 8 s         |
| Noise Level       | 0 – 1               |
| Drive Amount      | 0 – 1               |

### How to Use

1. **Select a channel** by clicking a pad.
2. **Click a source button** (LFO 1, LFO 2, or Vel) in the TransMod row. The button highlights in its source colour. The UI is now in *focus mode* for that source.
3. **Turn any target knob** in the advanced panel. In focus mode, turning a knob adjusts the modulation *depth* for that source, not the base parameter value.
4. **Click the source button again** (or click a different source) to leave focus mode. Knobs return to editing base values.
5. **Double-click a knob** while a source is focused to clear that source's depth on it.
6. **Right-click (or Ctrl-click) any knob** at any time — focused or not — to open a menu listing every source currently modulating it (colour-coded to match its ring), each with a "clear" action, plus a "Clear all modulation" option. Shows "No modulation assigned" if none.

### Visual Feedback

When a source is focused, each modulatable knob shows a two-arc display:

- **Dimmed teal arc** — the base (unmodulated) parameter position.
- **Source-coloured arc** — the depth extent from the base. The arc stretches from the base angle towards the effective modulated position.

This gives an immediate visual read of how far the modulation will push a parameter.

### Signal Path

Modulation is computed at block rate. At the start of each audio block, the LFO values are read at their current phase without advancing, and the velocity value is held from the last trigger. The effective normalised parameter value is:

```
effective_norm = clamp(base_norm + Σ source_val[i] × depth[i], 0, 1)
```

The clamped normalised value is then converted back to the physical range for the DSP. This means modulation never pushes a parameter outside its defined range.

---

## DSP Signal Flow

For each channel, per audio block:

```
LFO1/LFO2 snapshot → TransMod apply (filter cut, res, amp decay, noise lvl, drive)

Per sample:
  LFO1 tick → pitch mod (semitones)
  LFO2 tick → filter cutoff mod (semitones)
  Pitch envelope tick → pitch mod
  Filter envelope tick → filter cutoff mod

  [Oscillator: single osc | metallic cluster | partial shaper]
    +
  [Noise: white/pink → bandpass → noise envelope]

  Mix (osc level + noise level)
  → Pre-filter Drive (soft clip | hard clip | wavefold)
  → SVF Filter (LP/HP/BP/Peak/Notch, optional 4-pole)
  → Post-filter FX (soft clip | hard clip | bitcrusher | wavefold)
  → VCA (amp envelope)
  → output × velocity × outputGain
```

All 8 voices are accumulated into a single mono mix buffer, then distributed to the output.

---

## State Persistence

All channel parameters and TransMod depths are saved and restored via the host's plugin state (getStateInformation / setStateInformation). The state is serialised as XML using JUCE's ValueTree. When state is restored, `syncFromParams()` is called to realign the TransMod base values with the restored parameter values before any triggers occur.

---

## Architecture Notes

- **SVFFilter** — Chamberlin state-variable filter. One instance per pole pair. Two instances chained when 4-pole is enabled.
- **AhdEnvelope** — generic Attack–Hold–Decay envelope. Used for pitch, filter, and amp. The decay coefficient is computed from seconds to a per-sample multiplier via `exp(−1 / (sampleRate × sec))`.
- **Lfo** — single-phase oscillator with five waveforms. `tick()` advances phase; `peek()` reads the current value without advancing, used by TransMod.
- **DrumVoice** — owns all DSP state for one channel: oscillator phases, envelopes, filters, LFOs, and the TransModState.
- **DrumSynthProcessor** — owns the array of 8 DrumVoice instances, routes MIDI to triggers, mixes all voices, handles choke groups.
- **TransModState** — lives on DrumVoice. Holds one `TransModParam` per target; each stores a normalised base value and three depth values (one per source).
