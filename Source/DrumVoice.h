#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "TransMod.h"

namespace DrumSynth
{

enum ChannelIndex { Kick = 0, Snare, Clap, Tom1, Tom2, ClosedHat, OpenHat, Cymbal, NumChannels };
enum ChokeGroup   { None = 0, A, B, C };

// ============================================================
//  VoiceParams — full synthesis parameter set per channel
// ============================================================
struct VoiceParams
{
    // --- Oscillator ---
    float pitchHz   = 80.0f;
    float oscShape  = 0.0f;   // 0=Sine 0.33=Saw 0.66=Square 1.0=Tri
    bool  metallic  = false;  // 808-style metallic cluster (6 detuned squares)

    // --- Harmonic Partial Shaper ---
    bool  shaperEnabled = false;
    float partialPeak   = 1.0f;  // 1-8: which partial is loudest
    float partialSpace  = 0.5f;  // 0-1: partial frequency spacing
    float partialRoll   = 0.5f;  // 0-1: spectral rolloff away from peak
    float partialDecay  = 0.5f;  // 0-1: differential high-partial decay rate
    bool  membraneMode  = true;  // true=inharmonic drumskin, false=even harmonics

    // --- Fast Envelope (pitch / transient) ---
    float pitchEnvDepth  = 0.0f;   // semitones
    float pitchEnvDecay  = 0.04f;  // seconds

    // --- Noise ---
    float noiseLevel  = 0.0f;
    float noiseDecay  = 0.1f;
    enum class NoiseColor { White, Pink } noiseColor = NoiseColor::White;
    float noiseBPFreq = 8000.0f;
    float noiseBPQ    = 0.7f;

    // --- Pre-filter Drive ---
    float driveAmount = 0.0f;
    enum class DriveType { SoftClip, HardClip, Wavefold } driveType = DriveType::SoftClip;

    // --- Filter ---
    enum class FilterMode  { LP, HP, BP, Peak, Notch } filterMode  = FilterMode::LP;
    enum class FilterModel { Clean, Fat }               filterModel = FilterModel::Clean;
    bool  filterFourPole  = false;
    float filterCutoff    = 12000.0f;
    float filterResonance = 0.5f;   // 0-1

    // --- Slow Envelope (filter) ---
    float filterEnvAttack  = 0.005f;
    float filterEnvHold    = 0.0f;
    float filterEnvDecay   = 0.3f;
    float filterEnvDepth   = 0.0f;  // semitones

    // --- Amp Envelope (AHD, hardwired to VCA) ---
    float ampAttack  = 0.002f;
    float ampHold    = 0.0f;
    float ampDecay   = 0.5f;

    // --- LFO 1 (→ pitch FM) ---
    float lfo1Rate  = 1.0f;
    float lfo1Depth = 0.0f;   // semitones
    enum class LfoWave { Sine, Triangle, Square, SawUp, SawDown } lfo1Wave = LfoWave::Sine;

    // --- LFO 2 (→ filter cutoff mod) ---
    float   lfo2Rate  = 1.0f;
    float   lfo2Depth = 0.0f; // semitones
    LfoWave lfo2Wave  = LfoWave::Sine;

    // --- FX Slot 1 (post-filter distortion) ---
    enum class FxDistType { Off, SoftClip, HardClip, Bitcrusher, Wavefold } fx1Type = FxDistType::Off;
    float fx1Amount = 0.5f;
    float bitDepth  = 8.0f;

    // --- FX Slot 2 (2-band EQ, scaffolded) ---
    bool  eqEnabled   = false;
    float eqLowFreq   = 200.0f;
    float eqLowGain   = 0.0f;
    float eqHighFreq  = 4000.0f;
    float eqHighGain  = 0.0f;

    // --- Mix & output ---
    float oscLevel   = 1.0f;
    float outputGain = 0.8f;

    // --- Choke ---
    ChokeGroup chokeGroup = ChokeGroup::None;
};

// ============================================================
//  AHD Envelope  (Attack – Hold – Decay, generic, reusable)
// ============================================================
class AhdEnvelope
{
public:
    enum class Stage { Idle, Attack, Hold, Decay };

    void prepare (double sr) noexcept { sampleRate = sr; }

    void trigger (float attackSec, float holdSec, float decaySec) noexcept
    {
        value   = 0.0f;
        stage   = Stage::Attack;
        coeff   = coeffFor (attackSec);
        holdSmp = int (holdSec * sampleRate);
        holdCnt = 0;
        decSec  = decaySec;
    }

    void choke (float fadeTimeSec = 0.002f) noexcept
    {
        if (stage != Stage::Idle) { stage = Stage::Decay; coeff = coeffFor (fadeTimeSec); }
    }

    bool  isActive() const noexcept { return stage != Stage::Idle; }
    float getValue() const noexcept { return value; }

    float tick() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                value += (1.0f - value) * (1.0f - coeff);
                if (value >= 0.999f)
                {
                    value   = 1.0f;
                    stage   = holdSmp > 0 ? Stage::Hold : Stage::Decay;
                    holdCnt = 0;
                    coeff   = coeffFor (decSec);
                }
                break;
            case Stage::Hold:
                if (++holdCnt >= holdSmp) { stage = Stage::Decay; coeff = coeffFor (decSec); }
                break;
            case Stage::Decay:
                value *= coeff;
                if (value < 1e-6f) { value = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                break;
        }
        return value;
    }

private:
    double sampleRate = 44100.0;
    Stage  stage = Stage::Idle;
    float  value = 0.0f, coeff = 0.0f, decSec = 0.5f;
    int    holdSmp = 0, holdCnt = 0;

    float coeffFor (float sec) const noexcept
    {
        return sec > 0.0f ? std::exp (-1.0f / (float (sampleRate) * sec)) : 0.0f;
    }
};

// ============================================================
//  LFO  (supports audio-rate up to ~1 kHz for FM)
// ============================================================
class Lfo
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; }
    void reset()  noexcept           { phase = 0.0; }

    float tick (float rateHz, VoiceParams::LfoWave wave) noexcept
    {
        const float p   = float (phase);
        float       out = 0.0f;
        switch (wave)
        {
            case VoiceParams::LfoWave::Sine:
                out = std::sin (p * juce::MathConstants<float>::twoPi); break;
            case VoiceParams::LfoWave::Triangle:
                out = p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p); break;
            case VoiceParams::LfoWave::Square:
                out = p < 0.5f ? 1.0f : -1.0f; break;
            case VoiceParams::LfoWave::SawUp:
                out = 2.0f * p - 1.0f; break;
            case VoiceParams::LfoWave::SawDown:
                out = 1.0f - 2.0f * p; break;
        }
        phase += double (rateHz) / sampleRate;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }

    // Returns current value without advancing phase (for TransMod block-start snapshot)
    float peek (float /*rateHz*/, VoiceParams::LfoWave wave) const noexcept
    {
        const float p = float (phase);
        switch (wave)
        {
            case VoiceParams::LfoWave::Sine:
                return std::sin (p * juce::MathConstants<float>::twoPi);
            case VoiceParams::LfoWave::Triangle:
                return p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
            case VoiceParams::LfoWave::Square:
                return p < 0.5f ? 1.0f : -1.0f;
            case VoiceParams::LfoWave::SawUp:
                return 2.0f * p - 1.0f;
            case VoiceParams::LfoWave::SawDown:
                return 1.0f - 2.0f * p;
        }
        return 0.f;
    }

private:
    double sampleRate = 44100.0, phase = 0.0;
};

// ============================================================
//  SVF (State-Variable Filter, Chamberlin topology, 2-pole)
//  Provides LP / HP / BP / Notch / Peak simultaneously.
// ============================================================
class SVFFilter
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; reset(); }
    void reset()  noexcept           { z1 = z2 = 0.0f; }

    float process (float in, float cutHz, float q,
                   VoiceParams::FilterMode mode) noexcept
    {
        cutHz = juce::jlimit (20.0f, float (sampleRate * 0.48), cutHz);
        q     = juce::jlimit (0.1f, 20.0f, q);
        const float f  = 2.0f * std::sin (juce::MathConstants<float>::pi
                                           * cutHz / float (sampleRate));
        const float qr = 1.0f / (2.0f * q);
        float hp = in - z2 - qr * z1;
        float bp = f * hp + z1;
        float lp = f * bp + z2;
        z1 = bp; z2 = lp;
        switch (mode)
        {
            case VoiceParams::FilterMode::LP:    return lp;
            case VoiceParams::FilterMode::HP:    return hp;
            case VoiceParams::FilterMode::BP:    return bp;
            case VoiceParams::FilterMode::Notch: return lp + hp;
            case VoiceParams::FilterMode::Peak:  return lp - hp;
        }
        return lp;
    }

private:
    double sampleRate = 44100.0;
    float  z1 = 0.0f, z2 = 0.0f;
};

// ============================================================
//  DrumVoice — full synthesis engine for one drum channel
// ============================================================
class DrumVoice
{
public:
    DrumVoice() = default;

    void prepare (double sampleRate, int samplesPerBlock);
    void trigger (float velocity = 1.0f);
    void choke();
    bool isActive() const noexcept;
    void process (float* dest, int numSamples);

    VoiceParams   params;
    TransModState transmod;

private:
    double sampleRate = 44100.0;

    // Oscillator state
    double oscPhase = 0.0;
    std::array<double, 6> metalPhases  {};   // metallic cluster (808 hats)
    std::array<double, 8> partialPhases {};  // harmonic partial shaper

    // Partial shaper spectral state
    std::array<float, 8> partialWeights      {};  // evolve over time via differential decay
    std::array<float, 8> partialDecayCoeffs  {};  // cached per-partial decay coefficients

    // Envelopes
    AhdEnvelope pitchEnv, filterEnv, ampEnv;

    // Noise state
    float noiseEnvValue = 0.0f, noiseEnvCoeff = 0.0f;
    float pinkB[3] {};
    juce::Random rng;

    // Filters
    SVFFilter noiseBP;      // bandpass colouring for noise
    SVFFilter mainFilter1;
    SVFFilter mainFilter2;  // second stage for 4-pole (Fat model)

    // LFOs
    Lfo lfo1, lfo2;

    float velocity = 1.0f;
    float modSrcVals[kNumModSources] = {};

    void applyTransMod() noexcept;

    // DSP helpers
    float computeOscSample     (float pitchModSemitones) noexcept;
    float computeMetallicSample()                         noexcept;
    float computePartialSample (float pitchModSemitones) noexcept;
    float computeNoiseSample   ()                         noexcept;
    float applyDrive           (float in)          const noexcept;
    float applyFilter          (float in, float cutHz)    noexcept;
    float applyFx1             (float in)                 noexcept;
};

} // namespace DrumSynth
