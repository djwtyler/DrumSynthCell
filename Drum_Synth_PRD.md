# Product Requirements Document (PRD): Drum Synthesizer Plugin

## 1. Introduction & Objectives
**Goal:** To build a world-class, versatile drum synthesizer plugin that bridges the gap between classic vintage drum machines and modern, complex sound design tools. 

This document synthesizes deep research into the architectures of iconic drum machines (TR-808, TR-909, LinnDrum, Drumulator) and modern software masterpieces (FXpansion Tremor). It translates these insights into actionable product requirements for a new, highly capable drum synthesizer plugin.

---

## 2. Historical Context: Classic Drum Synthesis Techniques
To build a compelling drum synth, we must understand how the classics achieved their legendary sounds. These techniques fall into three main categories: pure analog subtractive synthesis, sample-based (PCM) playback, and hybrid architectures.

### 2.1 Pure Analog Subtractive Synthesis
**Examples:** Roland TR-808, Simmons SDS-V, Korg Volca Beats

* **The Roland TR-808 (1980):**
    * **Kicks & Toms (Bridged-T Networks):** The 808 kick doesn't use a standard oscillator. It uses a Bridged-T network (a resonant filter circuit) sitting just below the point of self-oscillation. A short pulse (trigger) excites the filter, causing it to "ring" at a specific frequency. The decay time controls how long the ringing lasts. 
    * **Snares:** Uses two bridged-T networks tuned to different frequencies to simulate the "body" (shell) of the snare, mixed with filtered white noise triggered by a VCA to simulate the snare wires.
    * **Cymbals & Hats (Metallic Noise):** Analog white noise wasn't metallic enough for cymbals. The 808 uses a cluster of six square-wave oscillators running at detuned, mathematically unrelated high frequencies. Mixed together, they create a harsh, inharmonic "metallic noise" that is then run through high-pass and band-pass filters and shaped by short envelopes.
* **The Simmons SDS-V (1981):**
    * Famous for the "pew" disco-tom sound. It used an analog oscillator (usually a triangle wave) coupled with a steep pitch-envelope drop, mixed with filtered noise.

### 2.2 Early PCM / Sample-Based Playback
**Examples:** Linn LM-1, LinnDrum, E-mu Drumulator

* **LinnDrum (1982) & LM-1:**
    * **Companded 8-bit Audio:** Samples were stored as 8-bit PCM, but encoded using non-linear "companding" (compressing/expanding) to maximize dynamic range, giving them a gritty, punchy characteristic.
    * **Variable Sample Rate Tuning:** Instead of mathematically interpolating pitches (like modern samplers), the Linn machines changed pitch by physically altering the clock rate of the playback chip. Lowering the pitch literally slowed down the playback rate, introducing beautiful, dark aliasing artifacts.
* **E-mu Drumulator (1983):**
    * Used 12-bit companded samples for slightly higher fidelity, combined with analog Curtis (SSM/CEM) filters and VCAs to shape the digital playback, warming up the cold digital samples.

### 2.3 Hybrid Synthesis
**Example:** Roland TR-909

* **The Roland TR-909 (1983):**
    * Recognized that analog synthesis was great for kicks, snares, and toms, but struggled to accurately recreate complex cymbals and hi-hats. 
    * **Analog Body:** The kick and snare use refined analog circuits (e.g., adding a pitch sweep to the kick, unlike the 808).
    * **Digital Top-End:** The hi-hats and cymbals are 6-bit, ~32kHz PCM samples. They are incredibly gritty and are pushed through analog VCAs and envelopes to integrate them with the rest of the kit.

---

## 3. Deep Dive: FXpansion Tremor Architecture
*Based on the FXpansion Tremor Operation Manual (v1.0).*

Tremor represents the pinnacle of modern software drum synthesis, eschewing samples entirely in favor of deep, circuit-modeled (DCAM) synthesis and complex modulation.

### 3.1 The Tremor Synthesis Voice (8 Voices Total)
Each voice in Tremor is identical, monophonic by default (with optional polyphony), and consists of the following signal flow:

**A. Oscillator & Harmonics**
* **The Core:** A specialized drum oscillator based on the physical properties of a vibrating membrane. 
* **Harmonics System:** Instead of standard waveforms, the oscillator generates partials. 
    * *Membrane Mode:* Partials are spaced at non-linear, inharmonic intervals (e.g., 1.5, 2.14, 2.3 octaves above fundamental) to simulate a circular drum skin.
    * *Harmonic Mode:* Partials are spaced evenly (e.g., 3 semitones apart) for synthetic tones.
    * *Parameters:* **Peak** (selects which partial is emphasized), **Space** (distance between partials), **Roll** (high-frequency roll-off of outer partials), and **Decay** (differential decay times for outer partials, simulating drum skin thickness).
* **Wave Shaping:** Morphs smoothly from Saw to Square to Triangle, featuring Pulse Width Modulation (PWM) and Hard Sync (synced to the sub-oscillator).
* **Audio-Rate FM:** Dedicated FM control allowing LFO 1 to modulate oscillator pitch at audio rates (up to 1024 Hz).

**B. Noise & Sub-Oscillator**
* **Noise:** Stereo white noise generator with dedicated Bandpass Filter (Tone, Width) and Stereo spread control. Crucial for claps, snares, and hats.
* **Sub-Oscillator:** Pure sine wave locked at 1, 2, or 3 octaves below the main oscillator.

**C. Drive & Filtering (DCAM Modeled)**
* **Pre-Drive:** Applies non-linear distortion (Diode, OTA, Op-amp, HalfRect, Shredder, Tannin, Clipper) *before* the filter to glue the Osc, Sub, and Noise together and generate new harmonics.
* **Filter:** Switchable between *Clean* (OTA-array) and *Fat* (State Variable Filter - SVF) models. Includes standard Cutoff, Resonance, and Drive/Gain.
* **Post-Drive:** A second distortion stage *after* the filter to crunch high-resonance peaks.

### 3.2 Tremor Modulation (TransMod System)
Tremor bypasses standard mod-matrix lists in favor of the **TransMod** system.
* **Visual Routing:** Users select a modulation source, then physically drag "depth rings" directly on the destination knobs.
* **Sources:**
    * **Envelopes:** Fast Env (optimized for clicks/transients, max 0.5s), Slow Env (max 5s), Amp Env (hardwired to VCA, max 10s).
    * **LFOs:** 2 Voice LFOs, 2 Global LFOs.
    * **Sample & Hold (S+H):** Clocked random values with adjustable Slew (glide).
    * **Performance:** Velocity, Note-on Random, Ramp (downward slope from ratchets).
    * **Macros (C1, C2, FxC, SxC):** FxC and SxC are special macros scaled by the Fast and Slow envelopes, respectively.

### 3.3 Sequencer & Graphs
* **Polyrhythmic Tracks:** 8 tracks per pattern. Each track can have a different length (e.g., Track 1 is 16 steps, Track 2 is 13 steps) and a different tempo multiplier (e.g., 1/2 speed, 2x speed).
* **Graphs:** 4 step-automation lanes per pattern. Can be assigned via TransMod to modulate *any* parameter per step.
* **Input Transform / Repeats:** Generates ratchets, flams, and rolls (up to 12 repeats per step) with adjustable time between repeats.

---

## 4. Proposed Product Requirements (The Plugin Build)

Based on the research above, our drum synth plugin will utilize a **Hybrid Synthesis Engine** to capture both analog punch, digital grit, and modern physical modeling. 

### 4.1 Global Architecture
* **Voices:** 8 independent drum channels (Kick, Snare, Clap, Toms, Hats, Cymbals, Perc).
* **Polyphony:** Choke groups (A, B, C) for closed/open hats. Optional polyphony per voice for overlapping crash cymbals.
* **Output:** Stereo Master Out, with optional routing to 8 individual DAW stereo auxiliary outputs.

### 4.2 Voice Engine Design (Per Channel)
Each of the 8 channels will contain a multi-layered sound engine:

**Layer 1: The Drum Skin Oscillator (Physical/Analog Modeled)**
* **Waveforms:** Sine, Triangle, Square, Saw, plus a "Metallic Cluster" (6 detuned square waves for 808-style hats).
* **Harmonic Shaper (Tremor-inspired):** Ability to inject inharmonic partials (Membrane tuning) to instantly convert a synthetic beep into a woody or metallic strike.
* **Pitch Envelope:** Dedicated ultrafast decay envelope hardwired to pitch (for punchy kicks/toms).

**Layer 2: The Noise/Transient Generator**
* **White/Pink Noise.**
* **Click/Transient Generator:** Short, synthetic clicks (sine bursts, impulses) to provide the initial "beater" sound of a kick or snare.

**Layer 3: The "Vintage PCM" Layer (Optional/Hybrid)**
* **Sample Playback:** A small library of classic 8-bit/12-bit samples (909 hats, LinnDrum claps).
* **Variable Clock Emulation:** Pitching these samples down should emulate the gritty anti-aliasing filter drop of the original Linn/E-mu machines, not modern interpolation.

**Mixer & Filter Block**
* **Osc / Noise / PCM Mix.**
* **Filter:** Analog-modeled multimode filter (LP, HP, BP, Peak) with a pre-filter overdrive circuit.

### 4.3 Modulation & Envelopes
* **3 Specialized Envelopes:**
    * *Pitch/Transient Env:* Exponential, extremely fast attack/decay for "snapping" transients.
    * *Filter Env:* ADSR or AHD.
    * *Amp Env:* AHDSR, hardwired to VCA.
* **LFOs:** 2 per voice. Must support audio-rate FM for creating metallic/bell-like tones.
* **Routing System:** Implement an intuitive drag-and-drop or visual depth-ring system (similar to Tremor's TransMod or Serum's mod rings) to assign Envelopes and Velocity to parameters (Cutoff, Pitch, Decay, Drive).

### 4.4 Effects (Per Voice & Master)
* **Per-Voice FX (2 Slots):**
    * *Distortion/Saturation:* Essential for making synthetic drums hit hard. Models should include Soft Clip, Hard Clip, Bitcrusher, and Wavefolder.
    * *Transient Shaper:* Attack/Sustain controls to tighten kicks or lengthen snare tails.
    * *EQ:* 2-band parametric.
* **Master Bus FX:**
    * *Bus Compressor:* SSL-style model for "gluing" the kit together.
    * *Reverb:* Short, dense room/plate reverbs (or a "TinCanVerb" style) to place the drums in a physical space.

### 4.5 The Sequencer (Optional but Highly Recommended)
* **Grid:** 16 to 32 steps per track.
* **Polyrhythms:** Independent track lengths for generative/evolving beats.
* **Per-Step Automation (P-Locks):** Ability to tie parameter changes (e.g., Pitch, Decay, Filter) to specific steps, mimicking Elektron sequencers or Tremor's Graphs.
* **Probability & Ratcheting:** Per-step probability (0-100% chance to play) and repeats (flams/rolls) for modern trap/electronic production.

---

## 5. Development Milestones
1.  **Phase 1: Core DSP Engine:** Build the oscillator, harmonic partial generator, and metallic noise cluster. Validate that basic 808/909 and synthetic sounds can be generated purely via math.
2.  **Phase 2: Hybrid Integration:** Implement the PCM playback engine with vintage variable-clock-rate interpolation.
3.  **Phase 3: Modulation & UI:** Build the visual modulation system (TransMod style) and envelope generators.
4.  **Phase 4: Sequencing & FX:** Implement the step sequencer, probability, ratchets, and built-in effects.
5.  **Phase 5: Presets & QA:** Sound design phase, ensuring the factory library covers classic analog, vintage PCM, and modern soundscapes.
