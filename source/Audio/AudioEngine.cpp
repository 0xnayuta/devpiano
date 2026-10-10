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

    pianoBuffer.setSize(2, juce::jmax(1, samplesPerBlockExpected), false, false, true);
    pianoBuffer.clear();
    pianoDelayedBuffer.setSize(2, juce::jmax(1, samplesPerBlockExpected), false, false, true);
    pianoDelayedBuffer.clear();
    pianoView.setDataToReferTo(pianoBuffer.getArrayOfWritePointers(), 2, juce::jmax(1, samplesPerBlockExpected));
    pianoDelayedView.setDataToReferTo(pianoDelayedBuffer.getArrayOfWritePointers(), 2,
                                      juce::jmax(1, samplesPerBlockExpected));

    const int reportedLatency = endpoint.isHostedPluginReady() ? endpoint.hostedInstance->getLatencySamples() : 0;
    const int safeLatency = devpiano::audio::sanitizePluginLatency(reportedLatency);
    const int delayCapacity = devpiano::audio::calculateDelayCapacity(reportedLatency);
    pianoDelayLine.prepare(delayCapacity, 2);
    pianoDelayLine.setDelay(safeLatency);
    latencyFaultPending.store(false, std::memory_order_release);

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
    playbackOwnedMidiBuffer.ensureSize(preparedMidiCapacity);
    pluginSegmentMidiBuffer.ensureSize(preparedMidiCapacity);
    syncPedalTempBuffer.ensureSize(preparedMidiCapacity);
    segmentMidiBuffer.ensureSize(preparedMidiCapacity);
    boundaryMidiBuffer.ensureSize(preparedMidiCapacity);
    builtinSegmentMidiBuffer.ensureSize(preparedMidiCapacity);
    midiEventSources.reserve(preparedMidiCapacity);
    segmentEventSources.reserve(preparedMidiCapacity);
    boundaryEventSources.reserve(preparedMidiCapacity);
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
    playbackOwnedMidiBuffer.clear();
    presetBoundaries.clear();
    const auto commands = recordingEngine != nullptr ? recordingEngine->applyPendingTransportCommands(midiBuffer)
                                                     : devpiano::recording::RecordingEngine::TransportCommandResult {};
    if (commands.seekApplied || commands.stopApplied) {
        playbackIdentityTracker.resetOwnership();
        resetPerformanceInputOwnership();
        syncPedalProcessor.reset();
        clearDisplayNotes();
        synth.handleSostenutoPedal(0, false);
        synth.allNotesOff(0, false);
        sineSynth.allNotesOff(0, false);
        pianoDelayLine.reset();
        builtinNoteState.reset();
        pluginNoteState.reset();
        previousBuiltinSynth = nullptr;
        previousPluginEnabled = false;
        roomReverb.reset();
    }
    collectLiveMidi(bufferToFill.numSamples);
    syncCutInBlock = syncPedalProcessor.getPolicy() == devpiano::core::SustainPolicy::syncPedal
        && (syncPedalProcessor.isPedalDown() || syncPedalProcessor.isCutPending());
    syncPedalProcessor.processMidiBlock(midiBuffer, syncPedalTempBuffer);
    injectPendingAllNotesOffIfNeeded();
    recordRealtimeMidiBufferIfNeeded(bufferToFill.numSamples);
    const auto preRollPending = playbackStartPreRollBlocksRemaining.load(std::memory_order_acquire) > 0;
    if (!(muteInstrumentDuringBlock && preRollPending) && !consumePlaybackStartPreRollBlockIfNeeded()) {
        renderPlaybackEventsIfNeeded(recordingEngine != nullptr ? recordingEngine->getPlaybackPositionSamples() : 0,
                                     bufferToFill.numSamples);
    }
    mergePerformanceInput();
    segmentMidiBuffer.clear();
    segmentEventSources.clear();
    auto cursor = midiBuffer.begin();
    const auto end = midiBuffer.end();
    std::size_t eventIndex = 0;
    int segmentStart = 0;
    for (const auto& boundary : presetBoundaries) {
        boundaryMidiBuffer.clear();
        boundaryEventSources.clear();
        while (cursor != end && eventIndex < boundary.midiEventCount) {
            const auto metadata = *cursor;
            if (metadata.samplePosition < boundary.sampleOffset) {
                devpiano::audio::appendOrderedMidi(segmentMidiBuffer, metadata.data, metadata.numBytes,
                                                   metadata.samplePosition - segmentStart);
                segmentEventSources.push_back(midiEventSources[eventIndex]);
            } else {
                devpiano::audio::appendOrderedMidi(boundaryMidiBuffer, metadata.data, metadata.numBytes, 0);
                boundaryEventSources.push_back(midiEventSources[eventIndex]);
            }
            ++cursor;
            ++eventIndex;
        }
        renderInstrumentSegment(bufferToFill, segmentStart, boundary.sampleOffset - segmentStart);
        segmentStart = boundary.sampleOffset;
        std::size_t boundaryEvent = 0;
        for (const auto metadata : boundaryMidiBuffer) {
            devpiano::audio::appendOrderedMidi(segmentMidiBuffer, metadata.data, metadata.numBytes, 0);
            segmentEventSources.push_back(boundaryEventSources[boundaryEvent++]);
        }
        renderInstrumentSegment(bufferToFill, segmentStart, 0);
        applyAcousticSnapshot(*boundary.acoustic, true);
    }
    for (; cursor != end; ++cursor) {
        const auto metadata = *cursor;
        devpiano::audio::appendOrderedMidi(segmentMidiBuffer, metadata.data, metadata.numBytes,
                                           metadata.samplePosition - segmentStart);
        segmentEventSources.push_back(midiEventSources[eventIndex++]);
    }
    renderInstrumentSegment(bufferToFill, segmentStart, bufferToFill.numSamples - segmentStart);
}

void AudioEngine::renderInstrumentSegment(const juce::AudioSourceChannelInfo& output, int offset, int numSamples) {
    const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(pluginHost);
    const auto dual = activeLayers.enabled;
    devpiano::audio::BuiltinSynthesiser* targetBuiltin = nullptr;
    if (dual) {
        if (activeLayers.pianoEnabled) {
            targetBuiltin = &synth;
        }
    } else if (!endpoint.isHostedPluginReady()) {
        targetBuiltin = activeSynth;
    }
    const auto targetPlugin = endpoint.isHostedPluginReady() && (!dual || activeLayers.pluginEnabled);
    const auto modeChanged = previousLayersEnabled != dual;
    const auto instanceChanged = previousPluginInstance != endpoint.hostedInstance;
    const auto builtinChanged = previousBuiltinSynth != targetBuiltin;
    const auto pluginChanged = previousPluginEnabled != targetPlugin || instanceChanged;
    const auto start = output.startSample + offset;
    pluginSegmentMidiBuffer.clear();
    builtinSegmentMidiBuffer.clear();

    if (modeChanged || builtinChanged) {
        synth.allNotesOff(0, false);
        sineSynth.allNotesOff(0, false);
        builtinNoteState.reset();
        pianoDelayLine.reset();
    }
    if (modeChanged || pluginChanged) {
        pluginNoteState.reset();
        if (!instanceChanged && previousPluginEnabled && endpoint.isHostedPluginReady()) {
            devpiano::recording::addPanicMidi(pluginSegmentMidiBuffer, 0);
            pluginView.setDataToReferTo(pluginBuffer.getArrayOfWritePointers(), preparedPluginChannels, 0);
            endpoint.hostedInstance->processBlock(pluginView, pluginSegmentMidiBuffer);
            pluginSegmentMidiBuffer.clear();
        }
    }
    const auto restoreControllers = [&](juce::MidiBuffer& buffer) {
        for (std::size_t channel = 0; channel < 16; ++channel) {
            for (std::size_t pedal = 0; pedal < 3; ++pedal) {
                devpiano::audio::appendOrderedMidi(
                    buffer,
                    juce::MidiMessage::controllerEvent(static_cast<int>(channel) + 1,
                                                       pedal == 0 ? 64 : static_cast<int>(pedal) + 65,
                                                       performancePedals[channel][pedal]),
                    0);
            }
        }
    };
    if (targetBuiltin != nullptr && (modeChanged || builtinChanged)) {
        restoreControllers(builtinSegmentMidiBuffer);
    }
    if (targetPlugin && (modeChanged || pluginChanged)) {
        restoreControllers(pluginSegmentMidiBuffer);
    }
    previousLayersEnabled = dual;
    previousBuiltinSynth = targetBuiltin;
    previousPluginEnabled = targetPlugin;
    previousPluginInstance = endpoint.hostedInstance;

    std::size_t eventIndex = 0;
    for (const auto metadata : segmentMidiBuffer) {
        const auto source = static_cast<std::size_t>(segmentEventSources[eventIndex++]);
        if (targetBuiltin != nullptr) {
            builtinNoteState.append(builtinSegmentMidiBuffer, metadata, source);
        }
        if (targetPlugin) {
            pluginNoteState.append(pluginSegmentMidiBuffer, metadata, source);
        }
        if (metadata.numBytes == 3 && (metadata.data[0] & 0xf0) == 0xb0) {
            auto& pedals = performancePedals[metadata.data[0] & 0x0f];
            const auto controller = metadata.data[1];
            if (controller == 64 || controller == 66 || controller == 67) {
                pedals[controller == 64 ? 0 : controller - 65] = metadata.data[2];
            } else if (controller == 121) {
                pedals.fill(0);
            }
        }
    }
    const auto latency
        = dual && targetBuiltin != nullptr && targetPlugin ? endpoint.hostedInstance->getLatencySamples() : 0;
    if (latency < 0 || pianoDelayLine.isOverCapacity(latency)) {
        latencyOverflowCount.fetch_add(1, std::memory_order_relaxed);
        latencyFaultPending.store(true, std::memory_order_release);
        allNotesOffPending.store(true, std::memory_order_release);
        output.buffer->clear(start, numSamples);
        segmentMidiBuffer.clear();
        segmentEventSources.clear();
        return;
    }
    pianoDelayLine.setDelay(latency);
    pianoView.setDataToReferTo(pianoBuffer.getArrayOfWritePointers(), 2, numSamples);
    pianoDelayedView.setDataToReferTo(pianoDelayedBuffer.getArrayOfWritePointers(), 2, numSamples);
    pianoView.clear();
    pianoDelayedView.clear();
    if (targetBuiltin != nullptr) {
        targetBuiltin->renderNextBlock(pianoView, builtinSegmentMidiBuffer, 0, numSamples);
        pianoView.applyGain(0, numSamples, dual ? activeLayers.pianoGain : 1.0f);
        pianoDelayLine.process(pianoView, pianoDelayedView, numSamples);
    }
    pluginView.setDataToReferTo(pluginBuffer.getArrayOfWritePointers(), preparedPluginChannels, numSamples);
    pluginView.clear();
    if (targetPlugin) {
        endpoint.hostedInstance->processBlock(pluginView, pluginSegmentMidiBuffer);
        pluginView.applyGain(0, numSamples, dual ? activeLayers.pluginGain : 1.0f);
    }
    segmentMidiBuffer.clear();
    segmentEventSources.clear();
    if (numSamples == 0) {
        return;
    }
    output.buffer->clear(start, numSamples);
    const auto outChannels = output.buffer->getNumChannels();
    if (targetBuiltin != nullptr) {
        if (outChannels == 1) {
            output.buffer->addFrom(0, start, pianoDelayedView, 0, 0, numSamples, 0.5f);
            output.buffer->addFrom(0, start, pianoDelayedView, 1, 0, numSamples, 0.5f);
        } else {
            for (int channel = 0; channel < std::min(outChannels, 2); ++channel) {
                output.buffer->addFrom(channel, start, pianoDelayedView, channel, 0, numSamples);
            }
        }
    }
    if (targetPlugin) {
        const auto pluginChannels = endpoint.hostedInstance->getTotalNumOutputChannels();
        if (outChannels >= 2 && pluginChannels == 1) {
            output.buffer->addFrom(0, start, pluginView, 0, 0, numSamples);
            output.buffer->addFrom(1, start, pluginView, 0, 0, numSamples);
        } else if (outChannels == 1 && pluginChannels >= 2) {
            output.buffer->addFrom(0, start, pluginView, 0, 0, numSamples, 0.5f);
            output.buffer->addFrom(0, start, pluginView, 1, 0, numSamples, 0.5f);
        } else {
            for (int channel = 0; channel < std::min(outChannels, pluginChannels); ++channel) {
                output.buffer->addFrom(channel, start, pluginView, channel, 0, numSamples);
            }
        }
    }
    if (outChannels >= 2) {
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
    synth.handleSostenutoPedal(0, false);
    resetPerformanceInputOwnership();
    synth.allNotesOff(0, false);
    sineSynth.allNotesOff(0, false);
    pianoDelayLine.reset();
    builtinNoteState.reset();
    pluginNoteState.reset();
    previousBuiltinSynth = nullptr;
    previousPluginEnabled = false;
    previousPluginInstance = nullptr;
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

void AudioEngine::setPianoTuning(bool stretchTuningEnabled, float duplexResonance) {
    pendingStretchTuningEnabled.store(stretchTuningEnabled, std::memory_order_relaxed);
    pendingDuplexResonance.store(juce::jlimit(0.0f, 1.0f, duplexResonance), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(tuningParameter, std::memory_order_release);
}
void AudioEngine::setInputTranspose(bool enabled, int semitoneOffset, std::uint16_t channelFollowKeyMask) noexcept {
    inputTransposeEnabled.store(enabled, std::memory_order_release);
    inputTransposeOffset.store(semitoneOffset, std::memory_order_release);
    inputChannelFollowKeyMask.store(channelFollowKeyMask, std::memory_order_release);
    pendingParameterMask.fetch_or(transposeParameter, std::memory_order_release);
}

bool AudioEngine::isInputTransposeEnabled() const noexcept {
    return inputTransposeEnabled.load(std::memory_order_acquire);
}

int AudioEngine::getInputTransposeOffset() const noexcept {
    return inputTransposeOffset.load(std::memory_order_acquire);
}

std::uint16_t AudioEngine::getInputChannelFollowKeyMask() const noexcept {
    return inputChannelFollowKeyMask.load(std::memory_order_acquire);
}

void AudioEngine::setInstrumentLayers(const devpiano::audio::InstrumentLayers& layers) noexcept {
    pendingLayersEnabled.store(layers.enabled, std::memory_order_relaxed);
    pendingPianoEnabled.store(layers.pianoEnabled, std::memory_order_relaxed);
    pendingPluginEnabled.store(layers.pluginEnabled, std::memory_order_relaxed);
    pendingPianoGain.store(juce::jlimit(0.0f, 1.0f, layers.pianoGain), std::memory_order_relaxed);
    pendingPluginGain.store(juce::jlimit(0.0f, 1.0f, layers.pluginGain), std::memory_order_relaxed);
    pendingParameterMask.fetch_or(layersParameter, std::memory_order_release);
}

devpiano::audio::InstrumentLayers AudioEngine::getInstrumentLayers() const noexcept {
    return {
        .enabled = pendingLayersEnabled.load(std::memory_order_relaxed),
        .pianoEnabled = pendingPianoEnabled.load(std::memory_order_relaxed),
        .pluginEnabled = pendingPluginEnabled.load(std::memory_order_relaxed),
        .pianoGain = pendingPianoGain.load(std::memory_order_relaxed),
        .pluginGain = pendingPluginGain.load(std::memory_order_relaxed),
    };
}

int AudioEngine::consumeLatencyOverflowCount() noexcept {
    return latencyOverflowCount.exchange(0, std::memory_order_acq_rel);
}

bool AudioEngine::consumeLatencyFaultPending() noexcept {
    return latencyFaultPending.exchange(false, std::memory_order_acq_rel);
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
    snapshot.transposeEnabled = inputTransposeEnabled.load(std::memory_order_relaxed);
    snapshot.transposeOffset = inputTransposeOffset.load(std::memory_order_relaxed);
    snapshot.channelFollowKeyMask = inputChannelFollowKeyMask.load(std::memory_order_relaxed);
    snapshot.stretchTuningEnabled = pendingStretchTuningEnabled.load(std::memory_order_relaxed);
    snapshot.duplexResonance = pendingDuplexResonance.load(std::memory_order_relaxed);
    snapshot.layers = getInstrumentLayers();
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
    if ((mask & tuningParameter) != 0) {
        snapshot.stretchTuningEnabled = pending.stretchTuningEnabled;
        snapshot.duplexResonance = pending.duplexResonance;
    }
    if ((mask & layersParameter) != 0) {
        snapshot.layers = pending.layers;
    }
    applyAcousticSnapshot(snapshot);
}

void AudioEngine::applyAcousticSnapshot(const devpiano::audio::AcousticSnapshot& snapshot, bool recordedPreset) {
    devpiano::recording::applyAcousticSnapshotToBuiltin(synth, sineSynth, activeSynth, roomReverb, activeMasterGain,
                                                        snapshot, false);
    activeAcoustic = snapshot;
    activeLayers = snapshot.layers;
    if (recordedPreset) {
        pendingLayersEnabled.store(snapshot.layers.enabled, std::memory_order_relaxed);
        pendingPianoEnabled.store(snapshot.layers.pianoEnabled, std::memory_order_relaxed);
        pendingPluginEnabled.store(snapshot.layers.pluginEnabled, std::memory_order_relaxed);
        pendingPianoGain.store(snapshot.layers.pianoGain, std::memory_order_relaxed);
        pendingPluginGain.store(snapshot.layers.pluginGain, std::memory_order_relaxed);

        syncPedalProcessor.setPolicy(snapshot.sustainPolicy);
    }
}

void AudioEngine::discardWarmupInputState() {
    clearDisplayNotes();
    liveMidiQueue.discardPublished();
    midiBuffer.clear();
    playbackVisualMidiBuffer.clear();
    liveOverflowPending.store(false, std::memory_order_relaxed);
    resetPerformanceInputOwnership();
    synth.handleSostenutoPedal(0, false);
    synth.allNotesOff(0, false);
    sineSynth.allNotesOff(0, false);
    pianoDelayLine.reset();
    builtinNoteState.reset();
    pluginNoteState.reset();
    previousBuiltinSynth = nullptr;
    previousPluginEnabled = false;
    performancePedals = {};
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
    resetPerformanceInputOwnership();
    syncPedalProcessor.reset();
    clearDisplayNotes();
    for (auto channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), 0); // sustain pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 66, 0), 0); // sostenuto pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 67, 0), 0); // soft pedal off
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0); // all sound off
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
    }
    synth.handleSostenutoPedal(0, false);
    synth.allNotesOff(0, false);
    sineSynth.allNotesOff(0, false);
    pianoDelayLine.reset();
    builtinNoteState.reset();
    pluginNoteState.reset();
    previousBuiltinSynth = nullptr;
    previousPluginEnabled = false;
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
    playbackOwnedMidiBuffer.clear();
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
            devpiano::audio::appendOrderedMidi(playbackOwnedMidiBuffer, metadata.data, metadata.numBytes,
                                               metadata.samplePosition);
            ++outputIndex;
            continue;
        }
        const auto msg = metadata.getMessage();
        const auto samplePos = metadata.samplePosition;
        const auto ch = msg.getChannel();

        if (msg.isNoteOn()) {
            const auto sourceNote = msg.getNoteNumber();
            const auto finalOutputPitch = playbackIdentityTracker.noteOn(ch, sourceNote, sourceNote);
            if (finalOutputPitch.has_value()) {
                devpiano::audio::appendOrderedMidi(
                    playbackOwnedMidiBuffer,
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
                    playbackOwnedMidiBuffer,
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
            devpiano::audio::appendOrderedMidi(playbackOwnedMidiBuffer, msg, samplePos);
            ++outputIndex;
        }
    }
    consumePresets();

    recordingEngine->advancePlaybackPosition(numSamples);
}

void AudioEngine::resetPerformanceInputOwnership() noexcept {
    inputNotes = {};
    inputPedals = {};
    hasRecordedSoftBaseline = false;
    recordedSoftBaseline = false;
}

void AudioEngine::mergePerformanceInput() {
    syncPedalTempBuffer.clear();
    midiEventSources.clear();
    std::uint8_t eventSource = 1;
    auto live = midiBuffer.begin();
    auto played = playbackOwnedMidiBuffer.begin();
    const auto liveEnd = midiBuffer.end();
    const auto playedEnd = playbackOwnedMidiBuffer.end();
    std::size_t rawCount = 0;
    std::size_t outputCount = 0;
    std::size_t boundaryIndex = 0;
    const auto emit = [&](const juce::MidiMessage& message, int offset) {
        devpiano::audio::appendOrderedMidi(syncPedalTempBuffer, message, offset);
        midiEventSources.push_back(eventSource);
        ++outputCount;
    };
    const auto publish = [&](std::size_t channel, std::size_t pitch, unsigned int velocity) {
        auto& display = displayNotes[channel][pitch];
        const auto held = inputNotes[0][channel][pitch] || inputNotes[1][channel][pitch];
        const auto level = velocity > 0 ? velocity : (display.load(std::memory_order_relaxed) & 0x7f);
        display.store((++displaySequence << 8) | level | (held ? 0x80U : 0U), std::memory_order_release);
    };
    const auto captureBoundaries = [&] {
        while (boundaryIndex < presetBoundaries.size() && presetBoundaries[boundaryIndex].midiEventCount == rawCount) {
            auto& boundary = presetBoundaries[boundaryIndex++];
            eventSource = 1;
            boundary.midiEventCount = outputCount;
            const auto soft = boundary.acoustic->unaCorda;
            if (!hasRecordedSoftBaseline || recordedSoftBaseline != soft) {
                for (std::size_t channel = 0; channel < 16; ++channel) {
                    inputPedals[1][channel][2] = soft ? 127 : 0;
                    emit(juce::MidiMessage::controllerEvent(
                             static_cast<int>(channel) + 1, 67,
                             std::max(inputPedals[0][channel][2], inputPedals[1][channel][2])),
                         boundary.sampleOffset);
                }
                hasRecordedSoftBaseline = true;
                recordedSoftBaseline = soft;
            }
        }
    };
    while (live != liveEnd || played != playedEnd) {
        captureBoundaries();
        const auto useLive
            = played == playedEnd || (live != liveEnd && (*live).samplePosition <= (*played).samplePosition);
        const auto metadata = useLive ? *live++ : *played++;
        eventSource = useLive ? 0 : 1;
        ++rawCount;
        if (metadata.numBytes > 3) {
            devpiano::audio::appendOrderedMidi(syncPedalTempBuffer, metadata.data, metadata.numBytes,
                                               metadata.samplePosition);
            midiEventSources.push_back(eventSource);
            ++outputCount;
            continue;
        }
        const auto message = metadata.getMessage();
        const auto channel = message.getChannel() - 1;
        if (channel < 0 || channel >= 16) {
            emit(message, metadata.samplePosition);
            continue;
        }
        const auto source = useLive ? std::size_t { 0 } : std::size_t { 1 };
        const auto other = std::size_t { 1 } - source;
        const auto ch = static_cast<std::size_t>(channel);
        if (message.isNoteOn()) {
            inputNotes[source][ch][static_cast<std::size_t>(message.getNoteNumber())] = true;
            emit(message, metadata.samplePosition);
            publish(ch, static_cast<std::size_t>(message.getNoteNumber()), message.getVelocity());
        } else if (message.isNoteOff()) {
            const auto note = static_cast<std::size_t>(message.getNoteNumber());
            inputNotes[source][ch][note] = false;
            emit(message, metadata.samplePosition);
            publish(ch, note, 0);
        } else if (message.isController()
                   && (message.getControllerNumber() == 64 || message.getControllerNumber() == 66
                       || message.getControllerNumber() == 67)) {
            const auto controller = message.getControllerNumber();
            const auto pedal = static_cast<std::size_t>(controller == 64 ? 0 : controller - 65);
            inputPedals[source][ch][pedal] = static_cast<std::uint8_t>(message.getControllerValue());
            const auto forcedDamp = useLive && controller == 64 && message.getControllerValue() == 0 && syncCutInBlock;
            const auto value = forcedDamp ? 0 : std::max(inputPedals[source][ch][pedal], inputPedals[other][ch][pedal]);
            emit(juce::MidiMessage::controllerEvent(channel + 1, controller, value), metadata.samplePosition);
        } else if (message.isAllSoundOff() || message.isAllNotesOff()) {
            inputNotes[source][ch].fill(false);
            inputPedals[source][ch].fill(0);
            emit(message, metadata.samplePosition);
            for (std::size_t note = 0; note < 128; ++note) {
                publish(ch, note, 0);
            }
        } else {
            emit(message, metadata.samplePosition);
            if (message.isControllerOfType(121)) {
                inputPedals[source][ch].fill(0);
                for (std::size_t pedal = 0; pedal < 3; ++pedal) {
                    const auto value = inputPedals[other][ch][pedal];
                    if (value > 0) {
                        emit(juce::MidiMessage::controllerEvent(channel + 1,
                                                                pedal == 0 ? 64 : static_cast<int>(pedal) + 65, value),
                             metadata.samplePosition);
                    }
                }
            }
        }
    }
    captureBoundaries();
    midiBuffer.swapWith(syncPedalTempBuffer);
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
            if ((value & 0x80) != 0) {
                const auto velocity = static_cast<float>(value & 0x7f) / 127.0f;
                keyboardState.noteOn(static_cast<int>(channel) + 1, static_cast<int>(pitch), velocity);
            } else {
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
