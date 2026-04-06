#pragma once

#include <JuceHeader.h>

class AudioEngine
{
public:
    AudioEngine() = default;
    ~AudioEngine() = default;

    void initialise();
    void shutdown();

    // Called from audio callback
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
