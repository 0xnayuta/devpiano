#include "AudioEngine.h"

void AudioEngine::initialise()
{
}

void AudioEngine::shutdown()
{
}

void AudioEngine::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    buffer.clear(startSample, numSamples);
}
