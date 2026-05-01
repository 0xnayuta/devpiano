#include "AudioEngine.h"

#include "Plugin/PluginHost.h"
#include "Recording/RecordingEngine.h"

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

void AudioEngine::setPluginHost(PluginHost* host) noexcept
{
    pluginHost = host;
}

void AudioEngine::setRecordingEngine(devpiano::recording::RecordingEngine* engine) noexcept
{
    recordingEngine = engine;
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlockExpected;

    synth.setCurrentPlaybackSampleRate(sampleRate);
    midiCollector.reset(sampleRate);
    midiBuffer.clear();
    pluginBuffer.setSize(2, juce::jmax(1, samplesPerBlockExpected), false, false, true);
    pluginBuffer.clear();

    const auto bytes = static_cast<size_t>(juce::jlimit(256, 65536, samplesPerBlockExpected * 16));
    midiBuffer.ensureSize(bytes);

    updateAdsrOnVoices();

    if (pluginHost != nullptr && pluginHost->hasLoadedPlugin())
        pluginHost->prepareToPlay(sampleRate, samplesPerBlockExpected);
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);

    midiBuffer.clear();
    midiCollector.removeNextBlockOfMessages(midiBuffer, bufferToFill.numSamples);
    keyboardState.processNextMidiBuffer(midiBuffer,
                                        0,
                                        bufferToFill.numSamples,
                                        true);
    injectPendingAllNotesOffIfNeeded();
    recordRealtimeMidiBufferIfNeeded(bufferToFill.numSamples);
    renderPlaybackEventsIfNeeded(recordingEngine != nullptr ? recordingEngine->getPlaybackPositionSamples() : 0,
                                  bufferToFill.numSamples);

    auto renderedByPlugin = false;

    if (pluginHost != nullptr && pluginHost->hasLoadedPlugin())
    {
        if (! pluginHost->isPrepared())
            pluginHost->prepareToPlay(currentSampleRate, currentBlockSize);

        if (auto* instance = pluginHost->getInstance(); instance != nullptr && pluginHost->isPrepared())
        {
            const auto requiredChannels = juce::jmax(1,
                                                     juce::jmax(instance->getTotalNumInputChannels(),
                                                                instance->getTotalNumOutputChannels()));
            if (pluginBuffer.getNumChannels() < requiredChannels || pluginBuffer.getNumSamples() < bufferToFill.numSamples)
                pluginBuffer.setSize(requiredChannels, bufferToFill.numSamples, false, false, true);

            pluginBuffer.clear();
            instance->processBlock(pluginBuffer, midiBuffer);

            const auto outputChannels = juce::jmin(bufferToFill.buffer->getNumChannels(), instance->getTotalNumOutputChannels());
            for (auto channel = 0; channel < outputChannels; ++channel)
                bufferToFill.buffer->copyFrom(channel,
                                              bufferToFill.startSample,
                                              pluginBuffer,
                                              channel,
                                              0,
                                              bufferToFill.numSamples);

            renderedByPlugin = true;
        }
    }

    if (! renderedByPlugin)
    {
        synth.renderNextBlock(*bufferToFill.buffer,
                              midiBuffer,
                              bufferToFill.startSample,
                              bufferToFill.numSamples);
    }

    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples, masterGain);
}

void AudioEngine::releaseResources()
{
    synth.allNotesOff(0, false);

    if (pluginHost != nullptr)
        pluginHost->releaseResources();
}

void AudioEngine::requestAllNotesOff() noexcept
{
    allNotesOffPending.store(true, std::memory_order_release);
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

void AudioEngine::injectPendingAllNotesOffIfNeeded()
{
    if (! allNotesOffPending.exchange(false, std::memory_order_acq_rel))
        return;

    for (auto channel = 1; channel <= 16; ++channel)
    {
        keyboardState.allNotesOff(channel);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), 0);   // sustain pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0);  // all sound off
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
    }

    synth.allNotesOff(0, false);
}

void AudioEngine::recordRealtimeMidiBufferIfNeeded(int numSamples)
{
    if (recordingEngine == nullptr || !recordingEngine->isRecording())
        return;

    const auto blockStartSamples = recordingEngine->getCurrentPositionSamples();
    recordingEngine->recordMidiBufferBlock(midiBuffer,
                                           devpiano::recording::RecordingEventSource::realtimeMidiBuffer,
                                           blockStartSamples);
    recordingEngine->advanceRecordingPosition(numSamples);
}

void AudioEngine::renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples)
{
    if (recordingEngine == nullptr || !recordingEngine->isPlaying())
        return;

    playbackVisualMidiBuffer.clear();
    recordingEngine->renderPlaybackBlock(playbackVisualMidiBuffer, blockStartSamples, numSamples);

    // Playback events are generated inside the audio callback after the keyboard
    // state has already processed realtime input for this block. Feed only the
    // playback events into MidiKeyboardState for virtual-keyboard visualisation,
    // without injecting any additional keyboard-generated MIDI events back into
    // the stream. UI listeners must remain passive; this path only updates state.
    keyboardState.processNextMidiBuffer(playbackVisualMidiBuffer,
                                        0,
                                        numSamples,
                                        false);
    midiBuffer.addEvents(playbackVisualMidiBuffer, 0, numSamples, 0);
    recordingEngine->advancePlaybackPosition(numSamples);
}
