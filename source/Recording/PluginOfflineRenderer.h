#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace devpiano::recording {
struct RecordingTake;
}

namespace devpiano::exporting {

struct WavExportOptions;

// Snapshots the current state of a live plugin instance (preset, parameters, etc.).
// Must be called on the message thread.
[[nodiscard]] juce::MemoryBlock snapshotPluginState(juce::AudioPluginInstance& plugin);

// Creates, bus-configures, and prepares an independent offline plugin instance.
// Must be called on the message thread. Returns nullptr on failure and populates errorMessage.
[[nodiscard]] std::unique_ptr<juce::AudioPluginInstance>
createOfflinePluginInstance(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                            double sampleRate, int blockSize, juce::String& errorMessage);

// Renders a RecordingTake through a prepared offline plugin instance into a WAV file.
// The offline instance must already be created (via createOfflinePluginInstance),
// prepared, and have its state restored before calling this function.
// Can be called from any thread (no message-thread dependencies during the render loop).
// If progressCallback is provided, it is called with progress in [0, 1] after each block.
// Return false from the callback to cancel the export.
bool renderTakeWithOfflinePlugin(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                                 const WavExportOptions& options, juce::AudioPluginInstance& offlinePlugin,
                                 std::function<bool(double)> progressCallback = nullptr);

} // namespace devpiano::exporting
