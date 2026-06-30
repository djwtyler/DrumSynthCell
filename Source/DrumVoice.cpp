#include "DrumVoice.h"

namespace DrumSynth
{

// Inharmonic partial ratios for a vibrating circular membrane (Bessel zeros)
static constexpr std::array<float, 8> kMembraneRatios = {
    1.0000f, 1.5933f, 2.1355f, 2.2954f, 2.6527f, 2.9173f, 3.1553f, 3.5001f
};

// Equal harmonic ratios (integer series)
static constexpr std::array<float, 8> kHarmonicRatios = {
    1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f
};

// Frequency ratios for the 808-style metallic cluster (6 detuned squares)
static constexpr std::array<float, 6> kMetallicRatios = {
    1.0000f, 1.4831f, 1.9996f, 2.5010f, 2.9994f, 3.5014f
};

// TransMod target <-> VoiceParams field bindings. Every continuous
// synthesis parameter is wired here once, driving both the base-value sync
// (preset load) and the per-block modulation apply (playback).
struct ModBinding { ModTarget target; float VoiceParams::* member; };
static constexpr ModBinding kModBindings[] = {
    { ModTarget::PitchHz,         &VoiceParams::pitchHz         },
    { ModTarget::OscShape,        &VoiceParams::oscShape        },
    { ModTarget::PartialPeak,     &VoiceParams::partialPeak     },
    { ModTarget::PartialSpace,    &VoiceParams::partialSpace    },
    { ModTarget::PartialRoll,     &VoiceParams::partialRoll     },
    { ModTarget::PartialDecay,    &VoiceParams::partialDecay    },
    { ModTarget::RingDecay,       &VoiceParams::ringDecay       },
    { ModTarget::Env1Attack,      &VoiceParams::env1Attack      },
    { ModTarget::Env1Hold,        &VoiceParams::env1Hold        },
    { ModTarget::Env1Decay,       &VoiceParams::env1Decay       },
    { ModTarget::NoiseLevel,      &VoiceParams::noiseLevel      },
    { ModTarget::NoiseDecay,      &VoiceParams::noiseDecay      },
    { ModTarget::NoiseBPFreq,     &VoiceParams::noiseBPFreq     },
    { ModTarget::NoiseBPQ,        &VoiceParams::noiseBPQ        },
    { ModTarget::DriveAmount,     &VoiceParams::driveAmount     },
    { ModTarget::FilterCutoff,    &VoiceParams::filterCutoff    },
    { ModTarget::FilterResonance, &VoiceParams::filterResonance },
    { ModTarget::Env2Attack,      &VoiceParams::env2Attack      },
    { ModTarget::Env2Hold,        &VoiceParams::env2Hold        },
    { ModTarget::Env2Decay,       &VoiceParams::env2Decay       },
    { ModTarget::AmpAttack,       &VoiceParams::ampAttack       },
    { ModTarget::AmpHold,         &VoiceParams::ampHold         },
    { ModTarget::AmpDecay,        &VoiceParams::ampDecay        },
    { ModTarget::Lfo1Rate,        &VoiceParams::lfo1Rate        },
    { ModTarget::Lfo2Rate,        &VoiceParams::lfo2Rate        },
    { ModTarget::Fx1Amount,       &VoiceParams::fx1Amount       },
    { ModTarget::BitDepth,        &VoiceParams::bitDepth        },
    { ModTarget::OutputGain,      &VoiceParams::outputGain      },
};

// ---------------------------------------------------------------------------
void DrumVoice::prepare (double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    oscPhase   = 0.0;
    metalPhases .fill (0.0);
    partialPhases.fill (0.0);

    env1     .prepare (sr);
    env2     .prepare (sr);
    ampEnv   .prepare (sr);
    lfo1     .prepare (sr);
    lfo2     .prepare (sr);
    noiseBP  .prepare (sr);
    mainFilter1.prepare (sr);
    mainFilter2.prepare (sr);
}

void DrumVoice::trigger (float vel)
{
    velocity  = vel;
    applyTransMod();   // snapshot LFO phase + velocity before reset; applies to envelope times

    oscPhase  = 0.0;
    metalPhases .fill (0.0);
    partialPhases.fill (0.0);
    resY1 = 1.0f; resY2 = 0.0f;   // single impulse excitation for the resonator
    pinkB[0] = pinkB[1] = pinkB[2] = 0.0f;

    lfo1.reset();
    lfo2.reset();
    noiseBP   .reset();
    mainFilter1.reset();
    mainFilter2.reset();

    env1.trigger (params.env1Attack, params.env1Hold, params.env1Decay);
    env2.trigger (params.env2Attack, params.env2Hold, params.env2Decay);
    ampEnv.trigger (params.ampAttack, params.ampHold, params.ampDecay);

    // Noise envelope (instant attack, exponential decay)
    noiseEnvValue = 1.0f;
    noiseEnvCoeff = params.noiseDecay > 0.0f
                  ? std::exp (-1.0f / (float (sampleRate) * params.noiseDecay))
                  : 0.0f;

    // Build partial shaper spectral weights from Peak + Roll
    for (int i = 0; i < 8; ++i)
    {
        float dist = std::abs (float (i) - (params.partialPeak - 1.0f));
        float roll = 0.01f + params.partialRoll * 0.99f;
        partialWeights[size_t (i)] = std::pow (roll, dist);

        // Higher partials decay faster; partialDecay=0 → all equal, =1 → 5× faster per octave
        float decayFactor = 1.0f + float (i) * params.partialDecay * 4.0f;
        float decayTime   = std::max (params.ampDecay / decayFactor, 0.001f);
        partialDecayCoeffs[size_t (i)] = std::exp (-1.0f / (float (sampleRate) * decayTime));
    }
}

void DrumVoice::choke()
{
    ampEnv.choke (0.002f);
    env1  .choke (0.002f);
    env2  .choke (0.002f);
}

bool DrumVoice::isActive() const noexcept { return ampEnv.isActive(); }

void DrumVoice::applyTransMod() noexcept
{
    modSrcVals[(int)ModSource::LFO1]     = lfo1.peek (params.lfo1Rate, params.lfo1Wave);
    modSrcVals[(int)ModSource::LFO2]     = lfo2.peek (params.lfo2Rate, params.lfo2Wave);
    modSrcVals[(int)ModSource::Env1]     = env1.getValue();
    modSrcVals[(int)ModSource::Env2]     = env2.getValue();
    modSrcVals[(int)ModSource::Velocity] = velocity;

    for (const auto& b : kModBindings)
        params.*(b.member) = modDenorm (b.target, transmod.get (b.target).computeNorm (modSrcVals));
}

void DrumVoice::syncTransModFromParams() noexcept
{
    for (const auto& b : kModBindings)
        transmod.get (b.target).base = modNorm (b.target, params.*(b.member));
}

// ---------------------------------------------------------------------------
// Oscillator helpers
// ---------------------------------------------------------------------------

float DrumVoice::computeOscSample() noexcept
{
    const float hz  = params.pitchHz;
    const float p   = float (oscPhase);
    const float tpi = juce::MathConstants<float>::twoPi;

    float sine = std::sin (p * tpi);
    float tri  = p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
    float saw  = 2.0f * p - 1.0f;
    float sq   = p < 0.5f ? 1.0f : -1.0f;

    // Continuous morph: Sine (0) → Triangle (1/3) → Saw (2/3) → Square (1)
    const float s    = juce::jlimit (0.0f, 0.9999f, params.oscShape) * 3.0f;
    const int   idx  = int (s);
    const float frac = s - float (idx);
    const float waves[4] = { sine, tri, saw, sq };
    float sample = waves[idx] * (1.0f - frac) + waves[idx < 3 ? idx + 1 : 3] * frac;

    oscPhase += double (hz) / sampleRate;
    if (oscPhase >= 1.0) oscPhase -= 1.0;
    return sample;
}

float DrumVoice::computeMetallicSample() noexcept
{
    float sum = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        float hz = params.pitchHz * kMetallicRatios[size_t (i)];
        sum += (metalPhases[size_t (i)] < 0.5) ? 1.0f : -1.0f;
        metalPhases[size_t (i)] += double (hz) / sampleRate;
        if (metalPhases[size_t (i)] >= 1.0) metalPhases[size_t (i)] -= 1.0;
    }
    return sum / 6.0f;
}

float DrumVoice::computePartialSample() noexcept
{
    const float fundamental = params.pitchHz;
    const auto& baseRatios  = params.membraneMode ? kMembraneRatios : kHarmonicRatios;

    // Differentially decay spectral weights and track total energy
    float totalWeight = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        partialWeights[size_t (i)] *= partialDecayCoeffs[size_t (i)];
        totalWeight += partialWeights[size_t (i)];
    }

    if (totalWeight < 1e-10f) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        // Space parameter stretches deviation from fundamental (0=unison, 0.5=natural, 1=stretched)
        float ratio = 1.0f + (baseRatios[size_t (i)] - 1.0f) * (params.partialSpace * 2.0f);
        float hz    = fundamental * ratio;
        float w     = partialWeights[size_t (i)] / totalWeight;

        sum += w * std::sin (float (partialPhases[size_t (i)])
                              * juce::MathConstants<float>::twoPi);

        partialPhases[size_t (i)] += double (hz) / sampleRate;
        if (partialPhases[size_t (i)] >= 1.0) partialPhases[size_t (i)] -= 1.0;
    }
    return sum;
}

float DrumVoice::computeResonatorSample() noexcept
{
    // Digital resonator: a 2-pole filter excited by a single impulse at
    // trigger() (resY1/resY2 seeded there), then left to ring freely on
    // its own poles - the DSP equivalent of an analog bridged-T feedback
    // network biased just below self-oscillation (TR-808 kick/tom tone
    // circuit). Pole radius r stays < 1 for stability; the closer to 1,
    // the longer (and closer to self-oscillating) the ring.
    const float theta = juce::MathConstants<float>::twoPi * params.pitchHz / float (sampleRate);
    const float r     = std::exp (-1.0f / (float (sampleRate) * juce::jmax (0.01f, params.ringDecay)));

    const float y0 = 2.0f * r * std::cos (theta) * resY1 - r * r * resY2;
    resY2 = resY1;
    resY1 = y0;
    return y0;
}

float DrumVoice::computeNoiseSample() noexcept
{
    float white = rng.nextFloat() * 2.0f - 1.0f;
    float noise = white;

    if (params.noiseColor == VoiceParams::NoiseColor::Pink)
    {
        pinkB[0] = 0.99886f * pinkB[0] + white * 0.0555179f;
        pinkB[1] = 0.99332f * pinkB[1] + white * 0.0750759f;
        pinkB[2] = 0.96900f * pinkB[2] + white * 0.1538520f;
        noise    = (pinkB[0] + pinkB[1] + pinkB[2] + white * 0.5362f) * 0.25f;
    }

    // Bandpass colouring (e.g. HP the noise for hats or narrow BP for snare snap)
    noise = noiseBP.process (noise, params.noiseBPFreq,
                              juce::jmax (0.1f, params.noiseBPQ),
                              VoiceParams::FilterMode::BP);
    return noise;
}

// ---------------------------------------------------------------------------
// Drive / filter / FX helpers
// ---------------------------------------------------------------------------

float DrumVoice::applyDrive (float in) const noexcept
{
    if (params.driveAmount <= 0.001f) return in;
    const float gain = 1.0f + params.driveAmount * 9.0f;
    float x = in * gain;
    switch (params.driveType)
    {
        case VoiceParams::DriveType::SoftClip: return std::tanh (x);
        case VoiceParams::DriveType::HardClip: return juce::jlimit (-1.0f, 1.0f, x);
        case VoiceParams::DriveType::Wavefold:
        {
            float v = std::fmod (x + 1.0f, 4.0f);
            if (v < 0.0f) v += 4.0f;
            return std::abs (v - 2.0f) - 1.0f;
        }
    }
    return x;
}

float DrumVoice::applyFilter (float in, float cutHz) noexcept
{
    // Q from resonance 0-1 → 0.5-20
    const float q   = 0.5f + params.filterResonance * 19.5f;
    float       out = mainFilter1.process (in, cutHz, q, params.filterMode);
    if (params.filterFourPole)
        out = mainFilter2.process (out, cutHz, q, params.filterMode);
    return out;
}

float DrumVoice::applyFx1 (float in) noexcept
{
    if (params.fx1Type == VoiceParams::FxDistType::Off) return in;
    const float gain = 1.0f + params.fx1Amount * 9.0f;
    float x = in * gain;
    switch (params.fx1Type)
    {
        case VoiceParams::FxDistType::SoftClip:  return std::tanh (x);
        case VoiceParams::FxDistType::HardClip:  return juce::jlimit (-1.0f, 1.0f, x);
        case VoiceParams::FxDistType::Bitcrusher:
        {
            float steps = std::pow (2.0f, juce::jlimit (1.0f, 24.0f, params.bitDepth));
            return std::round (in * steps) / steps;
        }
        case VoiceParams::FxDistType::Wavefold:
        {
            float v = std::fmod (x + 1.0f, 4.0f);
            if (v < 0.0f) v += 4.0f;
            return std::abs (v - 2.0f) - 1.0f;
        }
        case VoiceParams::FxDistType::Off: break;
    }
    return in;
}

// ---------------------------------------------------------------------------
// Main render loop
// ---------------------------------------------------------------------------
void DrumVoice::process (float* dest, int numSamples)
{
    // LFO1/LFO2/Env1/Env2 exist purely as TransMod sources (no hardwired
    // audio role), so they only need block-rate precision — advance once
    // per call, active or idle, so the TransMod ring keeps animating
    // between hits.
    lfo1.advanceBlock (params.lfo1Rate, numSamples);
    lfo2.advanceBlock (params.lfo2Rate, numSamples);
    env1.advanceBlock (numSamples);
    env2.advanceBlock (numSamples);
    applyTransMod();

    if (!ampEnv.isActive()) return;

    for (int i = 0; i < numSamples; ++i)
    {
        // --- Source generation ---
        float osc = 0.0f;
        switch (params.oscMode)
        {
            case VoiceParams::OscMode::Metallic:      osc = computeMetallicSample();  break;
            case VoiceParams::OscMode::PartialShaper: osc = computePartialSample();   break;
            case VoiceParams::OscMode::Resonator:     osc = computeResonatorSample(); break;
            case VoiceParams::OscMode::Single:        osc = computeOscSample();       break;
        }

        const float noiseEnv = noiseEnvValue;
        noiseEnvValue *= noiseEnvCoeff;
        const float noise = computeNoiseSample() * noiseEnv;

        // --- Mix ---
        float sig = osc * params.oscLevel + noise * params.noiseLevel * params.noiseMixGain;

        // --- Pre-filter drive ---
        sig = applyDrive (sig);

        // --- Filter ---
        sig = applyFilter (sig, params.filterCutoff);

        // --- Post-filter FX ---
        sig = applyFx1 (sig);

        // --- VCA ---
        const float amp = ampEnv.tick();
        dest[i] += sig * amp * velocity * params.outputGain;
    }
}

} // namespace DrumSynth
