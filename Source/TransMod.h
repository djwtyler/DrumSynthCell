#pragma once

// Modulation source IDs — matched 1:1 with sources available in DrumVoice
enum class ModSource : int { LFO1 = 0, LFO2, Velocity, NumSources };
static constexpr int kNumModSources = static_cast<int>(ModSource::NumSources);

// Parameters targetable by TransMod routing.
// Every continuous-valued synthesis parameter is a valid target — this list
// is append-only (existing indices must not be reordered/removed, since
// saved presets reference targets positionally via DrumVoice's binding table).
enum class ModTarget : int {
    PitchHz = 0,
    OscShape,
    PartialPeak,
    PartialSpace,
    PartialRoll,
    PartialDecay,
    PitchEnvDepth,
    PitchEnvDecay,
    NoiseLevel,
    NoiseDecay,
    NoiseBPFreq,
    NoiseBPQ,
    DriveAmount,
    FilterCutoff,
    FilterResonance,
    FilterEnvAttack,
    FilterEnvHold,
    FilterEnvDecay,
    FilterEnvDepth,
    AmpAttack,
    AmpHold,
    AmpDecay,
    Lfo1Rate,
    Lfo1Depth,
    Lfo2Rate,
    Lfo2Depth,
    Fx1Amount,
    BitDepth,
    OutputGain,
    NumTargets
};
static constexpr int kNumModTargets = static_cast<int>(ModTarget::NumTargets);

// Physical min/max for each target (linear normalization), in the same
// order as the ModTarget enum above.
struct ModTargetRange { float min, max; };
static constexpr ModTargetRange kModRanges[kNumModTargets] = {
    {   20.f,    2000.f },   // PitchHz
    {    0.f,       1.f },   // OscShape
    {    1.f,       8.f },   // PartialPeak
    {    0.f,       1.f },   // PartialSpace
    {    0.f,       1.f },   // PartialRoll
    {    0.f,       1.f },   // PartialDecay
    {    0.f,      48.f },   // PitchEnvDepth
    {0.001f,       2.f  },   // PitchEnvDecay
    {    0.f,       1.f },   // NoiseLevel
    {0.001f,       4.f  },   // NoiseDecay
    {  100.f,   20000.f },   // NoiseBPFreq
    {  0.1f,      10.f  },   // NoiseBPQ
    {    0.f,       1.f },   // DriveAmount
    {   20.f,   20000.f },   // FilterCutoff
    {    0.f,       1.f },   // FilterResonance
    {0.001f,       2.f  },   // FilterEnvAttack
    {    0.f,       2.f },   // FilterEnvHold
    {0.001f,       4.f  },   // FilterEnvDecay
    {  -48.f,      48.f },   // FilterEnvDepth
    {0.001f,       2.f  },   // AmpAttack
    {    0.f,       2.f },   // AmpHold
    {0.001f,       8.f  },   // AmpDecay
    {  0.01f,    1000.f },   // Lfo1Rate
    {    0.f,      24.f },   // Lfo1Depth
    {  0.01f,    1000.f },   // Lfo2Rate
    {    0.f,      24.f },   // Lfo2Depth
    {    0.f,       1.f },   // Fx1Amount
    {    1.f,      24.f },   // BitDepth
    {    0.f,       1.f },   // OutputGain
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

// Per-parameter TransMod state: base value + one depth per source.
// A target is "in" the modulation system for a given source exactly when
// its depth for that source is non-zero — turning a knob while a source is
// focused sets a non-zero depth (adds it); resetting depth to 0 removes it.
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
};
