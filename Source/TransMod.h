#pragma once

// Modulation source IDs — matched 1:1 with sources available in DrumVoice
enum class ModSource : int { LFO1 = 0, LFO2, Velocity, NumSources };
static constexpr int kNumModSources = static_cast<int>(ModSource::NumSources);

// Parameters targetable by TransMod routing
enum class ModTarget : int {
    FilterCutoff    = 0,
    FilterResonance = 1,
    AmpDecay        = 2,
    NoiseLevel      = 3,
    DriveAmount     = 4,
    NumTargets
};
static constexpr int kNumModTargets = static_cast<int>(ModTarget::NumTargets);

// Physical min/max for each target (linear normalization)
struct ModTargetRange { float min, max; };
static constexpr ModTargetRange kModRanges[kNumModTargets] = {
    {  20.f,    20000.f },   // FilterCutoff
    {   0.f,       1.f  },   // FilterResonance
    {   0.001f,    8.f  },   // AmpDecay
    {   0.f,       1.f  },   // NoiseLevel
    {   0.f,       1.f  },   // DriveAmount
};

inline float modNorm(ModTarget t, float v) noexcept
{
    const auto& r = kModRanges[static_cast<int>(t)];
    float n = (v - r.min) / (r.max - r.min);
    return n < 0.f ? 0.f : (n > 1.f ? 1.f : n);
}

inline float modDenorm(ModTarget t, float n) noexcept
{
    if (n < 0.f) n = 0.f; else if (n > 1.f) n = 1.f;
    const auto& r = kModRanges[static_cast<int>(t)];
    return r.min + n * (r.max - r.min);
}

// Per-parameter TransMod state: base value + one depth per source
struct TransModParam {
    float base = 0.f;                         // normalized 0..1
    float depths[kNumModSources] = {};         // -1..1 per source

    float computeNorm(const float sv[kNumModSources]) const noexcept
    {
        float v = base;
        for (int i = 0; i < kNumModSources; ++i)
            v += sv[i] * depths[i];
        return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    }
};

// Full TransMod state for one voice
struct TransModState {
    TransModParam targets[kNumModTargets];

    TransModParam&       get(ModTarget t)       noexcept { return targets[static_cast<int>(t)]; }
    const TransModParam& get(ModTarget t) const noexcept { return targets[static_cast<int>(t)]; }

    void syncFromParams(float filterCutoff, float filterRes,
                        float ampDecay,     float noiseLevel,
                        float driveAmount)  noexcept
    {
        get(ModTarget::FilterCutoff)    .base = modNorm(ModTarget::FilterCutoff,    filterCutoff);
        get(ModTarget::FilterResonance) .base = modNorm(ModTarget::FilterResonance, filterRes);
        get(ModTarget::AmpDecay)        .base = modNorm(ModTarget::AmpDecay,        ampDecay);
        get(ModTarget::NoiseLevel)      .base = modNorm(ModTarget::NoiseLevel,      noiseLevel);
        get(ModTarget::DriveAmount)     .base = modNorm(ModTarget::DriveAmount,     driveAmount);
    }
};
