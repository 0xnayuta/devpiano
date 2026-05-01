#pragma once

#include <JuceHeader.h>

#include <atomic>

class PluginHost;

namespace devpiano::recording
{
class RecordingEngine;
}

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine() = default;

    void setPluginHost(PluginHost* host) noexcept;
    void setRecordingEngine(devpiano::recording::RecordingEngine* engine) noexcept;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();
    void requestAllNotesOff() noexcept;

    void setMasterGain(float newGain);
    void setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds);

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    juce::MidiMessageCollector& getMidiCollector() noexcept { return midiCollector; }

private:
    class SimpleSineSound;
    class SimpleSineVoice;

    void rebuildSynth();
    void updateAdsrOnVoices();
    void injectPendingAllNotesOffIfNeeded();
    void recordRealtimeMidiBufferIfNeeded(int numSamples);
    void renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples);

    PluginHost* pluginHost = nullptr;
    devpiano::recording::RecordingEngine* recordingEngine = nullptr;
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;
    juce::MidiBuffer playbackVisualMidiBuffer;
    juce::AudioBuffer<float> pluginBuffer;

    juce::ADSR::Parameters adsrParameters;
    float masterGain = 0.8f;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    std::atomic_bool allNotesOffPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
