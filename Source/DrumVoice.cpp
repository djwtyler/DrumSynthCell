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
    { ModTarget::PitchEnvDepth,   &VoiceParams::pitchEnvDepth   },
    { ModTarget::PitchEnvDecay,   &VoiceParams::pitchEnvDecay   },
    { ModTarget::NoiseLevel,      &VoiceParams::noiseLevel      },
    { ModTarget::NoiseDecay,      &VoiceParams::noiseDecay      },
    { ModTarget::NoiseBPFreq,     &VoiceParams::noiseBPFreq     },
    { ModTarget::NoiseBPQ,        &VoiceParams::noiseBPQ        },
    { ModTarget::DriveAmount,     &VoiceParams::driveAmount     },
    { ModTarget::FilterCutoff,    &VoiceParams::filterCutoff    },
    { ModTarget::FilterResonance, &VoiceParams::filterResonance },
    { ModTarget::FilterEnvAttack, &VoiceParams::filterEnvAttack },
    { ModTarget::FilterEnvHold,   &VoiceParams::filterEnvHold   },
    { ModTarget::FilterEnvDecay,  &VoiceParams::filterEnvDecay  },
    { ModTarget::FilterEnvDepth,  &VoiceParams::filterEnvDepth  },
    { ModTarget::AmpAttack,       &VoiceParams::ampAttack       },
    { ModTarget::AmpHold,         &VoiceParams::ampHold         },
    { ModTarget::AmpDecay,        &VoiceParams::ampDecay        },
    { ModTarget::Lfo1Rate,        &VoiceParams::lfo1Rate        },
    { ModTarget::Lfo1Depth,       &VoiceParams::lfo1Depth       },
    { ModTarget::Lfo2Rate,        &VoiceParams::lfo2Rate        },
    { ModTarget::Lfo2Depth,       &VoiceParams::lfo2Depth       },
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

    pitchEnv .prepare (sr);
    filterEnv.prepare (sr);
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
    pinkB[0] = pinkB[1] = pinkB[2] = 0.0f;

    lfo1.reset();
    lfo2.reset();
    noiseBP   .reset();
    mainFilter1.reset();
    mainFilter2.reset();

    pitchEnv .trigger (0.001f,                params.pitchEnvDecay > 0 ? params.pitchEnvDecay : 0.001f,
                       params.pitchEnvDecay);
    filterEnv.trigger (params.filterEnvAttack, params.filterEnvHold,  params.filterEnvDecay);
    ampEnv   .trigger (params.ampAttack,       params.ampHold,        params.ampDecay);

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
    ampEnv   .choke (0.002f);
    pitchEnv .choke (0.002f);
    filterEnv.choke (0.002f);
}

bool DrumVoice::isActive() const noexcept { return ampEnv.isActive(); }

void DrumVoice::applyTransMod() noexcept
{
    modSrcVals[(int)ModSource::LFO1]     = lfo1.peek (params.lfo1Rate, params.lfo1Wave);
    modSrcVals[(int)ModSource::LFO2]     = lfo2.peek (params.lfo2Rate, params.lfo2Wave);
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

float DrumVoice::computeOscSample (float pitchMod) noexcept
{
    const float hz  = params.pitchHz * std::pow (2.0f, pitchMod / 12.0f);
    const float p   = float (oscPhase);
    const float tpi = juce::MathConstants<float>::twoPi;

    float sine = std::sin (p * tpi);
    float saw  = 2.0f * p - 1.0f;
    float sq   = p < 0.5f ? 1.0f : -1.0f;

    // Continuous morph: Sine (0) → Saw (0.5) → Square (1.0)
    const float s    = juce::jlimit (0.0f, 0.9999f, params.oscShape) * 2.0f;
    const int   idx  = int (s);
    const float frac = s - float (idx);
    const float waves[3] = { sine, saw, sq };
    float sample = waves[idx] * (1.0f - frac) + waves[idx < 2 ? idx + 1 : 2] * frac;

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

float DrumVoice::computePartialSample (float pitchMod) noexcept
{
    const float fundamental = params.pitchHz * std::pow (2.0f, pitchMod / 12.0f);
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
    if (!ampEnv.isActive()) return;
    applyTransMod();

    for (int i = 0; i < numSamples; ++i)
    {
        // --- Modulation ---
        const float lfo1Val = lfo1.tick (params.lfo1Rate, params.lfo1Wave);
        const float lfo2Val = lfo2.tick (params.lfo2Rate, params.lfo2Wave);

        const float pitchEnvVal = pitchEnv.tick();
        const float pitchMod    = pitchEnvVal * params.pitchEnvDepth
                                + lfo1Val     * params.lfo1Depth;

        const float filterEnvVal = filterEnv.tick();
        const float cutMod       = filterEnvVal * params.filterEnvDepth
                                 + lfo2Val      * params.lfo2Depth;
        const float cutHz = params.filterCutoff * std::pow (2.0f, cutMod / 12.0f);

        // --- Source generation ---
        float osc = 0.0f;
        if (params.metallic)
            osc = computeMetallicSample();
        else if (params.shaperEnabled)
            osc = computePartialSample (pitchMod);
        else
            osc = computeOscSample (pitchMod);

        const float noiseEnv = noiseEnvValue;
        noiseEnvValue *= noiseEnvCoeff;
        const float noise = computeNoiseSample() * noiseEnv;

        // --- Mix ---
        float sig = osc * params.oscLevel + noise * params.noiseLevel;

        // --- Pre-filter drive ---
        sig = applyDrive (sig);

        // --- Filter ---
        sig = applyFilter (sig, cutHz);

        // --- Post-filter FX ---
        sig = applyFx1 (sig);

        // --- VCA ---
        const float amp = ampEnv.tick();
        dest[i] += sig * amp * velocity * params.outputGain;
    }
}

} // namespace DrumSynth
