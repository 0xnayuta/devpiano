#include "AudioEngine.h"

#include "Audio/InstrumentEndpoint.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Export/ExportFlowSupport.h"
#include "Plugin/PluginHost.h"
#include "Recording/RecordingEngine.h"

#include "Diagnostics/Log.h"
#include <cmath>

namespace {
constexpr auto warmupSeconds = 0.025;
constexpr auto playbackStartPreRollSeconds = 0.025;
// 音频回调缓冲区的最小通道预分配余量：覆盖立体声、多输出与空间/环绕插件。
constexpr auto minInstrumentBufferChannels = 32;
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
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlockExpected, std::memory_order_relaxed);
    synth.setCurrentPlaybackSampleRate(sampleRate);
    midiCollector.reset(sampleRate);
    midiBuffer.clear();
    // Pre-allocate channels (covers stereo, multi-out, and spatial/ambisonic plugins up to 32+ channels).
    // The audio callback must never resize this buffer — heap allocation on the
    // real-time thread causes glitches.
    const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(pluginHost);
    auto requiredChannels = minInstrumentBufferChannels;
    if (endpoint.isHostedPlugin()) {
        requiredChannels = juce::jmax(requiredChannels, endpoint.getChannelCount());
    }
    pluginBuffer.setSize(requiredChannels, juce::jmax(1, samplesPerBlockExpected), false, false, true);
    pluginBuffer.clear();

    const auto bytes = static_cast<size_t>(juce::jlimit(4096, 65536, samplesPerBlockExpected * 16));
    midiBuffer.ensureSize(bytes);
    playbackVisualMidiBuffer.ensureSize(bytes);
    playbackTransposedMidiBuffer.ensureSize(bytes);
    syncPedalTempBuffer.ensureSize(bytes);
    applyPendingParametersIfNeeded();
    roomReverb.prepare(sampleRate);

    if (endpoint.isHostedPlugin()) {
        pluginHost->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }

    discardWarmupInputState();
    warmupBlocksRemaining.store(calculateWarmupBlockCount(sampleRate, samplesPerBlockExpected),
                                std::memory_order_release);
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    juce::ScopedNoDenormals noDenormals;
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
    syncPedalProcessor.processMidiBlock(midiBuffer, syncPedalTempBuffer);
    injectPendingAllNotesOffIfNeeded();
    recordRealtimeMidiBufferIfNeeded(bufferToFill.numSamples);
    if (!consumePlaybackStartPreRollBlockIfNeeded()) {
        renderPlaybackEventsIfNeeded(recordingEngine != nullptr ? recordingEngine->getPlaybackPositionSamples() : 0,
                                     bufferToFill.numSamples);
    }
    applyPendingParametersIfNeeded();
    auto renderedByPlugin = false;

    const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(pluginHost);
    if (endpoint.isHostedPlugin() && endpoint.hostedInstanceReady) {
        auto* instance = endpoint.hostedInstance;
        const auto requiredChannels = juce::jmax(1, endpoint.getChannelCount());
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

    if (!renderedByPlugin) {
        synth.renderNextBlock(*bufferToFill.buffer, midiBuffer, bufferToFill.startSample, bufferToFill.numSamples);
    }
    // 物理房间混响算法网络 (Phase 31-B, RoomReverbEngine: Studio / Chamber / Concert Hall)
    if (bufferToFill.buffer->getNumChannels() >= 2) {
        roomReverb.processStereo(bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample),
                                 bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample),
                                 bufferToFill.numSamples);
    }

    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples,
                                   masterGain.load(std::memory_order_relaxed));

    // Master bus soft-knee ceiling guard (PERF-004: shared helper across realtime audio & export)
    devpiano::exporting::applyMasterSoftLimiter(*bufferToFill.buffer, bufferToFill.startSample,
                                                bufferToFill.numSamples);
}

void AudioEngine::releaseResources() {
    warmupBlocksRemaining.store(0, std::memory_order_release);
    playbackStartPreRollBlocksRemaining.store(0, std::memory_order_release);
    discardWarmupInputState();
    synth.allNotesOff(0, false);

    roomReverb.reset();
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
void AudioEngine::sendController(int channel, int controllerType, int value) {
    if (controllerType == 64) {
        syncPedalProcessor.setPedalDown(value >= 64);
    }
    auto msg = juce::MidiMessage::controllerEvent(channel, controllerType, value);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void AudioEngine::setMasterGain(float newGain) {
    masterGain.store(juce::jlimit(0.0f, 1.0f, newGain), std::memory_order_relaxed);
}

int AudioEngine::consumePluginBufferResizeCount() noexcept {
    return pluginBufferResizeCount.exchange(0, std::memory_order_acq_rel);
}

void AudioEngine::setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) {
    pendingAttack.store(juce::jmax(0.001f, attackSeconds), std::memory_order_relaxed);
    pendingDecay.store(juce::jmax(0.001f, decaySeconds), std::memory_order_relaxed);
    pendingSustain.store(juce::jlimit(0.0f, 1.0f, sustainLevel), std::memory_order_relaxed);
    pendingRelease.store(juce::jmax(0.001f, releaseSeconds), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}

void AudioEngine::setPianoParameters(float brightness, float hammerHardness, float resonance) {
    pendingBrightness.store(juce::jlimit(0.0f, 1.0f, brightness), std::memory_order_relaxed);
    pendingHammerHardness.store(juce::jlimit(0.0f, 1.0f, hammerHardness), std::memory_order_relaxed);
    pendingResonance.store(juce::jlimit(0.0f, 1.0f, resonance), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}
void AudioEngine::setPlaybackTranspose(bool enabled, int semitoneOffset, std::uint16_t channelFollowKeyMask) noexcept {
    playbackTransposeEnabled.store(enabled, std::memory_order_release);
    playbackTransposeOffset.store(semitoneOffset, std::memory_order_release);
    playbackChannelFollowKeyMask.store(channelFollowKeyMask, std::memory_order_release);
}

bool AudioEngine::isPlaybackTransposeEnabled() const noexcept {
    return playbackTransposeEnabled.load(std::memory_order_acquire);
}

int AudioEngine::getPlaybackTransposeOffset() const noexcept {
    return playbackTransposeOffset.load(std::memory_order_acquire);
}

std::uint16_t AudioEngine::getPlaybackChannelFollowKeyMask() const noexcept {
    return playbackChannelFollowKeyMask.load(std::memory_order_acquire);
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
            auto* voice = new PianoSynthVoice();
            voice->setVoiceIndex(index);
            synth.addVoice(voice);
        }
    } else {
        synth.addSound(new SineSynthSound());
        for (auto index = 0; index < 8; ++index) {
            synth.addVoice(new SineSynthVoice());
        }
    }

    parametersNeedUpdate.store(true, std::memory_order_release);
    applyPendingParametersIfNeeded();
}

void AudioEngine::setLidPosition(LidPosition position) {
    pendingLidPosition.store(static_cast<std::uint8_t>(position), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}
void AudioEngine::setTemperament(Temperament temperament) {
    pendingTemperament.store(static_cast<std::uint8_t>(temperament), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}

void AudioEngine::setReferencePitchA4(double pitch) {
    pendingReferencePitchA4.store(devpiano::audio::TemperamentEngine::clampReferencePitch(pitch),
                                  std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}
void AudioEngine::setSoundPerspective(SoundPerspective perspective) {
    pendingSoundPerspective.store(static_cast<std::uint8_t>(perspective), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}
void AudioEngine::setReverbSpace(ReverbSpace space) {
    pendingReverbSpace.store(static_cast<std::uint8_t>(space), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}

void AudioEngine::setReverbWet(float wetLevel) {
    pendingReverbWet.store(std::clamp(wetLevel, 0.0f, 1.0f), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}
void AudioEngine::setPedalNoiseLevel(float level) {
    pendingPedalNoiseLevel.store(std::clamp(level, 0.0f, 1.0f), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}
void AudioEngine::setFeltAgeingAmount(float amount) {
    pendingFeltAgeingAmount.store(std::clamp(amount, 0.0f, 1.0f), std::memory_order_relaxed);
    parametersNeedUpdate.store(true, std::memory_order_release);
}

void AudioEngine::applyPendingParametersIfNeeded() {
    if (!parametersNeedUpdate.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    const auto attack = pendingAttack.load(std::memory_order_relaxed);
    const auto decay = pendingDecay.load(std::memory_order_relaxed);
    const auto sustain = pendingSustain.load(std::memory_order_relaxed);
    const auto release = pendingRelease.load(std::memory_order_relaxed);
    const auto brightness = pendingBrightness.load(std::memory_order_relaxed);
    const auto hammerHardness = pendingHammerHardness.load(std::memory_order_relaxed);
    const auto resonance = pendingResonance.load(std::memory_order_relaxed);
    const auto lid = static_cast<LidPosition>(pendingLidPosition.load(std::memory_order_relaxed));
    const auto temperament = static_cast<Temperament>(pendingTemperament.load(std::memory_order_relaxed));
    const auto refPitch = pendingReferencePitchA4.load(std::memory_order_relaxed);
    const auto perspective = static_cast<SoundPerspective>(pendingSoundPerspective.load(std::memory_order_relaxed));
    const auto revSpace = static_cast<ReverbSpace>(pendingReverbSpace.load(std::memory_order_relaxed));
    const auto revWet = pendingReverbWet.load(std::memory_order_relaxed);
    const auto pedalNoise = pendingPedalNoiseLevel.load(std::memory_order_relaxed);
    const auto feltAgeing = pendingFeltAgeingAmount.load(std::memory_order_relaxed);

    adsrParameters = { attack, decay, sustain, release };
    pianoBrightness = brightness;
    pianoHammerHardness = hammerHardness;
    pianoResonance = resonance;
    pianoLidPosition = lid;
    pianoTemperament = temperament;
    pianoReferencePitchA4 = refPitch;
    pianoSoundPerspective = perspective;
    pianoReverbSpace = revSpace;
    pianoReverbWet = revWet;
    pianoPedalNoiseLevel = pedalNoise;
    pianoFeltAgeingAmount = feltAgeing;
    roomReverb.setSpace(pianoReverbSpace);
    roomReverb.setWetLevel(pianoReverbWet);

    updateAdsrOnVoices();
    updatePianoParametersOnVoices();
}

void AudioEngine::updatePianoParametersOnVoices() {
    for (auto index = 0; index < synth.getNumVoices(); ++index) {
        if (auto* voice = dynamic_cast<PianoSynthVoice*>(synth.getVoice(index))) {
            voice->setPianoParameters(pianoBrightness, pianoHammerHardness, pianoResonance);
            voice->setLidPosition(static_cast<PianoSynthVoice::LidPosition>(pianoLidPosition));
            voice->setTemperament(pianoTemperament);
            voice->setReferencePitchA4(pianoReferencePitchA4);
            voice->setSoundPerspective(pianoSoundPerspective);
            voice->setPedalNoiseLevel(pianoPedalNoiseLevel);
            voice->setFeltAgeingAmount(pianoFeltAgeingAmount);
        } else if (auto* sineVoice = dynamic_cast<SineSynthVoice*>(synth.getVoice(index))) {
            sineVoice->setTemperament(pianoTemperament);
            sineVoice->setReferencePitchA4(pianoReferencePitchA4);
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
    midiCollector.reset(currentSampleRate.load(std::memory_order_relaxed));
    synth.allNotesOff(0, false);
    roomReverb.reset();
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
    roomReverb.reset();
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

    // Apply real-time playback transposition if enabled (per 16-channel followKey mask)
    const auto transposeEnabled = playbackTransposeEnabled.load(std::memory_order_acquire);
    const auto transposeOffset = playbackTransposeOffset.load(std::memory_order_acquire);
    const auto followMask = playbackChannelFollowKeyMask.load(std::memory_order_acquire);

    if (transposeEnabled && transposeOffset != 0 && !playbackVisualMidiBuffer.isEmpty()) {
        playbackTransposedMidiBuffer.clear();
        for (const auto metadata : playbackVisualMidiBuffer) {
            auto msg = metadata.getMessage();
            const auto chIdx = juce::jlimit(0, 15, msg.getChannel() - 1);
            const bool channelFollows = (followMask & (1U << chIdx)) != 0;

            if (msg.isNoteOnOrOff() && channelFollows) {
                const auto originalNote = msg.getNoteNumber();
                const auto transposedNote = juce::jlimit(0, 127, originalNote + transposeOffset);
                if (msg.isNoteOn()) {
                    playbackTransposedMidiBuffer.addEvent(
                        juce::MidiMessage::noteOn(msg.getChannel(), transposedNote, msg.getFloatVelocity()),
                        metadata.samplePosition);
                } else {
                    playbackTransposedMidiBuffer.addEvent(
                        juce::MidiMessage::noteOff(msg.getChannel(), transposedNote, msg.getFloatVelocity()),
                        metadata.samplePosition);
                }
            } else {
                playbackTransposedMidiBuffer.addEvent(msg, metadata.samplePosition);
            }
        }
        playbackVisualMidiBuffer.swapWith(playbackTransposedMidiBuffer);
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
