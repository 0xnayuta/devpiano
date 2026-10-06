#include <functional>

#include "Audio/RoomReverbEngine.h"
#include "Diagnostics/Log.h"
#include "Export/ExportFlowSupport.h"
#include "Plugin/PluginHost.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace devpiano::exporting {
namespace {

using devpiano::recording::addPanicMidi;
using devpiano::recording::applyAcousticSnapshotToReverbAndGain;
using devpiano::recording::hasUsableRenderOptions;
using devpiano::recording::PerformanceEventType;
using devpiano::recording::prepareRenderTimeline;
} // namespace

// ---------------------------------------------------------------------------
// snapshotPluginState
// ---------------------------------------------------------------------------
juce::MemoryBlock snapshotPluginState(juce::AudioPluginInstance& plugin) {
    juce::MemoryBlock state;
    plugin.getStateInformation(state);
    return state;
}

// ---------------------------------------------------------------------------
// createOfflinePluginInstance
// ---------------------------------------------------------------------------
std::unique_ptr<juce::AudioPluginInstance> createOfflinePluginInstance(juce::AudioPluginFormatManager& formatManager,
                                                                       const juce::PluginDescription& description,
                                                                       double sampleRate, int blockSize,
                                                                       juce::String& errorMessage) {
    auto instance = formatManager.createPluginInstance(description, sampleRate, blockSize, errorMessage);
    if (instance == nullptr) {
        DP_LOG_ERROR("[PluginOfflineRenderer] createPluginInstance failed: " + errorMessage);
        return nullptr;
    }

    if (!PluginHost::configureDefaultBuses(*instance)) {
        errorMessage = "Failed to configure offline plugin buses.";
        DP_LOG_ERROR("[PluginOfflineRenderer] " + errorMessage);
        return nullptr;
    }

    instance->setNonRealtime(true);
    instance->setRateAndBufferSizeDetails(sampleRate, blockSize);
    instance->prepareToPlay(sampleRate, blockSize);
    // NOLINTNEXTLINE(readability-ambiguous-smartptr-reset-call) - 意图是 AudioPluginInstance::reset()（实例方法）
    instance->reset();

    DP_LOG_INFO("[PluginOfflineRenderer] Offline instance created and prepared: " + description.name + " @ "
                + juce::String(sampleRate) + "Hz, block=" + juce::String(blockSize));
    return instance;
}

// ---------------------------------------------------------------------------
// renderTakeWithOfflinePlugin
// ---------------------------------------------------------------------------
bool renderTakeWithOfflinePlugin(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                                 const WavExportOptions& options, juce::AudioPluginInstance& offlinePlugin,
                                 const std::function<bool(double)>& progressCallback) {
    if (take.isEmpty() || take.sampleRate <= 0.0 || !hasUsableRenderOptions(options)
        || destinationFile == juce::File()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Invalid parameters for offline render");
        return false;
    }

    auto timeline = prepareRenderTimeline(take, options.sampleRate, 2.0);
    if (!timeline.has_value()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Invalid or unrepresentable timeline");
        return false;
    }
    const auto& renderEvents = timeline->events;
    const auto scaledTakeLength = timeline->takeLengthSamples;
    const auto totalSamples = timeline->totalSamples;

    // Create output directory if needed
    auto parentDirectory = destinationFile.getParentDirectory();
    if (!parentDirectory.exists() && !parentDirectory.createDirectory()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Cannot create output directory: " + parentDirectory.getFullPathName());
        return false;
    }

    juce::TemporaryFile temporaryFile(destinationFile);
    auto fileStream = std::make_unique<juce::FileOutputStream>(temporaryFile.getFile());
    if (!fileStream->openedOk()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Cannot open output file: " + destinationFile.getFullPathName());
        return false;
    }
    auto* const fileOutput = fileStream.get();
    std::unique_ptr<juce::OutputStream> outputStream = std::move(fileStream);

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate(options.sampleRate)
                             .withNumChannels(options.numChannels)
                             .withBitsPerSample(options.bitsPerSample);
    auto writer = wavFormat.createWriterFor(outputStream, writerOptions);
    if (writer == nullptr) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Failed to create WAV writer");
        return false;
    }

    devpiano::audio::PlaybackIdentityTracker identityTracker;
    identityTracker.prepare(0, static_cast<std::size_t>(std::ranges::count_if(take.events, [](const auto& event) {
                                return event.type == PerformanceEventType::midi && event.message.isNoteOn();
                            })));

    const auto initialGain = juce::jlimit(0.0f, 1.0f, options.masterGain);
    float currentMasterGain = initialGain;

    // Determine channel count from the plugin instance
    const auto requiredPluginChannels = juce::jmax(
        1, juce::jmax(offlinePlugin.getTotalNumInputChannels(), offlinePlugin.getTotalNumOutputChannels()));
    const auto outputChannels = juce::jmin(options.numChannels, offlinePlugin.getTotalNumOutputChannels());

    juce::AudioBuffer<float> pluginBuffer(requiredPluginChannels, options.blockSize);
    juce::MidiBuffer segmentMidiBuffer;
    segmentMidiBuffer.ensureSize(
        static_cast<size_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(options.blockSize) * 16, 256, 65536)));

    juce::AudioBuffer<float> outputBuffer(options.numChannels, options.blockSize);

    std::size_t eventIndex = 0;
    auto allNotesOffSent = false;
    devpiano::audio::RoomReverbEngine roomReverb;
    roomReverb.prepare(options.sampleRate);
    roomReverb.setSpace(options.reverbSpace);
    roomReverb.setWetLevel(options.reverbWet);

    DP_LOG_INFO("[PluginOfflineRenderer] Starting offline render: " + juce::String(renderEvents.size()) + " events, "
                + juce::String(totalSamples) + " total samples, " + juce::String(outputChannels) + " output channels");

    for (std::int64_t blockStart = 0; blockStart < totalSamples;) {
        if (progressCallback
            && !progressCallback(static_cast<double>(blockStart) / static_cast<double>(totalSamples))) {
            writer.reset();
            return false;
        }

        const auto numSamples = static_cast<int>(std::min<std::int64_t>(options.blockSize, totalSamples - blockStart));
        const auto blockEnd = blockStart + numSamples;

        outputBuffer.setSize(options.numChannels, numSamples, false, false, true);
        outputBuffer.clear();

        for (int segStart = 0; segStart < numSamples;) {
            const auto segAbsStart = blockStart + segStart;
            segmentMidiBuffer.clear();

            while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples == segAbsStart
                   && renderEvents[eventIndex].type == PerformanceEventType::presetChange) {
                const auto& ev = renderEvents[eventIndex];
                if (ev.presetId < timeline->presets.size()) {
                    const auto& preset = timeline->presets[ev.presetId];
                    applyAcousticSnapshotToReverbAndGain(roomReverb, currentMasterGain, preset.acoustic);
                    for (int ch = 1; ch <= 16; ++ch) {
                        segmentMidiBuffer.addEvent(
                            juce::MidiMessage::controllerEvent(ch, 67, preset.acoustic.unaCorda ? 127 : 0), 0);
                    }
                }
                ++eventIndex;
            }

            auto nextPresetOffset = numSamples;
            for (std::size_t scan = eventIndex; scan < renderEvents.size(); ++scan) {
                if (renderEvents[scan].timestampSamples >= blockEnd) {
                    break;
                }
                if (renderEvents[scan].type == PerformanceEventType::presetChange) {
                    nextPresetOffset = static_cast<int>(renderEvents[scan].timestampSamples - blockStart);
                    break;
                }
            }

            const auto segEnd = nextPresetOffset;
            const auto segLen = segEnd - segStart;
            const auto segAbsEnd = blockStart + segEnd;

            while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples < segAbsEnd) {
                const auto& event = renderEvents[eventIndex];
                if (event.type == PerformanceEventType::presetChange) {
                    break;
                }
                if (event.timestampSamples >= segAbsStart) {
                    const auto sampleOffset = static_cast<int>(event.timestampSamples - segAbsStart);
                    const auto clampedOffset = juce::jlimit(0, segLen - 1, sampleOffset);
                    const auto& msg = event.message;

                    if (msg.isNoteOn()) {
                        const auto ch = msg.getChannel();
                        const auto sourceNote = msg.getNoteNumber();
                        const auto finalOutputPitch = identityTracker.noteOn(ch, sourceNote, sourceNote);
                        if (finalOutputPitch.has_value()) {
                            segmentMidiBuffer.addEvent(juce::MidiMessage::noteOn(ch,
                                                                                 static_cast<int>(*finalOutputPitch),
                                                                                 msg.getFloatVelocity()),
                                                       clampedOffset);
                        }
                    } else if (msg.isNoteOff()) {
                        const auto ch = msg.getChannel();
                        const auto sourceNote = msg.getNoteNumber();
                        const auto result = identityTracker.noteOff(ch, sourceNote);
                        if (result.shouldEmit) {
                            segmentMidiBuffer.addEvent(
                                juce::MidiMessage::noteOff(ch, result.outputPitch, msg.getFloatVelocity()),
                                clampedOffset);
                        }
                    } else {
                        if (msg.isController()) {
                            const auto ctrl = msg.getControllerNumber();
                            if (ctrl == 120 || ctrl == 123) {
                                identityTracker.resetChannel(msg.getChannel());
                            }
                        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                            identityTracker.resetChannel(msg.getChannel());
                        }
                        segmentMidiBuffer.addEvent(msg, clampedOffset);
                    }
                }
                ++eventIndex;
            }

            if (!allNotesOffSent && scaledTakeLength >= segAbsStart && scaledTakeLength < segAbsEnd) {
                const auto offset = juce::jlimit(0, segLen - 1, static_cast<int>(scaledTakeLength - segAbsStart));
                addPanicMidi(segmentMidiBuffer, offset);
                identityTracker.resetOwnership();
                allNotesOffSent = true;
            }

            pluginBuffer.setSize(requiredPluginChannels, segLen, false, false, true);
            pluginBuffer.clear();

            offlinePlugin.processBlock(pluginBuffer, segmentMidiBuffer);

            juce::AudioBuffer<float> segmentOutputBuffer(outputBuffer.getArrayOfWritePointers(), options.numChannels,
                                                         segStart, segLen);
            segmentOutputBuffer.clear();

            const auto pluginOutputChannels = offlinePlugin.getTotalNumOutputChannels();
            if (options.numChannels == 2 && pluginOutputChannels == 1) {
                segmentOutputBuffer.copyFrom(0, 0, pluginBuffer, 0, 0, segLen);
                segmentOutputBuffer.copyFrom(1, 0, pluginBuffer, 0, 0, segLen);
            } else if (options.numChannels == 1 && pluginOutputChannels >= 2) {
                segmentOutputBuffer.copyFrom(0, 0, pluginBuffer, 0, 0, segLen);
                segmentOutputBuffer.addFrom(0, 0, pluginBuffer, 1, 0, segLen);
                segmentOutputBuffer.applyGain(0, 0, segLen, 0.5f);
            } else {
                for (auto channel = 0; channel < outputChannels; ++channel) {
                    segmentOutputBuffer.copyFrom(channel, 0, pluginBuffer, channel, 0, segLen);
                }
            }

            if (options.numChannels >= 2 && roomReverb.getWetLevel() > 1e-4f) {
                roomReverb.processStereo(segmentOutputBuffer.getWritePointer(0), segmentOutputBuffer.getWritePointer(1),
                                         segLen);
            }

            segmentOutputBuffer.applyGain(currentMasterGain);

            segStart = segEnd;
        }

        applyMasterSoftLimiter(outputBuffer, numSamples);
        if (!writer->writeFromAudioSampleBuffer(outputBuffer, 0, numSamples)) {
            DP_LOG_ERROR("[PluginOfflineRenderer] WAV write failed at block " + juce::String(blockStart));
            writer.reset();
            return false;
        }
        blockStart = blockEnd;
    }

    if (!writer->flush()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Failed to finalise WAV: " + destinationFile.getFullPathName());
        writer.reset();
        return false;
    }
    fileOutput->flush();
    if (fileOutput->getStatus().failed()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Failed to flush WAV: " + destinationFile.getFullPathName());
        writer.reset();
        return false;
    }
    writer.reset();

    if (progressCallback && !progressCallback(1.0)) {
        return false;
    }
    if (!temporaryFile.overwriteTargetFileWithTemporary()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Failed to replace WAV: " + destinationFile.getFullPathName());
        return false;
    }

    DP_LOG_INFO("[PluginOfflineRenderer] Offline render complete: " + destinationFile.getFullPathName());
    return true;
}

bool renderTakeThroughInstrumentEndpoint(const devpiano::recording::RecordingTake& take,
                                         const juce::File& destinationFile, const WavExportOptions& options,
                                         juce::AudioPluginInstance* offlinePluginInstance,
                                         const std::function<bool(double)>& progressCallback) {
    if (offlinePluginInstance != nullptr) {
        return renderTakeWithOfflinePlugin(take, destinationFile, options, *offlinePluginInstance, progressCallback);
    }

    return exportTakeAsWavFile(take, destinationFile, options, progressCallback);
}

} // namespace devpiano::exporting
