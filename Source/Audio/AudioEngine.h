#pragma once

#include <JuceHeader.h>

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine() = default;

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

    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;

    juce::ADSR::Parameters adsrParameters;
    float masterGain = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
