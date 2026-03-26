#pragma once

#include <JuceHeader.h>

class PluginHost;

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine() = default;

    void setPluginHost(PluginHost* host) noexcept;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();

    void setMasterGain(float newGain);
    void setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds);

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    juce::MidiMessageCollector& getMidiCollector() noexcept { return midiCollector; }

private:
    class SimpleSineSound;
    class SimpleSineVoice;

    void rebuildSynth();
    void updateAdsrOnVoices();

    PluginHost* pluginHost = nullptr;
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;
    juce::AudioBuffer<float> pluginBuffer;

    juce::ADSR::Parameters adsrParameters;
    float masterGain = 0.8f;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
