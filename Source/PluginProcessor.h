#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "DrumVoice.h"

class DrumSynthProcessor : public juce::AudioProcessor
{
public:
    DrumSynthProcessor();
    ~DrumSynthProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DrumSynthCell"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // MIDI note -> channel index mapping (C2=Kick, D2=Snare, etc.)
    static int midiNoteToChannel (int midiNote) noexcept;

    DrumSynth::DrumVoice& getVoice (int channel) noexcept { return voices[size_t (channel)]; }

private:
    std::array<DrumSynth::DrumVoice, DrumSynth::NumChannels> voices;
    juce::AudioBuffer<float> monoMixBuf;

    void applyChokeGroups (int triggeredChannel);
    void handleMidiMessage (const juce::MidiMessage& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSynthProcessor)
};
