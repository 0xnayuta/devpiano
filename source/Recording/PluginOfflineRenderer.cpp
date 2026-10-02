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
using devpiano::recording::hasUsableRenderOptions;
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

    const auto gain = juce::jlimit(0.0f, 1.0f, options.masterGain);

    // Determine channel count from the plugin instance
    const auto requiredPluginChannels = juce::jmax(
        1, juce::jmax(offlinePlugin.getTotalNumInputChannels(), offlinePlugin.getTotalNumOutputChannels()));
    const auto outputChannels = juce::jmin(options.numChannels, offlinePlugin.getTotalNumOutputChannels());

    juce::AudioBuffer<float> pluginBuffer(requiredPluginChannels, options.blockSize);
    juce::MidiBuffer midiBuffer;
    midiBuffer.ensureSize(
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
            return false;
        }

        const auto numSamples = static_cast<int>(std::min<std::int64_t>(options.blockSize, totalSamples - blockStart));
        const auto blockEnd = blockStart + numSamples;

        // Prepare buffers for this block
        pluginBuffer.setSize(requiredPluginChannels, numSamples, false, false, true);
        pluginBuffer.clear();
        midiBuffer.clear();

        // Schedule events that fall within this block
        while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples < blockEnd) {
            const auto& event = renderEvents[eventIndex];
            if (event.timestampSamples >= blockStart) {
                const auto sampleOffset = static_cast<int>(event.timestampSamples - blockStart);
                midiBuffer.addEvent(event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
            }
            ++eventIndex;
        }

        // Send all-notes-off across all 16 MIDI channels at the end of the take content
        if (!allNotesOffSent && scaledTakeLength >= blockStart && scaledTakeLength < blockEnd) {
            const auto offset = juce::jlimit(0, numSamples - 1, static_cast<int>(scaledTakeLength - blockStart));
            addPanicMidi(midiBuffer, offset);
            allNotesOffSent = true;
        }

        // Process through the offline plugin instance
        offlinePlugin.processBlock(pluginBuffer, midiBuffer);

        // Copy plugin output (with channel down-mix if needed) and apply master gain
        outputBuffer.setSize(options.numChannels, numSamples, false, false, true);
        outputBuffer.clear();

        const auto pluginOutputChannels = offlinePlugin.getTotalNumOutputChannels();
        if (options.numChannels == 2 && pluginOutputChannels == 1) {
            // Mono plugin rendered to stereo: duplicate mono channel to L & R
            outputBuffer.copyFrom(0, 0, pluginBuffer, 0, 0, numSamples);
            outputBuffer.copyFrom(1, 0, pluginBuffer, 0, 0, numSamples);
        } else if (options.numChannels == 1 && pluginOutputChannels >= 2) {
            // Stereo/multi-channel plugin rendered to mono: downmix L + R
            outputBuffer.copyFrom(0, 0, pluginBuffer, 0, 0, numSamples);
            outputBuffer.addFrom(0, 0, pluginBuffer, 1, 0, numSamples);
            outputBuffer.applyGain(0, 0, numSamples, 0.5f);
        } else {
            for (auto channel = 0; channel < outputChannels; ++channel) {
                outputBuffer.copyFrom(channel, 0, pluginBuffer, channel, 0, numSamples);
            }
        }
        if (options.numChannels >= 2 && options.reverbWet > 1e-4f) {
            roomReverb.processStereo(outputBuffer.getWritePointer(0), outputBuffer.getWritePointer(1), numSamples);
        }

        outputBuffer.applyGain(gain);
        applyMasterSoftLimiter(outputBuffer, numSamples);
        if (!writer->writeFromAudioSampleBuffer(outputBuffer, 0, numSamples)) {
            DP_LOG_ERROR("[PluginOfflineRenderer] WAV write failed at block " + juce::String(blockStart));
            return false;
        }
        blockStart = blockEnd;
    }

    if (!writer->flush()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Failed to finalise WAV: " + destinationFile.getFullPathName());
        return false;
    }
    fileOutput->flush();
    if (fileOutput->getStatus().failed()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Failed to flush WAV: " + destinationFile.getFullPathName());
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
