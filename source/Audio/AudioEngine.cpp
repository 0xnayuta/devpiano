#include "AudioEngine.h"

#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Plugin/PluginHost.h"
#include "Recording/RecordingEngine.h"

#include "Diagnostics/Log.h"
#include <cmath>

namespace {
constexpr auto warmupSeconds = 0.025;
constexpr auto playbackStartPreRollSeconds = 0.025;
} // namespace

int AudioEngine::calculateWarmupBlockCount(double sampleRate, int blockSize) noexcept {
    if (sampleRate <= 0.0 || blockSize <= 0) {
        return 1;
    }

    return juce::jmax(1, static_cast<int>(std::ceil(warmupSeconds * sampleRate / static_cast<double>(blockSize))));
}

int AudioEngine::calculatePlaybackStartPreRollBlockCount(double sampleRate, int blockSize) noexcept {
    if (sampleRate <= 0.0 || blockSize <= 0) {
        return 1;
    }

    return juce::jmax(
        1, static_cast<int>(std::ceil(playbackStartPreRollSeconds * sampleRate / static_cast<double>(blockSize))));
}

AudioEngine::AudioEngine() {
    adsrParameters.attack = 0.01f;
    adsrParameters.decay = 0.2f;
    adsrParameters.sustain = 0.8f;
    adsrParameters.release = 0.3f;

    rebuildSynth();
}

void AudioEngine::setPluginHost(PluginHost* host) noexcept {
    pluginHost = host;
}

void AudioEngine::setRecordingEngine(devpiano::recording::RecordingEngine* engine) noexcept {
    recordingEngine = engine;
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlockExpected;

    synth.setCurrentPlaybackSampleRate(sampleRate);
    midiCollector.reset(sampleRate);
    midiBuffer.clear();
    // Pre-allocate channels (covers stereo, multi-out, and spatial/ambisonic plugins up to 32+ channels).
    // The audio callback must never resize this buffer — heap allocation on the
    // real-time thread causes glitches.
    auto requiredChannels = 32;
    if (pluginHost != nullptr && pluginHost->hasLoadedPlugin()) {
        if (auto* instance = pluginHost->getInstance()) {
            requiredChannels
                = juce::jmax(requiredChannels,
                             juce::jmax(instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()));
        }
    }
    pluginBuffer.setSize(requiredChannels, juce::jmax(1, samplesPerBlockExpected), false, false, true);
    pluginBuffer.clear();

    const auto bytes = static_cast<size_t>(juce::jlimit(4096, 65536, samplesPerBlockExpected * 16));
    midiBuffer.ensureSize(bytes);

    updateAdsrOnVoices();

    if (pluginHost != nullptr && pluginHost->hasLoadedPlugin()) {
        pluginHost->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }

    discardWarmupInputState();
    warmupBlocksRemaining.store(calculateWarmupBlockCount(sampleRate, samplesPerBlockExpected),
                                std::memory_order_release);
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    if (bufferToFill.buffer == nullptr) {
        return;
    }

    bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);

    if (consumeWarmupBlockIfNeeded()) {
        return;
    }

    midiBuffer.clear();
    midiCollector.removeNextBlockOfMessages(midiBuffer, bufferToFill.numSamples);
    keyboardState.processNextMidiBuffer(midiBuffer, 0, bufferToFill.numSamples, true);
    injectPendingAllNotesOffIfNeeded();
    recordRealtimeMidiBufferIfNeeded(bufferToFill.numSamples);
    if (!consumePlaybackStartPreRollBlockIfNeeded()) {
        renderPlaybackEventsIfNeeded(recordingEngine != nullptr ? recordingEngine->getPlaybackPositionSamples() : 0,
                                     bufferToFill.numSamples);
    }

    auto renderedByPlugin = false;

    if (pluginHost != nullptr && pluginHost->hasLoadedPlugin()) {

        if (auto* instance = pluginHost->getInstance(); instance != nullptr && pluginHost->isPrepared()) {
            const auto requiredChannels = juce::jmax(
                1, juce::jmax(instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()));
            // Buffer is pre-allocated in prepareToPlay and should never need resizing here.
            // If this triggers, the audio device changed its block size without calling prepareToPlay
            // — which is a framework contract violation. Resize as a safety net in release builds.
            jassert(pluginBuffer.getNumChannels() >= requiredChannels);
            jassert(pluginBuffer.getNumSamples() >= bufferToFill.numSamples);
            if (pluginBuffer.getNumChannels() < requiredChannels
                || pluginBuffer.getNumSamples() < bufferToFill.numSamples) {
                pluginBuffer.setSize(requiredChannels, bufferToFill.numSamples, false, false, true);
                // 实时回调内只计数，日志由消息线程 consume 后输出（ERR-002）。
                pluginBufferResizeCount.fetch_add(1, std::memory_order_relaxed);
            }

            pluginBuffer.clear();
            instance->processBlock(pluginBuffer, midiBuffer);

            const auto outputChannels
                = juce::jmin(bufferToFill.buffer->getNumChannels(), instance->getTotalNumOutputChannels());
            for (auto channel = 0; channel < outputChannels; ++channel) {
                bufferToFill.buffer->copyFrom(channel, bufferToFill.startSample, pluginBuffer, channel, 0,
                                              bufferToFill.numSamples);
            }

            renderedByPlugin = true;
        }
    }

    if (!renderedByPlugin) {
        synth.renderNextBlock(*bufferToFill.buffer, midiBuffer, bufferToFill.startSample, bufferToFill.numSamples);
    }

    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples,
                                   masterGain.load(std::memory_order_relaxed));
}

void AudioEngine::releaseResources() {
    warmupBlocksRemaining.store(0, std::memory_order_release);
    playbackStartPreRollBlocksRemaining.store(0, std::memory_order_release);
    discardWarmupInputState();
    synth.allNotesOff(0, false);

    if (pluginHost != nullptr) {
        pluginHost->releaseResources();
    }
}

void AudioEngine::requestAllNotesOff() noexcept {
    allNotesOffPending.store(true, std::memory_order_release);
}

void AudioEngine::armPlaybackStartPreRoll(double sampleRate, int blockSize) noexcept {
    playbackStartPreRollBlocksRemaining.store(calculatePlaybackStartPreRollBlockCount(sampleRate, blockSize),
                                              std::memory_order_release);
}

void AudioEngine::setMasterGain(float newGain) {
    masterGain.store(juce::jlimit(0.0f, 1.0f, newGain), std::memory_order_relaxed);
}

int AudioEngine::consumePluginBufferResizeCount() noexcept {
    return pluginBufferResizeCount.exchange(0, std::memory_order_acq_rel);
}

void AudioEngine::setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) {
    adsrParameters.attack = juce::jmax(0.001f, attackSeconds);
    adsrParameters.decay = juce::jmax(0.001f, decaySeconds);
    adsrParameters.sustain = juce::jlimit(0.0f, 1.0f, sustainLevel);
    adsrParameters.release = juce::jmax(0.001f, releaseSeconds);
    updateAdsrOnVoices();
}

void AudioEngine::setPianoParameters(float brightness, float hammerHardness, float resonance) {
    pianoBrightness = juce::jlimit(0.0f, 1.0f, brightness);
    pianoHammerHardness = juce::jlimit(0.0f, 1.0f, hammerHardness);
    pianoResonance = juce::jlimit(0.0f, 1.0f, resonance);
    updatePianoParametersOnVoices();
}
void AudioEngine::setPlaybackTranspose(bool enabled, int semitoneOffset) noexcept {
    playbackTransposeEnabled.store(enabled, std::memory_order_release);
    playbackTransposeOffset.store(semitoneOffset, std::memory_order_release);
}

bool AudioEngine::isPlaybackTransposeEnabled() const noexcept {
    return playbackTransposeEnabled.load(std::memory_order_acquire);
}

int AudioEngine::getPlaybackTransposeOffset() const noexcept {
    return playbackTransposeOffset.load(std::memory_order_acquire);
}

void AudioEngine::setBuiltinSynthTone(BuiltinSynthTone tone) {
    if (builtinTone == tone) {
        return;
    }

    builtinTone = tone;
    rebuildSynth();
}

void AudioEngine::rebuildSynth() {
    synth.clearSounds();
    synth.clearVoices();

    if (builtinTone == BuiltinSynthTone::piano) {
        synth.addSound(new PianoSynthSound());
        for (auto index = 0; index < 8; ++index) {
            synth.addVoice(new PianoSynthVoice());
        }
    } else {
        synth.addSound(new SineSynthSound());
        for (auto index = 0; index < 8; ++index) {
            synth.addVoice(new SineSynthVoice());
        }
    }

    updateAdsrOnVoices();
    updatePianoParametersOnVoices();
}

void AudioEngine::updatePianoParametersOnVoices() {
    for (auto index = 0; index < synth.getNumVoices(); ++index) {
        if (auto* voice = dynamic_cast<PianoSynthVoice*>(synth.getVoice(index))) {
            voice->setPianoParameters(pianoBrightness, pianoHammerHardness, pianoResonance);
        }
    }
}

void AudioEngine::updateAdsrOnVoices() {
    for (auto index = 0; index < synth.getNumVoices(); ++index) {
        if (auto* sineVoice = dynamic_cast<SineSynthVoice*>(synth.getVoice(index))) {
            sineVoice->setAdsrParameters(adsrParameters);
        } else if (auto* pianoVoice = dynamic_cast<PianoSynthVoice*>(synth.getVoice(index))) {
            pianoVoice->setAdsrParameters(adsrParameters);
        }
    }
}

void AudioEngine::discardWarmupInputState() {
    keyboardState.reset();
    midiBuffer.clear();
    playbackVisualMidiBuffer.clear();
    midiCollector.reset(currentSampleRate);
    synth.allNotesOff(0, false);
}

bool AudioEngine::consumeWarmupBlockIfNeeded() {
    if (warmupBlocksRemaining.load(std::memory_order_acquire) <= 0) {
        return false;
    }

    warmupBlocksRemaining.fetch_sub(1, std::memory_order_acq_rel);
    discardWarmupInputState();
    return true;
}

bool AudioEngine::consumePlaybackStartPreRollBlockIfNeeded() {
    if (recordingEngine == nullptr || !recordingEngine->isPlaying()) {
        playbackStartPreRollBlocksRemaining.store(0, std::memory_order_release);
        return false;
    }

    if (playbackStartPreRollBlocksRemaining.load(std::memory_order_acquire) <= 0) {
        return false;
    }

    // Let plugin/synth render a few post-warmup blocks before timestamp-0 playback
    // events are scheduled. Do not advance RecordingEngine playback position here:
    // this is wall-clock arming time, not part of the imported MIDI timeline.
    playbackStartPreRollBlocksRemaining.fetch_sub(1, std::memory_order_acq_rel);
    playbackVisualMidiBuffer.clear();
    return true;
}

void AudioEngine::injectPendingAllNotesOffIfNeeded() {
    if (!allNotesOffPending.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    for (auto channel = 1; channel <= 16; ++channel) {
        keyboardState.allNotesOff(channel);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), 0); // sustain pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0); // all sound off
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
    }

    synth.allNotesOff(0, false);
}

void AudioEngine::recordRealtimeMidiBufferIfNeeded(int numSamples) {
    if (recordingEngine == nullptr || !recordingEngine->isRecording()) {
        return;
    }

    const auto blockStartSamples = recordingEngine->getCurrentPositionSamples();
    recordingEngine->recordMidiBufferBlock(midiBuffer, devpiano::recording::RecordingEventSource::realtimeMidiBuffer,
                                           blockStartSamples);
    recordingEngine->advanceRecordingPosition(numSamples);
}

void AudioEngine::renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples) {
    if (recordingEngine == nullptr || !recordingEngine->isPlaying()) {
        return;
    }

    playbackVisualMidiBuffer.clear();
    recordingEngine->renderPlaybackBlock(playbackVisualMidiBuffer, blockStartSamples, numSamples);

    // Apply real-time playback transposition if enabled (with GM Channel 10 drum bypass)
    const auto transposeEnabled = playbackTransposeEnabled.load(std::memory_order_acquire);
    const auto transposeOffset = playbackTransposeOffset.load(std::memory_order_acquire);

    if (transposeEnabled && transposeOffset != 0 && !playbackVisualMidiBuffer.isEmpty()) {
        juce::MidiBuffer transposedBuffer;
        for (const auto metadata : playbackVisualMidiBuffer) {
            auto msg = metadata.getMessage();
            if (msg.isNoteOnOrOff() && msg.getChannel() != 10) {
                const auto originalNote = msg.getNoteNumber();
                const auto transposedNote = juce::jlimit(0, 127, originalNote + transposeOffset);
                if (msg.isNoteOn()) {
                    transposedBuffer.addEvent(
                        juce::MidiMessage::noteOn(msg.getChannel(), transposedNote, msg.getFloatVelocity()),
                        metadata.samplePosition);
                } else {
                    transposedBuffer.addEvent(
                        juce::MidiMessage::noteOff(msg.getChannel(), transposedNote, msg.getFloatVelocity()),
                        metadata.samplePosition);
                }
            } else {
                transposedBuffer.addEvent(msg, metadata.samplePosition);
            }
        }
        playbackVisualMidiBuffer.swapWith(transposedBuffer);
    }

    // Playback events are generated inside the audio callback after the keyboard
    // state has already processed realtime input for this block. Feed only the
    // playback events into MidiKeyboardState for virtual-keyboard visualisation,
    // without injecting any additional keyboard-generated MIDI events back into
    // the stream. UI listeners must remain passive; this path only updates state.
    keyboardState.processNextMidiBuffer(playbackVisualMidiBuffer, 0, numSamples, false);
    midiBuffer.addEvents(playbackVisualMidiBuffer, 0, numSamples, 0);
    recordingEngine->advancePlaybackPosition(numSamples);
}
