#include "AudioEngine.h"

#include <cmath>

class AudioEngine::SimpleSineSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class AudioEngine::SimpleSineVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SimpleSineSound*>(sound) != nullptr;
    }

    void setAdsrParameters(const juce::ADSR::Parameters& parameters)
    {
        adsr.setParameters(parameters);
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        level = velocity * 0.2f;
        frequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        phase = 0.0;
        increment = static_cast<float>(juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / getSampleRate());

        adsr.setSampleRate(getSampleRate());
        adsr.noteOn();
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
            return;
        }

        adsr.reset();
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto envelope = adsr.getNextSample();
            if (envelope <= 0.0f && ! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }

            const auto value = static_cast<float>(std::sin(phase) * level * envelope);
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            const auto sampleIndex = startSample + sample;
            for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                outputBuffer.addSample(channel, sampleIndex, value);
        }
    }

private:
    double phase = 0.0;
    float increment = 0.0f;
    float frequency = 440.0f;
    float level = 0.0f;
    juce::ADSR adsr;
};

AudioEngine::AudioEngine()
{
    adsrParameters.attack = 0.01f;
    adsrParameters.decay = 0.2f;
    adsrParameters.sustain = 0.8f;
    adsrParameters.release = 0.3f;

    rebuildSynth();
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    midiCollector.reset(sampleRate);
    midiBuffer.clear();

    const auto bytes = static_cast<size_t>(juce::jlimit(256, 65536, samplesPerBlockExpected * 16));
    midiBuffer.ensureSize(bytes);

    updateAdsrOnVoices();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);

    midiBuffer.clear();
    midiCollector.removeNextBlockOfMessages(midiBuffer, bufferToFill.numSamples);
    keyboardState.processNextMidiBuffer(midiBuffer,
                                        bufferToFill.startSample,
                                        bufferToFill.numSamples,
                                        true);

    synth.renderNextBlock(*bufferToFill.buffer,
                          midiBuffer,
                          bufferToFill.startSample,
                          bufferToFill.numSamples);

    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples, masterGain);
}

void AudioEngine::releaseResources()
{
    synth.allNotesOff(0, false);
}

void AudioEngine::setMasterGain(float newGain)
{
    masterGain = juce::jlimit(0.0f, 1.0f, newGain);
}

void AudioEngine::setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds)
{
    adsrParameters.attack = juce::jmax(0.001f, attackSeconds);
    adsrParameters.decay = juce::jmax(0.001f, decaySeconds);
    adsrParameters.sustain = juce::jlimit(0.0f, 1.0f, sustainLevel);
    adsrParameters.release = juce::jmax(0.001f, releaseSeconds);
    updateAdsrOnVoices();
}

void AudioEngine::rebuildSynth()
{
    synth.clearSounds();
    synth.clearVoices();

    synth.addSound(new SimpleSineSound());
    for (auto index = 0; index < 8; ++index)
        synth.addVoice(new SimpleSineVoice());

    updateAdsrOnVoices();
}

void AudioEngine::updateAdsrOnVoices()
{
    for (auto index = 0; index < synth.getNumVoices(); ++index)
        if (auto* voice = dynamic_cast<SimpleSineVoice*>(synth.getVoice(index)))
            voice->setAdsrParameters(adsrParameters);
}
