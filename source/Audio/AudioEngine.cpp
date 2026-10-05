#include "AudioEngine.h"

#include "Audio/InstrumentEndpoint.h"
#include "Audio/OrderedMidi.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Export/ExportFlowSupport.h"
#include "Plugin/PluginHost.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"

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
    keyboardState.addListener(this);
    presetBoundaries.reserve(2);
    rebuildSynth();
}

AudioEngine::~AudioEngine() {
    keyboardState.removeListener(this);
}

void AudioEngine::setPluginHost(PluginHost* host) noexcept {
    pluginHost = host;
}

void AudioEngine::setRecordingEngine(devpiano::recording::RecordingEngine* engine) noexcept {
    recordingEngine = engine;
    if (recordingEngine != nullptr) {
        recordingEngine->setPlaybackBlockSize(currentBlockSize.load(std::memory_order_relaxed));
    }
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlockExpected, std::memory_order_relaxed);
    if (recordingEngine != nullptr) {
        recordingEngine->setPlaybackBlockSize(samplesPerBlockExpected);
        recordingEngine->prepareForAudioDevice(sampleRate);
    }
    synth.setCurrentPlaybackSampleRate(sampleRate);
    sineSynth.setCurrentPlaybackSampleRate(sampleRate);
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
    preparedPluginChannels = requiredChannels;
    pluginView.setDataToReferTo(pluginBuffer.getArrayOfWritePointers(), requiredChannels,
                                juce::jmax(1, samplesPerBlockExpected));
    builtinChannelPointers.resize(static_cast<std::size_t>(requiredChannels));
    builtinView.setDataToReferTo(pluginBuffer.getArrayOfWritePointers(), requiredChannels,
                                 juce::jmax(1, samplesPerBlockExpected));

    preparePlaybackResources();
    applyPendingParametersIfNeeded();
    roomReverb.prepare(sampleRate);
    metronomeProcessor.prepareToPlay(sampleRate);

    if (endpoint.isHostedPlugin()) {
        pluginHost->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }

    const bool isCountIn
        = (recordingEngine != nullptr && recordingEngine->getState() == devpiano::recording::RecordingState::countingIn)
        || metronomeProcessor.isCountInArmed();
    const bool isTransportActive
        = (recordingEngine != nullptr
           && (recordingEngine->isRecording()
               || recordingEngine->getState() == devpiano::recording::RecordingState::recordingPaused
               || recordingEngine->needsPlaybackRender()))
        || isCountIn;
    if (!isTransportActive) {
        discardWarmupInputState();
    }
    warmupBlocksRemaining.store(calculateWarmupBlockCount(sampleRate, samplesPerBlockExpected),
                                std::memory_order_release);
}

void AudioEngine::preparePlaybackResources() {
    preparedMidiCapacity = recordingEngine != nullptr ? recordingEngine->getPlaybackMidiCapacityBytes() : 131072;
    if (recordingEngine != nullptr) {
        playbackIdentityTracker.prepare(recordingEngine->getPlaybackGeneration(),
                                        recordingEngine->getPlaybackNoteOnCount());
        presetBoundaries.reserve(std::max<std::size_t>(2, recordingEngine->getScheduledPresetChanges().capacity()));
    }
    midiBuffer.ensureSize(preparedMidiCapacity);
    playbackVisualMidiBuffer.ensureSize(preparedMidiCapacity);
    playbackTransposedMidiBuffer.ensureSize(preparedMidiCapacity);
    syncPedalTempBuffer.ensureSize(preparedMidiCapacity);
    segmentMidiBuffer.ensureSize(preparedMidiCapacity);
    boundaryMidiBuffer.ensureSize(preparedMidiCapacity);
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    juce::ScopedNoDenormals noDenormals;
    if (bufferToFill.buffer == nullptr) {
        return;
    }
    const auto available = bufferToFill.buffer->getNumSamples();
    if (bufferToFill.startSample < 0 || bufferToFill.numSamples < 0 || bufferToFill.startSample > available
        || bufferToFill.numSamples > available - bufferToFill.startSample) {
        pluginBufferResizeCount.fetch_add(1, std::memory_order_relaxed);
        allNotesOffPending.store(true, std::memory_order_release);
        return;
    }
    bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);
    if (bufferToFill.numSamples == 0) {
        return;
    }
    const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(pluginHost);
    if (bufferToFill.numSamples > currentBlockSize.load(std::memory_order_relaxed)
        || bufferToFill.buffer->getNumChannels() > preparedPluginChannels
        || (endpoint.isHostedPluginReady() && endpoint.getChannelCount() > preparedPluginChannels)) {
        pluginBufferResizeCount.fetch_add(1, std::memory_order_relaxed);
        allNotesOffPending.store(true, std::memory_order_release);
        return;
    }
    if (recordingEngine != nullptr && recordingEngine->needsPlaybackRender()
        && recordingEngine->getPlaybackMidiCapacityBytes() > preparedMidiCapacity) {
        realtimeOverflowCount.fetch_add(1, std::memory_order_relaxed);
        allNotesOffPending.store(true, std::memory_order_release);
        return;
    }
    const bool isCountIn = metronomeProcessor.isCountInArmed()
        || (recordingEngine != nullptr
            && recordingEngine->getState() == devpiano::recording::RecordingState::countingIn);
    const bool isTransportActive = isCountIn
        || (recordingEngine != nullptr
            && (recordingEngine->isRecording()
                || recordingEngine->getState() == devpiano::recording::RecordingState::recordingPaused
                || recordingEngine->needsPlaybackRender()));
    muteInstrumentDuringBlock = warmupBlocksRemaining.load(std::memory_order_acquire) > 0;
    if (muteInstrumentDuringBlock) {
        warmupBlocksRemaining.fetch_sub(1, std::memory_order_acq_rel);
        if (!isTransportActive) {
            discardWarmupInputState();
            return;
        }
    }
    applyPendingParametersIfNeeded();
    midiBuffer.clear();
    presetBoundaries.clear();
    const auto commands = recordingEngine != nullptr ? recordingEngine->applyPendingTransportCommands(midiBuffer)
                                                     : devpiano::recording::RecordingEngine::TransportCommandResult {};
    if (commands.seekApplied || commands.stopApplied) {
        playbackIdentityTracker.resetOwnership();
        syncPedalProcessor.reset();
        clearDisplayNotes();
        synth.allNotesOff(0, false);
        sineSynth.allNotesOff(0, false);
        roomReverb.reset();
    }
    collectLiveMidi(bufferToFill.numSamples);
    syncPedalProcessor.processMidiBlock(midiBuffer, syncPedalTempBuffer);
    injectPendingAllNotesOffIfNeeded();
    recordRealtimeMidiBufferIfNeeded(bufferToFill.numSamples);
    const auto preRollPending = playbackStartPreRollBlocksRemaining.load(std::memory_order_acquire) > 0;
    if (!(muteInstrumentDuringBlock && preRollPending) && !consumePlaybackStartPreRollBlockIfNeeded()) {
        renderPlaybackEventsIfNeeded(recordingEngine != nullptr ? recordingEngine->getPlaybackPositionSamples() : 0,
                                     bufferToFill.numSamples);
    }
    publishMidiForDisplay(midiBuffer);
    segmentMidiBuffer.clear();
    auto cursor = midiBuffer.begin();
    const auto end = midiBuffer.end();
    std::size_t eventIndex = 0;
    int segmentStart = 0;
    for (const auto& boundary : presetBoundaries) {
        boundaryMidiBuffer.clear();
        while (cursor != end && eventIndex < boundary.midiEventCount) {
            const auto metadata = *cursor;
            if (metadata.samplePosition < boundary.sampleOffset) {
                devpiano::audio::appendOrderedMidi(segmentMidiBuffer, metadata.data, metadata.numBytes,
                                                   metadata.samplePosition - segmentStart);
            } else {
                devpiano::audio::appendOrderedMidi(boundaryMidiBuffer, metadata.data, metadata.numBytes, 0);
            }
            ++cursor;
            ++eventIndex;
        }
        renderInstrumentSegment(bufferToFill, segmentStart, boundary.sampleOffset - segmentStart);
        segmentStart = boundary.sampleOffset;
        for (const auto metadata : boundaryMidiBuffer) {
            devpiano::audio::appendOrderedMidi(segmentMidiBuffer, metadata.data, metadata.numBytes, 0);
        }
        renderInstrumentSegment(bufferToFill, segmentStart, 0);
        applyAcousticSnapshot(*boundary.acoustic, true);
    }
    for (; cursor != end; ++cursor) {
        const auto metadata = *cursor;
        devpiano::audio::appendOrderedMidi(segmentMidiBuffer, metadata.data, metadata.numBytes,
                                           metadata.samplePosition - segmentStart);
    }
    renderInstrumentSegment(bufferToFill, segmentStart, bufferToFill.numSamples - segmentStart);
}

void AudioEngine::renderInstrumentSegment(const juce::AudioSourceChannelInfo& output, int offset, int numSamples) {
    const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(pluginHost);
    if (endpoint.isHostedPluginReady()) {
        if (numSamples == 0) {
            return;
        }
        pluginView.setDataToReferTo(pluginBuffer.getArrayOfWritePointers(), preparedPluginChannels, numSamples);
        pluginView.clear();
        endpoint.hostedInstance->processBlock(pluginView, segmentMidiBuffer);
        const auto outputChannels
            = std::min(output.buffer->getNumChannels(), endpoint.hostedInstance->getTotalNumOutputChannels());
        for (int channel = 0; channel < outputChannels; ++channel) {
            output.buffer->copyFrom(channel, output.startSample + offset, pluginView, channel, 0, numSamples);
        }
    } else {
        for (int channel = 0; channel < preparedPluginChannels; ++channel) {
            builtinChannelPointers[static_cast<std::size_t>(channel)] = channel < output.buffer->getNumChannels()
                ? output.buffer->getWritePointer(channel, output.startSample + offset)
                : pluginBuffer.getWritePointer(channel);
        }
        builtinView.setDataToReferTo(builtinChannelPointers.data(), preparedPluginChannels, numSamples);
        activeSynth->renderNextBlock(builtinView, segmentMidiBuffer, 0, numSamples);
    }
    segmentMidiBuffer.clear();
    if (numSamples == 0) {
        return;
    }
    const auto start = output.startSample + offset;
    if (output.buffer->getNumChannels() >= 2) {
        roomReverb.processStereo(output.buffer->getWritePointer(0, start), output.buffer->getWritePointer(1, start),
                                 numSamples);
    }
    if (muteInstrumentDuringBlock) {
        output.buffer->clear(start, numSamples);
    }
    metronomeProcessor.processAndMix(output.buffer, start, numSamples);
    output.buffer->applyGain(start, numSamples, activeMasterGain);
    devpiano::exporting::applyMasterSoftLimiter(*output.buffer, start, numSamples);
}

void AudioEngine::releaseResources() {
    warmupBlocksRemaining.store(0, std::memory_order_release);
    playbackStartPreRollBlocksRemaining.store(0, std::memory_order_release);
    const bool isCountIn
        = (recordingEngine != nullptr && recordingEngine->getState() == devpiano::recording::RecordingState::countingIn)
        || metronomeProcessor.isCountInArmed();
    if (!isCountIn) {
        discardWarmupInputState();
        metronomeProcessor.reset();
    }
    synth.allNotesOff(0, false);
    sineSynth.allNotesOff(0, false);

    roomReverb.reset();
    if (pluginHost != nullptr) {
        pluginHost->releaseResources();
    }
}

void AudioEngine::requestAllNotesOff() noexcept {
    syncPedalProcessor.reset();
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
    enqueueLiveMidi(juce::MidiMessage::controllerEvent(channel, controllerType, value));
}

void AudioEngine::setMasterGain(float newGain) {
    masterGain.store(juce::jlimit(0.0f, 1.0f, newGain), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(gainParameter, std::memory_order_release);
}

int AudioEngine::consumePluginBufferResizeCount() noexcept {
    return pluginBufferResizeCount.exchange(0, std::memory_order_acq_rel);
}

void AudioEngine::setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) {
    pendingAttack.store(juce::jmax(0.001f, attackSeconds), std::memory_order_relaxed);
    pendingDecay.store(juce::jmax(0.001f, decaySeconds), std::memory_order_relaxed);
    pendingSustain.store(juce::jlimit(0.0f, 1.0f, sustainLevel), std::memory_order_relaxed);
    pendingRelease.store(juce::jmax(0.001f, releaseSeconds), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(adsrParameter, std::memory_order_release);
}

void AudioEngine::setPianoParameters(float brightness, float hammerHardness, float resonance) {
    pendingBrightness.store(juce::jlimit(0.0f, 1.0f, brightness), std::memory_order_relaxed);
    pendingHammerHardness.store(juce::jlimit(0.0f, 1.0f, hammerHardness), std::memory_order_relaxed);
    pendingResonance.store(juce::jlimit(0.0f, 1.0f, resonance), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(pianoParameter, std::memory_order_release);
}
void AudioEngine::setPlaybackTranspose(bool enabled, int semitoneOffset, std::uint16_t channelFollowKeyMask) noexcept {
    playbackTransposeEnabled.store(enabled, std::memory_order_release);
    playbackTransposeOffset.store(semitoneOffset, std::memory_order_release);
    playbackChannelFollowKeyMask.store(channelFollowKeyMask, std::memory_order_release);
    pendingParameterMask.fetch_or(transposeParameter, std::memory_order_release);
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
    builtinTone.store(tone, std::memory_order_relaxed);
    pendingParameterMask.fetch_or(toneParameter, std::memory_order_release);
}

void AudioEngine::rebuildSynth() {
    synth.clearSounds();
    synth.clearVoices();
    sineSynth.clearSounds();
    sineSynth.clearVoices();
    synth.addSound(new PianoSynthSound());
    sineSynth.addSound(new SineSynthSound());
    for (int index = 0; index < 8; ++index) {
        auto* voice = new PianoSynthVoice();
        voice->setVoiceIndex(index);
        synth.addVoice(voice);
        sineSynth.addVoice(new SineSynthVoice());
    }
    pendingParameterMask.store(allParameters, std::memory_order_release);
    applyPendingParametersIfNeeded();
}

void AudioEngine::setLidPosition(LidPosition position) {
    pendingLidPosition.store(static_cast<std::uint8_t>(position), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(lidParameter, std::memory_order_release);
}
void AudioEngine::setTemperament(Temperament temperament) {
    pendingTemperament.store(static_cast<std::uint8_t>(temperament), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(temperamentParameter, std::memory_order_release);
}

void AudioEngine::setReferencePitchA4(double pitch) {
    pendingReferencePitchA4.store(devpiano::audio::TemperamentEngine::clampReferencePitch(pitch),
                                  std::memory_order_relaxed);
    pendingParameterMask.fetch_or(pitchParameter, std::memory_order_release);
}
void AudioEngine::setSoundPerspective(SoundPerspective perspective) {
    pendingSoundPerspective.store(static_cast<std::uint8_t>(perspective), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(perspectiveParameter, std::memory_order_release);
}
void AudioEngine::setReverbSpace(ReverbSpace space) {
    pendingReverbSpace.store(static_cast<std::uint8_t>(space), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(spaceParameter, std::memory_order_release);
}

void AudioEngine::setReverbWet(float wetLevel) {
    pendingReverbWet.store(std::clamp(wetLevel, 0.0f, 1.0f), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(wetParameter, std::memory_order_release);
}
void AudioEngine::setPedalNoiseLevel(float level) {
    pendingPedalNoiseLevel.store(std::clamp(level, 0.0f, 1.0f), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(noiseParameter, std::memory_order_release);
}
void AudioEngine::setFeltAgeingAmount(float amount) {
    pendingFeltAgeingAmount.store(std::clamp(amount, 0.0f, 1.0f), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(feltParameter, std::memory_order_release);
}

devpiano::audio::AcousticSnapshot AudioEngine::captureAcousticSnapshot() const noexcept {
    devpiano::audio::AcousticSnapshot snapshot;
    snapshot.builtinTone = static_cast<devpiano::core::BuiltinTone>(builtinTone.load(std::memory_order_relaxed));
    snapshot.masterGain = masterGain.load(std::memory_order_relaxed);
    snapshot.adsr = { pendingAttack.load(std::memory_order_relaxed), pendingDecay.load(std::memory_order_relaxed),
                      pendingSustain.load(std::memory_order_relaxed), pendingRelease.load(std::memory_order_relaxed) };
    snapshot.brightness = pendingBrightness.load(std::memory_order_relaxed);
    snapshot.hammerHardness = pendingHammerHardness.load(std::memory_order_relaxed);
    snapshot.resonance = pendingResonance.load(std::memory_order_relaxed);
    snapshot.lidPosition = pendingLidPosition.load(std::memory_order_relaxed);
    snapshot.temperament = static_cast<Temperament>(pendingTemperament.load(std::memory_order_relaxed));
    snapshot.referencePitchA4 = pendingReferencePitchA4.load(std::memory_order_relaxed);
    snapshot.soundPerspective = static_cast<SoundPerspective>(pendingSoundPerspective.load(std::memory_order_relaxed));
    snapshot.reverbSpace = static_cast<ReverbSpace>(pendingReverbSpace.load(std::memory_order_relaxed));
    snapshot.reverbWet = pendingReverbWet.load(std::memory_order_relaxed);
    snapshot.pedalNoiseLevel = pendingPedalNoiseLevel.load(std::memory_order_relaxed);
    snapshot.feltAgeingAmount = pendingFeltAgeingAmount.load(std::memory_order_relaxed);
    snapshot.sustainPolicy = syncPedalProcessor.getPolicy();
    snapshot.transposeEnabled = playbackTransposeEnabled.load(std::memory_order_relaxed);
    snapshot.transposeOffset = playbackTransposeOffset.load(std::memory_order_relaxed);
    snapshot.channelFollowKeyMask = playbackChannelFollowKeyMask.load(std::memory_order_relaxed);
    return snapshot;
}

void AudioEngine::applyPendingParametersIfNeeded() {
    const auto mask = pendingParameterMask.exchange(0, std::memory_order_acq_rel);
    if (mask == 0) {
        return;
    }
    const auto pending = captureAcousticSnapshot();
    auto snapshot = activeAcoustic;
    if ((mask & gainParameter) != 0) {
        snapshot.masterGain = pending.masterGain;
    }
    if ((mask & adsrParameter) != 0) {
        snapshot.adsr = pending.adsr;
    }
    if ((mask & pianoParameter) != 0) {
        snapshot.brightness = pending.brightness;
        snapshot.hammerHardness = pending.hammerHardness;
        snapshot.resonance = pending.resonance;
    }
    if ((mask & toneParameter) != 0) {
        snapshot.builtinTone = pending.builtinTone;
    }
    if ((mask & lidParameter) != 0) {
        snapshot.lidPosition = pending.lidPosition;
    }
    if ((mask & temperamentParameter) != 0) {
        snapshot.temperament = pending.temperament;
    }
    if ((mask & pitchParameter) != 0) {
        snapshot.referencePitchA4 = pending.referencePitchA4;
    }
    if ((mask & perspectiveParameter) != 0) {
        snapshot.soundPerspective = pending.soundPerspective;
    }
    if ((mask & spaceParameter) != 0) {
        snapshot.reverbSpace = pending.reverbSpace;
    }
    if ((mask & wetParameter) != 0) {
        snapshot.reverbWet = pending.reverbWet;
    }
    if ((mask & noiseParameter) != 0) {
        snapshot.pedalNoiseLevel = pending.pedalNoiseLevel;
    }
    if ((mask & feltParameter) != 0) {
        snapshot.feltAgeingAmount = pending.feltAgeingAmount;
    }
    if ((mask & transposeParameter) != 0) {
        snapshot.transposeEnabled = pending.transposeEnabled;
        snapshot.transposeOffset = pending.transposeOffset;
        snapshot.channelFollowKeyMask = pending.channelFollowKeyMask;
    }
    applyAcousticSnapshot(snapshot);
}

void AudioEngine::applyAcousticSnapshot(const devpiano::audio::AcousticSnapshot& snapshot, bool recordedPreset) {
    devpiano::recording::applyAcousticSnapshotToBuiltin(synth, sineSynth, activeSynth, roomReverb, activeMasterGain,
                                                        snapshot, recordedPreset);
    activeAcoustic = snapshot;
    if (recordedPreset) {
        syncPedalProcessor.setPolicy(snapshot.sustainPolicy);
        if (devpiano::audio::resolveInstrumentEndpoint(pluginHost).isHostedPluginReady()) {
            for (int channel = 1; channel <= 16; ++channel) {
                devpiano::audio::appendOrderedMidi(
                    segmentMidiBuffer, juce::MidiMessage::controllerEvent(channel, 67, snapshot.unaCorda ? 127 : 0), 0);
            }
        }
    }
}

void AudioEngine::discardWarmupInputState() {
    clearDisplayNotes();
    liveMidiQueue.discardPublished();
    midiBuffer.clear();
    playbackVisualMidiBuffer.clear();
    liveOverflowPending.store(false, std::memory_order_relaxed);
    synth.allNotesOff(0, false);
    sineSynth.allNotesOff(0, false);
    roomReverb.reset();
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

    playbackIdentityTracker.resetOwnership();
    syncPedalProcessor.reset();
    clearDisplayNotes();
    for (auto channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), 0); // sustain pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 67, 0), 0); // soft pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0); // all sound off
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
    }
    synth.allNotesOff(0, false);
    sineSynth.allNotesOff(0, false);
    roomReverb.reset();
}

void AudioEngine::recordRealtimeMidiBufferIfNeeded(int numSamples) {
    if (recordingEngine == nullptr) {
        return;
    }

    int firstSample = 0;
    const auto state = recordingEngine->getState();
    if (state == devpiano::recording::RecordingState::countingIn) {
        const auto offset = metronomeProcessor.consumeCountInStartOffset(numSamples);
        if (offset < 0) {
            return;
        }
        recordingEngine->startArmedRecording();
        firstSample = offset;
    } else if (state != devpiano::recording::RecordingState::recording) {
        return;
    }

    const auto blockStartSamples = recordingEngine->getCurrentPositionSamples();
    recordingEngine->recordMidiBufferBlock(midiBuffer, devpiano::recording::RecordingEventSource::realtimeMidiBuffer,
                                           blockStartSamples, firstSample);
    const auto samplesToAdvance = numSamples - firstSample;
    if (samplesToAdvance > 0) {
        recordingEngine->advanceRecordingPosition(samplesToAdvance);
    }
}

void AudioEngine::renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples) {
    if (recordingEngine == nullptr || !recordingEngine->needsPlaybackRender()) {
        return;
    }
    const auto currentGen = recordingEngine->getPlaybackGeneration();
    if (currentGen != playbackIdentityTracker.currentGeneration) {
        playbackIdentityTracker.currentGeneration = currentGen;
        playbackIdentityTracker.resetOwnership();
    }

    playbackVisualMidiBuffer.clear();
    recordingEngine->renderPlaybackBlock(playbackVisualMidiBuffer, blockStartSamples, numSamples);
    playbackTransposedMidiBuffer.clear();
    auto transposeEnabled = activeAcoustic.transposeEnabled;
    auto transposeOffset = activeAcoustic.transposeOffset;
    auto followMask = activeAcoustic.channelFollowKeyMask;
    const auto& scheduled = recordingEngine->getScheduledPresetChanges();
    std::size_t presetIndex = 0;
    std::size_t inputIndex = 0;
    std::size_t outputIndex = 0;
    std::size_t livePrefixCount = 0;
    auto liveCursor = midiBuffer.begin();
    const auto liveEnd = midiBuffer.end();
    const auto consumePresets = [&] {
        while (presetIndex < scheduled.size() && scheduled[presetIndex].midiEventCount <= inputIndex) {
            const auto& change = scheduled[presetIndex++];
            while (liveCursor != liveEnd && (*liveCursor).samplePosition <= change.sampleOffset) {
                ++liveCursor;
                ++livePrefixCount;
            }
            if (const auto* preset = recordingEngine->getPlaybackPreset(change.presetId)) {
                transposeEnabled = preset->acoustic.transposeEnabled;
                transposeOffset = preset->acoustic.transposeOffset;
                followMask = preset->acoustic.channelFollowKeyMask;
                if (presetBoundaries.size() < presetBoundaries.capacity()) {
                    presetBoundaries.push_back(
                        { &preset->acoustic, change.sampleOffset, outputIndex + livePrefixCount });
                } else {
                    realtimeOverflowCount.fetch_add(1, std::memory_order_relaxed);
                    allNotesOffPending.store(true, std::memory_order_release);
                }
            }
        }
    };

    for (const auto metadata : playbackVisualMidiBuffer) {
        consumePresets();
        ++inputIndex;
        if (metadata.numBytes > 3) {
            devpiano::audio::appendOrderedMidi(playbackTransposedMidiBuffer, metadata.data, metadata.numBytes,
                                               metadata.samplePosition);
            ++outputIndex;
            continue;
        }
        const auto msg = metadata.getMessage();
        const auto samplePos = metadata.samplePosition;
        const auto ch = msg.getChannel();
        const auto chIdx = juce::jlimit(0, 15, ch - 1);
        const bool channelFollows = (followMask & (1U << chIdx)) != 0;

        if (msg.isNoteOn()) {
            const auto sourceNote = msg.getNoteNumber();
            const auto candidatePitch = (transposeEnabled && channelFollows)
                ? juce::jlimit(0, 127, sourceNote + transposeOffset)
                : sourceNote;
            const auto finalOutputPitch = playbackIdentityTracker.noteOn(ch, sourceNote, candidatePitch);
            if (finalOutputPitch.has_value()) {
                devpiano::audio::appendOrderedMidi(
                    playbackTransposedMidiBuffer,
                    juce::MidiMessage::noteOn(ch, static_cast<int>(*finalOutputPitch), msg.getFloatVelocity()),
                    samplePos);
                ++outputIndex;
            } else {
                realtimeOverflowCount.fetch_add(1, std::memory_order_relaxed);
                allNotesOffPending.store(true, std::memory_order_release);
            }
        } else if (msg.isNoteOff()) {
            const auto sourceNote = msg.getNoteNumber();
            const auto result = playbackIdentityTracker.noteOff(ch, sourceNote);
            if (result.matched && result.shouldEmit) {
                devpiano::audio::appendOrderedMidi(
                    playbackTransposedMidiBuffer,
                    juce::MidiMessage::noteOff(ch, static_cast<int>(result.outputPitch), msg.getFloatVelocity()),
                    samplePos);
                ++outputIndex;
            }
        } else {
            if (msg.isController()) {
                const auto ctrl = msg.getControllerNumber();
                if (ctrl == 120 || ctrl == 123) {
                    playbackIdentityTracker.resetChannel(ch);
                }
            } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                playbackIdentityTracker.resetChannel(ch);
            }
            devpiano::audio::appendOrderedMidi(playbackTransposedMidiBuffer, msg, samplePos);
            ++outputIndex;
        }
    }
    consumePresets();

    playbackVisualMidiBuffer.swapWith(playbackTransposedMidiBuffer);
    if (midiBuffer.isEmpty()) {
        midiBuffer.swapWith(playbackVisualMidiBuffer);
    } else {
        syncPedalTempBuffer.clear();
        auto live = midiBuffer.begin();
        auto played = playbackVisualMidiBuffer.begin();
        while (live != midiBuffer.end() || played != playbackVisualMidiBuffer.end()) {
            const bool useLive = played == playbackVisualMidiBuffer.end()
                || (live != midiBuffer.end() && (*live).samplePosition <= (*played).samplePosition);
            const auto metadata = useLive ? *live++ : *played++;
            devpiano::audio::appendOrderedMidi(syncPedalTempBuffer, metadata.data, metadata.numBytes,
                                               metadata.samplePosition);
        }
        midiBuffer.swapWith(syncPedalTempBuffer);
    }
    recordingEngine->advancePlaybackPosition(numSamples);
}

void AudioEngine::handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (!dispatchingDisplay) {
        enqueueLiveMidi(juce::MidiMessage::noteOn(channel, note, velocity));
    }
}

void AudioEngine::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (!dispatchingDisplay) {
        enqueueLiveMidi(juce::MidiMessage::noteOff(channel, note, velocity));
    }
}

void AudioEngine::enqueueLiveMidi(const juce::MidiMessage& message) noexcept {
    LiveMidiEvent event;
    event.size = static_cast<std::uint8_t>(message.getRawDataSize());
    event.timestampMilliseconds = juce::Time::getMillisecondCounter();
    std::copy_n(message.getRawData(), event.size, event.bytes.begin());
    if (!liveMidiQueue.push(event)) {
        realtimeOverflowCount.fetch_add(1, std::memory_order_relaxed);
        liveOverflowPending.store(true, std::memory_order_release);
    }
}

void AudioEngine::collectLiveMidi(int numSamples) noexcept {
    if (liveOverflowPending.exchange(false, std::memory_order_acq_rel)) {
        liveMidiQueue.discardPublished();
        allNotesOffPending.store(true, std::memory_order_release);
        return;
    }
    LiveMidiEvent first;
    LiveMidiEvent last;
    LiveMidiEvent event;
    const auto count = liveMidiQueue.snapshot(first, last);
    const auto span = last.timestampMilliseconds - first.timestampMilliseconds;
    const auto scale = static_cast<double>(numSamples) / (static_cast<double>(span) + 1.0);
    for (std::size_t index = 0; index < count && liveMidiQueue.pop(event); ++index) {
        const auto elapsed = event.timestampMilliseconds - first.timestampMilliseconds;
        const auto offset = juce::jlimit(0, numSamples - 1, static_cast<int>(std::round(elapsed * scale)));
        midiBuffer.addEvent(event.bytes.data(), event.size, offset);
    }
}

void AudioEngine::clearDisplayNotes() noexcept {
    for (auto& channel : displayNotes) {
        for (auto& note : channel) {
            note.store(++displaySequence << 8, std::memory_order_release);
        }
    }
}

void AudioEngine::publishMidiForDisplay(const juce::MidiBuffer& buffer) noexcept {
    for (const auto metadata : buffer) {
        if (metadata.numBytes != 3 || metadata.data[0] < 0x80 || metadata.data[0] >= 0xf0) {
            continue;
        }
        const auto type = metadata.data[0] & 0xf0;
        auto& channel = displayNotes[metadata.data[0] & 0x0f];
        if (type == 0x90 || type == 0x80) {
            auto& note = channel[metadata.data[1] & 0x7f];
            const bool on = type == 0x90 && metadata.data[2] > 0;
            const auto velocity = on ? metadata.data[2] & 0x7f : note.load(std::memory_order_relaxed) & 0x7f;
            note.store((++displaySequence << 8) | velocity | (on ? 0x80U : 0U), std::memory_order_release);
        } else if (type == 0xb0 && (metadata.data[1] == 120 || metadata.data[1] == 123)) {
            for (auto& note : channel) {
                note.store((++displaySequence << 8) | (note.load(std::memory_order_relaxed) & 0x7f),
                           std::memory_order_release);
            }
        }
    }
}

void AudioEngine::dispatchPendingDisplayEvents() {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    dispatchingDisplay = true;
    for (std::size_t channel = 0; channel < displayNotes.size(); ++channel) {
        for (std::size_t pitch = 0; pitch < displayNotes[channel].size(); ++pitch) {
            const auto value = displayNotes[channel][pitch].load(std::memory_order_acquire);
            if (value == observedDisplayNotes[channel][pitch]) {
                continue;
            }
            observedDisplayNotes[channel][pitch] = value;
            const auto velocity = static_cast<float>(value & 0x7f) / 127.0f;
            if (velocity > 0.0f) {
                keyboardState.noteOn(static_cast<int>(channel) + 1, static_cast<int>(pitch), velocity);
            }
            if ((value & 0x80) == 0) {
                keyboardState.noteOff(static_cast<int>(channel) + 1, static_cast<int>(pitch), 0.0f);
            }
        }
    }
    uiDiscardMidi.clear();
    keyboardState.processNextMidiBuffer(uiDiscardMidi, 0, 1, false);
    dispatchingDisplay = false;
}

std::size_t AudioEngine::consumeRealtimeOverflowCount() noexcept {
    return realtimeOverflowCount.exchange(0, std::memory_order_acq_rel);
}
