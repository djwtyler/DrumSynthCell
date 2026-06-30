#include "PluginProcessor.h"
#include "PluginEditor.h"

// MIDI note layout: C2=Kick D2=Snare D#2=Clap F2=Tom1 G2=Tom2 F#2=ClosedHat A#2=OpenHat C#3=Cymbal
static const int kChannelNotes[DrumSynth::NumChannels] = { 36, 38, 39, 41, 43, 42, 46, 49 };

DrumSynthProcessor::DrumSynthProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    using VP = DrumSynth::VoiceParams;

    // --- Kick (60 Hz bridged-T-style resonator ring, click + light drive) ---
    {
        auto& p = voices[DrumSynth::Kick].params;
        p.pitchHz        = 60.0f;
        p.oscMode        = VP::OscMode::Resonator;   // impulse-excited ring, the digital
                                                       // equivalent of the 808's bridged-T
                                                       // feedback network biased just below
                                                       // self-oscillation
        p.ringDecay      = 0.15f;    // shorter, punchier ring - 0.35s was too boomy/sustained
        p.env1Attack     = 0.001f;
        p.env1Decay      = 0.04f;
        p.noiseLevel     = 0.25f;    // louder click - needs to read as a distinct transient, not a texture
        p.noiseDecay     = 0.015f;   // tight, fast click
        p.noiseBPFreq    = 2200.0f;  // brighter, separated from the ~60Hz body so it reads as "snap" not boom
        p.noiseBPQ       = 0.6f;
        p.driveAmount    = 0.18f;   // light grit so the ring doesn't sound like a pure test tone
        p.driveType      = VP::DriveType::SoftClip;
        p.filterMode     = VP::FilterMode::LP;
        p.filterCutoff   = 1500.0f;  // raised from 800Hz so the click's transient brightness isn't rolled off
        p.filterResonance= 0.15f;    // lower resonance - 0.3 was emphasizing a narrow band, adding to the boom
        p.env2Decay      = 0.06f;
        p.ampAttack      = 0.001f;   // fastest possible attack for the hardest transient onset
        // Resonator's own ring decay and the Amp envelope's decay multiply
        // together (sig already carries the ring's decay, then gets scaled
        // by amp on top) - two multiplied exponentials compound to a
        // SHORTER combined decay than either alone (1/(1/ring + 1/amp)).
        // Keeping ampDecay near its max means Ring decay is what actually
        // controls the audible length, as intended.
        p.ampDecay       = 3.5f;
        p.outputGain     = 0.9f;
    }
    voices[DrumSynth::Kick].syncTransModFromParams();
    // Small Env1->Pitch sweep: a perfectly steady ring reads as a sustained
    // synth tone, not a drum hit - the fast pitch-fall at onset is a big part
    // of what makes percussion sound percussive. The previous depth (0.09)
    // was ~60x too large for this linearly-normalized 20-20000Hz range (it
    // produced a ~1800Hz swing on a 60Hz tone - nearly 2 octaves - audible as
    // a separate note). 0.0015 gives ~30Hz of swing (60Hz -> ~90Hz at onset,
    // falling back over env1Decay) - present but not a distinct pitch.
    voices[DrumSynth::Kick].transmod.get (ModTarget::PitchHz)
        .depths[(int) ModSource::Env1] = 0.0015f;
    voices[DrumSynth::Kick].transmod.get (ModTarget::FilterCutoff)
        .depths[(int) ModSource::Env2] = 0.04f;

    // --- Snare (180 Hz tone, slight pitch drop) ---
    {
        auto& p = voices[DrumSynth::Snare].params;
        p.pitchHz        = 180.0f;
        p.env1Attack     = 0.001f;
        p.env1Decay      = 0.02f;
        p.noiseDecay     = 0.2f;
        p.noiseColor     = VP::NoiseColor::White;
        p.noiseBPFreq    = 3000.0f;
        p.noiseBPQ       = 0.5f;
        p.filterMode     = VP::FilterMode::HP;
        p.filterCutoff   = 180.0f;
        p.filterResonance= 0.15f;
        p.ampDecay       = 0.25f;
        p.oscLevel       = 0.5f;
        p.outputGain     = 0.85f;
    }
    voices[DrumSynth::Snare].syncTransModFromParams();
    voices[DrumSynth::Snare].transmod.get (ModTarget::PitchHz)
        .depths[(int) ModSource::Env1] = 0.04f;

    // --- Clap (square wave body) ---
    {
        auto& p = voices[DrumSynth::Clap].params;
        p.pitchHz        = 900.0f;
        p.oscShape       = 0.66f;         // square
        p.oscLevel       = 0.15f;
        p.noiseDecay     = 0.12f;
        p.noiseColor     = VP::NoiseColor::White;
        p.noiseBPFreq    = 1200.0f;
        p.noiseBPQ       = 0.8f;
        p.filterMode     = VP::FilterMode::BP;
        p.filterCutoff   = 1500.0f;
        p.filterResonance= 0.4f;
        p.ampDecay       = 0.15f;
        p.outputGain     = 0.8f;
    }
    voices[DrumSynth::Clap].syncTransModFromParams();

    // --- Tom 1 (120 Hz, partial shaper, membrane mode) ---
    {
        auto& p = voices[DrumSynth::Tom1].params;
        p.pitchHz        = 120.0f;
        p.oscMode        = VP::OscMode::PartialShaper;
        p.partialPeak    = 1.0f;
        p.partialSpace   = 0.5f;
        p.partialRoll    = 0.6f;
        p.partialDecay   = 0.4f;
        p.membraneMode   = true;
        p.env1Attack     = 0.001f;
        p.env1Decay      = 0.03f;
        p.noiseDecay     = 0.05f;
        p.filterMode     = VP::FilterMode::LP;
        p.filterCutoff   = 600.0f;
        p.filterResonance= 0.2f;
        p.ampDecay       = 0.35f;
        p.outputGain     = 0.85f;
    }
    voices[DrumSynth::Tom1].syncTransModFromParams();
    voices[DrumSynth::Tom1].transmod.get (ModTarget::PitchHz)
        .depths[(int) ModSource::Env1] = 0.06f;

    // --- Tom 2 (90 Hz, deeper membrane) ---
    {
        auto& p = voices[DrumSynth::Tom2].params;
        p.pitchHz        = 90.0f;
        p.oscMode        = VP::OscMode::PartialShaper;
        p.partialPeak    = 1.0f;
        p.partialSpace   = 0.5f;
        p.partialRoll    = 0.6f;
        p.partialDecay   = 0.4f;
        p.membraneMode   = true;
        p.env1Attack     = 0.001f;
        p.env1Decay      = 0.03f;
        p.noiseDecay     = 0.05f;
        p.filterMode     = VP::FilterMode::LP;
        p.filterCutoff   = 500.0f;
        p.filterResonance= 0.2f;
        p.ampDecay       = 0.4f;
        p.outputGain     = 0.85f;
    }
    voices[DrumSynth::Tom2].syncTransModFromParams();
    voices[DrumSynth::Tom2].transmod.get (ModTarget::PitchHz)
        .depths[(int) ModSource::Env1] = 0.045f;

    // --- Closed Hat (808-style metallic cluster, short decay, choke group A) ---
    {
        auto& p = voices[DrumSynth::ClosedHat].params;
        p.pitchHz        = 8000.0f;
        p.oscMode        = VP::OscMode::Metallic;
        p.oscLevel       = 0.4f;
        p.noiseDecay     = 0.05f;
        p.noiseColor     = VP::NoiseColor::White;
        p.noiseBPFreq    = 10000.0f;
        p.noiseBPQ       = 1.0f;
        p.filterMode     = VP::FilterMode::HP;
        p.filterCutoff   = 6000.0f;
        p.filterResonance= 0.1f;
        p.ampDecay       = 0.08f;
        p.chokeGroup     = DrumSynth::ChokeGroup::A;
        p.outputGain     = 0.7f;
    }
    voices[DrumSynth::ClosedHat].syncTransModFromParams();

    // --- Open Hat (same cluster, longer decay, choke group A) ---
    {
        auto& p = voices[DrumSynth::OpenHat].params;
        p.pitchHz        = 8000.0f;
        p.oscMode        = VP::OscMode::Metallic;
        p.oscLevel       = 0.35f;
        p.noiseDecay     = 0.5f;
        p.noiseColor     = VP::NoiseColor::White;
        p.noiseBPFreq    = 9000.0f;
        p.noiseBPQ       = 0.8f;
        p.filterMode     = VP::FilterMode::HP;
        p.filterCutoff   = 5000.0f;
        p.filterResonance= 0.1f;
        p.ampDecay       = 0.5f;
        p.chokeGroup     = DrumSynth::ChokeGroup::A;
        p.outputGain     = 0.7f;
    }
    voices[DrumSynth::OpenHat].syncTransModFromParams();

    // --- Cymbal (808 metallic, long decay, HP) ---
    {
        auto& p = voices[DrumSynth::Cymbal].params;
        p.pitchHz        = 5000.0f;
        p.oscMode        = VP::OscMode::Metallic;
        p.oscLevel       = 0.25f;
        p.noiseDecay     = 1.0f;
        p.noiseColor     = VP::NoiseColor::White;
        p.noiseBPFreq    = 8000.0f;
        p.noiseBPQ       = 0.7f;
        p.filterMode     = VP::FilterMode::HP;
        p.filterCutoff   = 4000.0f;
        p.filterResonance= 0.15f;
        p.ampDecay       = 1.2f;
        p.outputGain     = 0.65f;
    }
    voices[DrumSynth::Cymbal].syncTransModFromParams();
}

void DrumSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (auto& v : voices)
        v.prepare (sampleRate, samplesPerBlock);
    monoMixBuf.setSize (1, samplesPerBlock);
}

void DrumSynthProcessor::releaseResources() {}

bool DrumSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

int DrumSynthProcessor::midiNoteToChannel (int midiNote) noexcept
{
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
        if (kChannelNotes[ch] == midiNote) return ch;
    return -1;
}

void DrumSynthProcessor::applyChokeGroups (int triggeredChannel)
{
    const auto group = voices[size_t (triggeredChannel)].params.chokeGroup;
    if (group == DrumSynth::ChokeGroup::None) return;
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
        if (ch != triggeredChannel && voices[size_t (ch)].params.chokeGroup == group)
            voices[size_t (ch)].choke();
}

void DrumSynthProcessor::handleMidiMessage (const juce::MidiMessage& msg)
{
    if (!msg.isNoteOn()) return;
    int ch = midiNoteToChannel (msg.getNoteNumber());
    if (ch < 0) return;
    applyChokeGroups (ch);
    voices[size_t (ch)].trigger (msg.getFloatVelocity());
}

void DrumSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer&          midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    monoMixBuf.clear();
    float*     mix        = monoMixBuf.getWritePointer (0);
    const int  numSamples = buffer.getNumSamples();
    int        samplePos  = 0;

    for (const auto metadata : midiMessages)
    {
        const int eventSample = metadata.samplePosition;
        if (eventSample > samplePos)
        {
            for (auto& v : voices) v.process (mix + samplePos, eventSample - samplePos);
            samplePos = eventSample;
        }
        handleMidiMessage (metadata.getMessage());
    }

    if (samplePos < numSamples)
        for (auto& v : voices) v.process (mix + samplePos, numSamples - samplePos);

    // Master bus: volume, then optional soft-clip limiter
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float s = mix[i] * masterVolume;
        if (limiterEnabled)
            s = std::tanh (s);
        mix[i] = s;
        peak = juce::jmax (peak, std::abs (s));
    }
    masterPeakLevel.store (peak, std::memory_order_relaxed);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, monoMixBuf, 0, 0, numSamples);
}

juce::AudioProcessorEditor* DrumSynthProcessor::createEditor()
{
    return new DrumSynthEditor (*this);
}

// ---------------------------------------------------------------------------
// State save / restore (serialises every VoiceParams field)
// ---------------------------------------------------------------------------
void DrumSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("DrumSynthCell");
    state.setProperty ("masterVolume",   masterVolume,   nullptr);
    state.setProperty ("limiterEnabled", limiterEnabled, nullptr);
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        const auto& p = voices[size_t (ch)].params;
        juce::ValueTree v ("Voice");
        v.setProperty ("ch",              ch,                              nullptr);
        v.setProperty ("pitchHz",         p.pitchHz,                      nullptr);
        v.setProperty ("oscShape",        p.oscShape,                     nullptr);
        v.setProperty ("oscMode",         int (p.oscMode),                nullptr);
        v.setProperty ("partialPeak",     p.partialPeak,                  nullptr);
        v.setProperty ("partialSpace",    p.partialSpace,                 nullptr);
        v.setProperty ("partialRoll",     p.partialRoll,                  nullptr);
        v.setProperty ("partialDecay",    p.partialDecay,                 nullptr);
        v.setProperty ("ringDecay",       p.ringDecay,                    nullptr);
        v.setProperty ("membraneMode",    p.membraneMode,                 nullptr);
        v.setProperty ("env1Attack",      p.env1Attack,                   nullptr);
        v.setProperty ("env1Hold",        p.env1Hold,                     nullptr);
        v.setProperty ("env1Decay",       p.env1Decay,                    nullptr);
        v.setProperty ("noiseLevel",      p.noiseLevel,                   nullptr);
        v.setProperty ("noiseDecay",      p.noiseDecay,                   nullptr);
        v.setProperty ("noiseColor",      int (p.noiseColor),             nullptr);
        v.setProperty ("noiseBPFreq",     p.noiseBPFreq,                  nullptr);
        v.setProperty ("noiseBPQ",        p.noiseBPQ,                     nullptr);
        v.setProperty ("driveAmount",     p.driveAmount,                  nullptr);
        v.setProperty ("driveType",       int (p.driveType),              nullptr);
        v.setProperty ("filterMode",      int (p.filterMode),             nullptr);
        v.setProperty ("filterModel",     int (p.filterModel),            nullptr);
        v.setProperty ("filterFourPole",  p.filterFourPole,               nullptr);
        v.setProperty ("filterCutoff",    p.filterCutoff,                 nullptr);
        v.setProperty ("filterResonance", p.filterResonance,              nullptr);
        v.setProperty ("env2Attack",      p.env2Attack,                   nullptr);
        v.setProperty ("env2Hold",        p.env2Hold,                     nullptr);
        v.setProperty ("env2Decay",       p.env2Decay,                    nullptr);
        v.setProperty ("ampAttack",       p.ampAttack,                    nullptr);
        v.setProperty ("ampHold",         p.ampHold,                      nullptr);
        v.setProperty ("ampDecay",        p.ampDecay,                     nullptr);
        v.setProperty ("lfo1Rate",        p.lfo1Rate,                     nullptr);
        v.setProperty ("lfo1Wave",        int (p.lfo1Wave),               nullptr);
        v.setProperty ("lfo2Rate",        p.lfo2Rate,                     nullptr);
        v.setProperty ("lfo2Wave",        int (p.lfo2Wave),               nullptr);
        v.setProperty ("fx1Type",         int (p.fx1Type),                nullptr);
        v.setProperty ("fx1Amount",       p.fx1Amount,                    nullptr);
        v.setProperty ("bitDepth",        p.bitDepth,                     nullptr);
        v.setProperty ("oscLevel",        p.oscLevel,                     nullptr);
        v.setProperty ("noiseMixGain",    p.noiseMixGain,                 nullptr);
        v.setProperty ("outputGain",      p.outputGain,                   nullptr);
        v.setProperty ("chokeGroup",      int (p.chokeGroup),             nullptr);

        // TransMod depths: flat "target source value" list, one entry per
        // non-zero depth. Base values are not stored — they are re-derived
        // from the params above via syncTransModFromParams() on load.
        {
            juce::String depths;
            const auto& tm = voices[size_t (ch)].transmod;
            for (int t = 0; t < kNumModTargets; ++t)
                for (int s = 0; s < kNumModSources; ++s)
                {
                    float d = tm.get ((ModTarget) t).depths[s];
                    if (std::abs (d) > 0.0001f)
                        depths << t << ' ' << s << ' ' << d << ' ';
                }
            v.setProperty ("tmDepths", depths, nullptr);
        }

        state.addChild (v, -1, nullptr);
    }
    juce::MemoryOutputStream mos (destData, true);
    state.writeToStream (mos);
}

void DrumSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, size_t (sizeInBytes));
    if (!state.isValid() || state.getType() != juce::Identifier ("DrumSynthCell")) return;

    masterVolume   = float (state.getProperty ("masterVolume",   masterVolume));
    limiterEnabled = bool  (state.getProperty ("limiterEnabled", limiterEnabled));

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto v = state.getChild (i);
        int  ch = int (v.getProperty ("ch", -1));
        if (ch < 0 || ch >= DrumSynth::NumChannels) continue;

        auto& p = voices[size_t (ch)].params;
        using VP = DrumSynth::VoiceParams;

        p.pitchHz        = float (v.getProperty ("pitchHz",        p.pitchHz));
        p.oscShape       = float (v.getProperty ("oscShape",       p.oscShape));
        p.oscMode        = VP::OscMode (int (v.getProperty ("oscMode", int (p.oscMode))));
        p.partialPeak    = float (v.getProperty ("partialPeak",    p.partialPeak));
        p.partialSpace   = float (v.getProperty ("partialSpace",   p.partialSpace));
        p.partialRoll    = float (v.getProperty ("partialRoll",    p.partialRoll));
        p.partialDecay   = float (v.getProperty ("partialDecay",   p.partialDecay));
        p.ringDecay      = float (v.getProperty ("ringDecay",      p.ringDecay));
        p.membraneMode   = bool  (v.getProperty ("membraneMode",   p.membraneMode));
        p.env1Attack     = float (v.getProperty ("env1Attack",     p.env1Attack));
        p.env1Hold       = float (v.getProperty ("env1Hold",       p.env1Hold));
        p.env1Decay      = float (v.getProperty ("env1Decay",      p.env1Decay));
        p.noiseLevel     = float (v.getProperty ("noiseLevel",     p.noiseLevel));
        p.noiseDecay     = float (v.getProperty ("noiseDecay",     p.noiseDecay));
        p.noiseColor     = VP::NoiseColor  (int (v.getProperty ("noiseColor",  int (p.noiseColor))));
        p.noiseBPFreq    = float (v.getProperty ("noiseBPFreq",    p.noiseBPFreq));
        p.noiseBPQ       = float (v.getProperty ("noiseBPQ",       p.noiseBPQ));
        p.driveAmount    = float (v.getProperty ("driveAmount",    p.driveAmount));
        p.driveType      = VP::DriveType   (int (v.getProperty ("driveType",   int (p.driveType))));
        p.filterMode     = VP::FilterMode  (int (v.getProperty ("filterMode",  int (p.filterMode))));
        p.filterModel    = VP::FilterModel (int (v.getProperty ("filterModel", int (p.filterModel))));
        p.filterFourPole = bool  (v.getProperty ("filterFourPole", p.filterFourPole));
        p.filterCutoff   = float (v.getProperty ("filterCutoff",   p.filterCutoff));
        p.filterResonance= float (v.getProperty ("filterResonance",p.filterResonance));
        p.env2Attack     = float (v.getProperty ("env2Attack",     p.env2Attack));
        p.env2Hold       = float (v.getProperty ("env2Hold",       p.env2Hold));
        p.env2Decay      = float (v.getProperty ("env2Decay",      p.env2Decay));
        p.ampAttack      = float (v.getProperty ("ampAttack",      p.ampAttack));
        p.ampHold        = float (v.getProperty ("ampHold",        p.ampHold));
        p.ampDecay       = float (v.getProperty ("ampDecay",       p.ampDecay));
        p.lfo1Rate       = float (v.getProperty ("lfo1Rate",       p.lfo1Rate));
        p.lfo1Wave       = VP::LfoWave (int (v.getProperty ("lfo1Wave", int (p.lfo1Wave))));
        p.lfo2Rate       = float (v.getProperty ("lfo2Rate",       p.lfo2Rate));
        p.lfo2Wave       = VP::LfoWave (int (v.getProperty ("lfo2Wave", int (p.lfo2Wave))));
        p.fx1Type        = VP::FxDistType (int (v.getProperty ("fx1Type",  int (p.fx1Type))));
        p.fx1Amount      = float (v.getProperty ("fx1Amount",      p.fx1Amount));
        p.bitDepth       = float (v.getProperty ("bitDepth",       p.bitDepth));
        p.oscLevel       = float (v.getProperty ("oscLevel",       p.oscLevel));
        p.noiseMixGain   = float (v.getProperty ("noiseMixGain",   p.noiseMixGain));
        p.outputGain     = float (v.getProperty ("outputGain",     p.outputGain));
        p.chokeGroup     = DrumSynth::ChokeGroup (int (v.getProperty ("chokeGroup", int (p.chokeGroup))));

        voices[size_t (ch)].syncTransModFromParams();

        // Restore TransMod depths saved as a flat "target source value" list
        auto& tm = voices[size_t (ch)].transmod;
        for (int t = 0; t < kNumModTargets; ++t)
            for (int s = 0; s < kNumModSources; ++s)
                tm.get ((ModTarget) t).depths[s] = 0.0f;

        juce::StringArray toks;
        toks.addTokens (v.getProperty ("tmDepths", juce::String()).toString(), " ", "");
        for (int k = 0; k + 2 < toks.size(); k += 3)
        {
            int t = toks[k].getIntValue();
            int s = toks[k + 1].getIntValue();
            if (t >= 0 && t < kNumModTargets && s >= 0 && s < kNumModSources)
                tm.get ((ModTarget) t).depths[s] = toks[k + 2].getFloatValue();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumSynthProcessor();
}
