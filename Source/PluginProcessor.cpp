#include "PluginProcessor.h"
#include "PluginEditor.h"

// MIDI note layout: C2=Kick D2=Snare D#2=Clap F2=Tom1 G2=Tom2 F#2=ClosedHat A#2=OpenHat C#3=Cymbal
static const int kChannelNotes[DrumSynth::NumChannels] = { 36, 38, 39, 41, 43, 42, 46, 49 };

DrumSynthProcessor::DrumSynthProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    using VP = DrumSynth::VoiceParams;

    // --- Kick (60 Hz sine, 2-octave pitch sweep, light pre-filter drive) ---
    {
        auto& p = voices[DrumSynth::Kick].params;
        p.pitchHz        = 60.0f;
        p.oscShape       = 0.0f;          // sine
        p.shaperEnabled  = false;
        p.pitchEnvDepth  = 24.0f;
        p.pitchEnvDecay  = 0.04f;
        p.noiseLevel     = 0.05f;
        p.noiseDecay     = 0.04f;
        p.noiseBPFreq    = 600.0f;
        p.noiseBPQ       = 0.5f;
        p.driveAmount    = 0.2f;
        p.driveType      = VP::DriveType::SoftClip;
        p.filterMode     = VP::FilterMode::LP;
        p.filterCutoff   = 800.0f;
        p.filterResonance= 0.3f;
        p.filterEnvDepth = 12.0f;
        p.filterEnvDecay = 0.06f;
        p.ampAttack      = 0.002f;
        p.ampDecay       = 0.6f;
        p.outputGain     = 0.9f;
    }
    voices[DrumSynth::Kick].transmod.syncFromParams (
        voices[DrumSynth::Kick].params.filterCutoff,  voices[DrumSynth::Kick].params.filterResonance,
        voices[DrumSynth::Kick].params.ampDecay,       voices[DrumSynth::Kick].params.noiseLevel,
        voices[DrumSynth::Kick].params.driveAmount);

    // --- Snare (180 Hz + white noise, slight pitch drop) ---
    {
        auto& p = voices[DrumSynth::Snare].params;
        p.pitchHz        = 180.0f;
        p.pitchEnvDepth  = 6.0f;
        p.pitchEnvDecay  = 0.02f;
        p.noiseLevel     = 0.8f;
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
    voices[DrumSynth::Snare].transmod.syncFromParams (
        voices[DrumSynth::Snare].params.filterCutoff,  voices[DrumSynth::Snare].params.filterResonance,
        voices[DrumSynth::Snare].params.ampDecay,       voices[DrumSynth::Snare].params.noiseLevel,
        voices[DrumSynth::Snare].params.driveAmount);

    // --- Clap (noise burst, square wave body) ---
    {
        auto& p = voices[DrumSynth::Clap].params;
        p.pitchHz        = 900.0f;
        p.oscShape       = 0.66f;         // square
        p.oscLevel       = 0.15f;
        p.noiseLevel     = 1.0f;
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
    voices[DrumSynth::Clap].transmod.syncFromParams (
        voices[DrumSynth::Clap].params.filterCutoff,  voices[DrumSynth::Clap].params.filterResonance,
        voices[DrumSynth::Clap].params.ampDecay,       voices[DrumSynth::Clap].params.noiseLevel,
        voices[DrumSynth::Clap].params.driveAmount);

    // --- Tom 1 (120 Hz, partial shaper, membrane mode) ---
    {
        auto& p = voices[DrumSynth::Tom1].params;
        p.pitchHz        = 120.0f;
        p.shaperEnabled  = true;
        p.partialPeak    = 1.0f;
        p.partialSpace   = 0.5f;
        p.partialRoll    = 0.6f;
        p.partialDecay   = 0.4f;
        p.membraneMode   = true;
        p.pitchEnvDepth  = 12.0f;
        p.pitchEnvDecay  = 0.03f;
        p.noiseLevel     = 0.1f;
        p.noiseDecay     = 0.05f;
        p.filterMode     = VP::FilterMode::LP;
        p.filterCutoff   = 600.0f;
        p.filterResonance= 0.2f;
        p.ampDecay       = 0.35f;
        p.outputGain     = 0.85f;
    }
    voices[DrumSynth::Tom1].transmod.syncFromParams (
        voices[DrumSynth::Tom1].params.filterCutoff,  voices[DrumSynth::Tom1].params.filterResonance,
        voices[DrumSynth::Tom1].params.ampDecay,       voices[DrumSynth::Tom1].params.noiseLevel,
        voices[DrumSynth::Tom1].params.driveAmount);

    // --- Tom 2 (90 Hz, deeper membrane) ---
    {
        auto& p = voices[DrumSynth::Tom2].params;
        p.pitchHz        = 90.0f;
        p.shaperEnabled  = true;
        p.partialPeak    = 1.0f;
        p.partialSpace   = 0.5f;
        p.partialRoll    = 0.6f;
        p.partialDecay   = 0.4f;
        p.membraneMode   = true;
        p.pitchEnvDepth  = 12.0f;
        p.pitchEnvDecay  = 0.03f;
        p.noiseLevel     = 0.1f;
        p.noiseDecay     = 0.05f;
        p.filterMode     = VP::FilterMode::LP;
        p.filterCutoff   = 500.0f;
        p.filterResonance= 0.2f;
        p.ampDecay       = 0.4f;
        p.outputGain     = 0.85f;
    }
    voices[DrumSynth::Tom2].transmod.syncFromParams (
        voices[DrumSynth::Tom2].params.filterCutoff,  voices[DrumSynth::Tom2].params.filterResonance,
        voices[DrumSynth::Tom2].params.ampDecay,       voices[DrumSynth::Tom2].params.noiseLevel,
        voices[DrumSynth::Tom2].params.driveAmount);

    // --- Closed Hat (808-style metallic cluster, short decay, choke group A) ---
    {
        auto& p = voices[DrumSynth::ClosedHat].params;
        p.pitchHz        = 8000.0f;
        p.metallic       = true;
        p.oscLevel       = 0.4f;
        p.noiseLevel     = 0.6f;
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
    voices[DrumSynth::ClosedHat].transmod.syncFromParams (
        voices[DrumSynth::ClosedHat].params.filterCutoff,  voices[DrumSynth::ClosedHat].params.filterResonance,
        voices[DrumSynth::ClosedHat].params.ampDecay,       voices[DrumSynth::ClosedHat].params.noiseLevel,
        voices[DrumSynth::ClosedHat].params.driveAmount);

    // --- Open Hat (same cluster, longer decay, choke group A) ---
    {
        auto& p = voices[DrumSynth::OpenHat].params;
        p.pitchHz        = 8000.0f;
        p.metallic       = true;
        p.oscLevel       = 0.35f;
        p.noiseLevel     = 0.7f;
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
    voices[DrumSynth::OpenHat].transmod.syncFromParams (
        voices[DrumSynth::OpenHat].params.filterCutoff,  voices[DrumSynth::OpenHat].params.filterResonance,
        voices[DrumSynth::OpenHat].params.ampDecay,       voices[DrumSynth::OpenHat].params.noiseLevel,
        voices[DrumSynth::OpenHat].params.driveAmount);

    // --- Cymbal (808 metallic, long decay, HP) ---
    {
        auto& p = voices[DrumSynth::Cymbal].params;
        p.pitchHz        = 5000.0f;
        p.metallic       = true;
        p.oscLevel       = 0.25f;
        p.noiseLevel     = 0.85f;
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
    voices[DrumSynth::Cymbal].transmod.syncFromParams (
        voices[DrumSynth::Cymbal].params.filterCutoff,  voices[DrumSynth::Cymbal].params.filterResonance,
        voices[DrumSynth::Cymbal].params.ampDecay,       voices[DrumSynth::Cymbal].params.noiseLevel,
        voices[DrumSynth::Cymbal].params.driveAmount);
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
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        const auto& p = voices[size_t (ch)].params;
        juce::ValueTree v ("Voice");
        v.setProperty ("ch",              ch,                              nullptr);
        v.setProperty ("pitchHz",         p.pitchHz,                      nullptr);
        v.setProperty ("oscShape",        p.oscShape,                     nullptr);
        v.setProperty ("metallic",        p.metallic,                     nullptr);
        v.setProperty ("shaperEnabled",   p.shaperEnabled,                nullptr);
        v.setProperty ("partialPeak",     p.partialPeak,                  nullptr);
        v.setProperty ("partialSpace",    p.partialSpace,                 nullptr);
        v.setProperty ("partialRoll",     p.partialRoll,                  nullptr);
        v.setProperty ("partialDecay",    p.partialDecay,                 nullptr);
        v.setProperty ("membraneMode",    p.membraneMode,                 nullptr);
        v.setProperty ("pitchEnvDepth",   p.pitchEnvDepth,                nullptr);
        v.setProperty ("pitchEnvDecay",   p.pitchEnvDecay,                nullptr);
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
        v.setProperty ("filterEnvAttack", p.filterEnvAttack,              nullptr);
        v.setProperty ("filterEnvHold",   p.filterEnvHold,                nullptr);
        v.setProperty ("filterEnvDecay",  p.filterEnvDecay,               nullptr);
        v.setProperty ("filterEnvDepth",  p.filterEnvDepth,               nullptr);
        v.setProperty ("ampAttack",       p.ampAttack,                    nullptr);
        v.setProperty ("ampHold",         p.ampHold,                      nullptr);
        v.setProperty ("ampDecay",        p.ampDecay,                     nullptr);
        v.setProperty ("lfo1Rate",        p.lfo1Rate,                     nullptr);
        v.setProperty ("lfo1Depth",       p.lfo1Depth,                    nullptr);
        v.setProperty ("lfo1Wave",        int (p.lfo1Wave),               nullptr);
        v.setProperty ("lfo2Rate",        p.lfo2Rate,                     nullptr);
        v.setProperty ("lfo2Depth",       p.lfo2Depth,                    nullptr);
        v.setProperty ("lfo2Wave",        int (p.lfo2Wave),               nullptr);
        v.setProperty ("fx1Type",         int (p.fx1Type),                nullptr);
        v.setProperty ("fx1Amount",       p.fx1Amount,                    nullptr);
        v.setProperty ("bitDepth",        p.bitDepth,                     nullptr);
        v.setProperty ("oscLevel",        p.oscLevel,                     nullptr);
        v.setProperty ("outputGain",      p.outputGain,                   nullptr);
        v.setProperty ("chokeGroup",      int (p.chokeGroup),             nullptr);
        state.addChild (v, -1, nullptr);
    }
    juce::MemoryOutputStream mos (destData, true);
    state.writeToStream (mos);
}

void DrumSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, size_t (sizeInBytes));
    if (!state.isValid() || state.getType() != juce::Identifier ("DrumSynthCell")) return;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto v = state.getChild (i);
        int  ch = int (v.getProperty ("ch", -1));
        if (ch < 0 || ch >= DrumSynth::NumChannels) continue;

        auto& p = voices[size_t (ch)].params;
        using VP = DrumSynth::VoiceParams;

        p.pitchHz        = float (v.getProperty ("pitchHz",        p.pitchHz));
        p.oscShape       = float (v.getProperty ("oscShape",       p.oscShape));
        p.metallic       = bool  (v.getProperty ("metallic",       p.metallic));
        p.shaperEnabled  = bool  (v.getProperty ("shaperEnabled",  p.shaperEnabled));
        p.partialPeak    = float (v.getProperty ("partialPeak",    p.partialPeak));
        p.partialSpace   = float (v.getProperty ("partialSpace",   p.partialSpace));
        p.partialRoll    = float (v.getProperty ("partialRoll",    p.partialRoll));
        p.partialDecay   = float (v.getProperty ("partialDecay",   p.partialDecay));
        p.membraneMode   = bool  (v.getProperty ("membraneMode",   p.membraneMode));
        p.pitchEnvDepth  = float (v.getProperty ("pitchEnvDepth",  p.pitchEnvDepth));
        p.pitchEnvDecay  = float (v.getProperty ("pitchEnvDecay",  p.pitchEnvDecay));
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
        p.filterEnvAttack= float (v.getProperty ("filterEnvAttack",p.filterEnvAttack));
        p.filterEnvHold  = float (v.getProperty ("filterEnvHold",  p.filterEnvHold));
        p.filterEnvDecay = float (v.getProperty ("filterEnvDecay", p.filterEnvDecay));
        p.filterEnvDepth = float (v.getProperty ("filterEnvDepth", p.filterEnvDepth));
        p.ampAttack      = float (v.getProperty ("ampAttack",      p.ampAttack));
        p.ampHold        = float (v.getProperty ("ampHold",        p.ampHold));
        p.ampDecay       = float (v.getProperty ("ampDecay",       p.ampDecay));
        p.lfo1Rate       = float (v.getProperty ("lfo1Rate",       p.lfo1Rate));
        p.lfo1Depth      = float (v.getProperty ("lfo1Depth",      p.lfo1Depth));
        p.lfo1Wave       = VP::LfoWave (int (v.getProperty ("lfo1Wave", int (p.lfo1Wave))));
        p.lfo2Rate       = float (v.getProperty ("lfo2Rate",       p.lfo2Rate));
        p.lfo2Depth      = float (v.getProperty ("lfo2Depth",      p.lfo2Depth));
        p.lfo2Wave       = VP::LfoWave (int (v.getProperty ("lfo2Wave", int (p.lfo2Wave))));
        p.fx1Type        = VP::FxDistType (int (v.getProperty ("fx1Type",  int (p.fx1Type))));
        p.fx1Amount      = float (v.getProperty ("fx1Amount",      p.fx1Amount));
        p.bitDepth       = float (v.getProperty ("bitDepth",       p.bitDepth));
        p.oscLevel       = float (v.getProperty ("oscLevel",       p.oscLevel));
        p.outputGain     = float (v.getProperty ("outputGain",     p.outputGain));
        p.chokeGroup     = DrumSynth::ChokeGroup (int (v.getProperty ("chokeGroup", int (p.chokeGroup))));

        voices[size_t (ch)].transmod.syncFromParams (
            p.filterCutoff, p.filterResonance,
            p.ampDecay, p.noiseLevel, p.driveAmount);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumSynthProcessor();
}
