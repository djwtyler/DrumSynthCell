#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

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

    // --- Master bus ---
    float masterVolume    = 1.0f;   // linear gain, 0..1.5 (some headroom above unity)
    bool  limiterEnabled  = true;   // soft-clip (tanh) limiter on the master bus

    // Post-volume/limiter peak of the mono mix for this block, written by
    // the audio thread and read by the UI for the master meter. Benign
    // single-writer/single-reader race; never used for DSP.
    float getMasterPeakLevel() const noexcept { return masterPeakLevel.load (std::memory_order_relaxed); }

private:
    std::array<DrumSynth::DrumVoice, DrumSynth::NumChannels> voices;
    juce::AudioBuffer<float> monoMixBuf;
    std::atomic<float> masterPeakLevel { 0.0f };

    void applyChokeGroups (int triggeredChannel);
    void handleMidiMessage (const juce::MidiMessage& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSynthProcessor)
};
