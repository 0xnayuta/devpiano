#include "Recording/PluginOfflineRenderer.h"

#include "Diagnostics/DebugLog.h"
#include "Plugin/PluginHost.h"
#include "Recording/RecordingEngine.h"
#include "Recording/WavFileExporter.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace devpiano::exporting {
namespace {

struct RenderEvent {
    juce::MidiMessage message;
    std::int64_t timestampSamples = 0;
};

[[nodiscard]] bool hasUsableOptions(const WavExportOptions& options) noexcept {
    return options.sampleRate > 0.0 && options.numChannels > 0 && options.blockSize > 0 && options.bitsPerSample > 0;
}

[[nodiscard]] std::int64_t scaleTimestamp(std::int64_t timestampSamples, double ratio) noexcept {
    return static_cast<std::int64_t>(std::llround(static_cast<double>(timestampSamples) * ratio));
}

[[nodiscard]] std::vector<RenderEvent> buildRenderEvents(const devpiano::recording::RecordingTake& take,
                                                         double targetSampleRate) {
    std::vector<RenderEvent> events;
    events.reserve(take.events.size());

    const auto ratio = take.sampleRate > 0.0 ? targetSampleRate / take.sampleRate : 1.0;

    for (const auto& perfEvent : take.events) {
        auto msg = perfEvent.message;
        msg.setTimeStamp(0.0);

        RenderEvent event;
        event.message = msg;
        event.timestampSamples = scaleTimestamp(perfEvent.timestampSamples, ratio);
        events.push_back(std::move(event));
    }

    std::stable_sort(events.begin(), events.end(), [](const RenderEvent& a, const RenderEvent& b) {
        return a.timestampSamples < b.timestampSamples;
    });

    return events;
}

[[nodiscard]] std::int64_t getScaledTakeLengthSamples(const devpiano::recording::RecordingTake& take,
                                                      const std::vector<RenderEvent>& events,
                                                      double targetSampleRate) noexcept {
    if (events.empty())
        return 0;

    const auto lastTimestamp = events.back().timestampSamples;
    const auto ratio = take.sampleRate > 0.0 ? targetSampleRate / take.sampleRate : 1.0;
    const auto scaledLengthFromTake = scaleTimestamp(take.lengthSamples, ratio);

    return std::max(lastTimestamp + 1, scaledLengthFromTake);
}

void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) {
    for (auto channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
}

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
    instance->reset();

    DP_LOG_INFO("[PluginOfflineRenderer] Offline instance created and prepared: " + description.name + " @ "
                + juce::String(sampleRate) + "Hz, block=" + juce::String(blockSize));
    return instance;
}

// ---------------------------------------------------------------------------
// renderTakeWithOfflinePlugin
// ---------------------------------------------------------------------------
bool renderTakeWithOfflinePlugin(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                                 const WavExportOptions& options, juce::AudioPluginInstance& offlinePlugin) {
    if (take.isEmpty() || take.sampleRate <= 0.0 || !hasUsableOptions(options) || destinationFile == juce::File()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Invalid parameters for offline render");
        return false;
    }

    // Create output directory if needed
    auto parentDirectory = destinationFile.getParentDirectory();
    if (!parentDirectory.exists() && !parentDirectory.createDirectory()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Cannot create output directory: " + parentDirectory.getFullPathName());
        return false;
    }

    // Open output file
    auto fileStream = std::make_unique<juce::FileOutputStream>(destinationFile);
    if (!fileStream->openedOk()) {
        DP_LOG_ERROR("[PluginOfflineRenderer] Cannot open output file: " + destinationFile.getFullPathName());
        return false;
    }
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

    // Build render events from take (timestamp-scaled to target sample rate)
    auto renderEvents = buildRenderEvents(take, options.sampleRate);
    const auto scaledTakeLength = getScaledTakeLengthSamples(take, renderEvents, options.sampleRate);

    constexpr auto tailSeconds = 2.0;
    const auto tailSamples = static_cast<std::int64_t>(std::ceil(tailSeconds * options.sampleRate));
    const auto totalSamples = std::max<std::int64_t>(1, scaledTakeLength + tailSamples);
    const auto gain = juce::jlimit(0.0f, 1.0f, options.masterGain);

    // Determine channel count from the plugin instance
    const auto requiredPluginChannels = juce::jmax(
        1, juce::jmax(offlinePlugin.getTotalNumInputChannels(), offlinePlugin.getTotalNumOutputChannels()));
    const auto outputChannels = juce::jmin(options.numChannels, offlinePlugin.getTotalNumOutputChannels());

    juce::AudioBuffer<float> pluginBuffer(requiredPluginChannels, options.blockSize);
    juce::MidiBuffer midiBuffer;
    midiBuffer.ensureSize(static_cast<size_t>(juce::jlimit(256, 65536, options.blockSize * 16)));

    juce::AudioBuffer<float> outputBuffer(options.numChannels, options.blockSize);

    std::size_t eventIndex = 0;
    auto allNotesOffSent = false;

    DP_LOG_INFO("[PluginOfflineRenderer] Starting offline render: " + juce::String(renderEvents.size()) + " events, "
                + juce::String(totalSamples) + " total samples, " + juce::String(outputChannels) + " output channels");

    for (std::int64_t blockStart = 0; blockStart < totalSamples; blockStart += options.blockSize) {
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
        for (auto channel = 0; channel < outputChannels; ++channel)
            outputBuffer.copyFrom(channel, 0, pluginBuffer, channel, 0, numSamples);
        outputBuffer.applyGain(gain);

        if (!writer->writeFromAudioSampleBuffer(outputBuffer, 0, numSamples)) {
            DP_LOG_ERROR("[PluginOfflineRenderer] WAV write failed at block " + juce::String(blockStart));
            return false;
        }
    }

    DP_LOG_INFO("[PluginOfflineRenderer] Offline render complete: " + destinationFile.getFullPathName());
    return true;
}

} // namespace devpiano::exporting
