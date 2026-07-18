#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace devpiano::recording {
struct RecordingTake;
}

#include "Export/WavExportOptions.h"
namespace devpiano::exporting {

//
// Offline-render lifecycle (3 phases):
//
//   Phase 1 — Message thread (caller):
//     1. snapshotPluginState(liveInstance)          — capture state from live plugin
//     2. createOfflinePluginInstance(...)           — create + bus-config + prepareToPlay + reset
//     3. offlinePlugin->setStateInformation(state)  — restore captured state onto offline instance
//     4. WavExportTask(std::move(offlinePlugin))    — transfer ownership to background task
//
//   Phase 2 — Thread transition:
//     WavExportTask::runThread() blocks the message thread with a nested loop;
//     WavExportTask::run() executes on the internal background thread.
//
//   Phase 3 — Background thread (WavExportTask::run):
//     renderTakeWithOfflinePlugin(...)              — render loop, no message-thread deps
//

// Captures the current state (preset, parameters, etc.) from a live plugin instance.
// Must be called on the message thread (Phase 1).
// The returned MemoryBlock should later be restored onto the offline instance
// via offlinePlugin->setStateInformation() after createOfflinePluginInstance succeeds.
[[nodiscard]] juce::MemoryBlock snapshotPluginState(juce::AudioPluginInstance& plugin);

// Creates, bus-configures, and prepares an independent offline plugin instance.
// Internally calls:
//   formatManager.createPluginInstance(...)         [requires message thread]
//   PluginHost::configureDefaultBuses(...)
//   instance->setRateAndBufferSizeDetails(...)
//   instance->prepareToPlay(...)
//   instance->reset()
//
// Must be called on the message thread (AudioPluginFormatManager is not thread-safe).
// Returns nullptr on failure and populates errorMessage.
//
// Phase 1 of the offline-render lifecycle (message thread).
// After this succeeds, the caller should restore the captured plugin state:
//   offlinePlugin->setStateInformation(state)
// then transfer ownership to WavExportTask for Phase 2->3.
[[nodiscard]] std::unique_ptr<juce::AudioPluginInstance>
createOfflinePluginInstance(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                            double sampleRate, int blockSize, juce::String& errorMessage);

// Renders a RecordingTake through a prepared offline plugin instance into a WAV file.
//
// The offline instance MUST have completed Phase 1 setup on the message thread:
//   - createOfflinePluginInstance succeeded
//   - setStateInformation (state restoration) was called
//
// Phase 3 of the offline-render lifecycle (background thread).
// Can be called from any thread (no message-thread dependencies during the render loop).
// If progressCallback is provided, it is called with progress in [0, 1] after each block.
// Return false from the callback to cancel the export.
bool renderTakeWithOfflinePlugin(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                                 const WavExportOptions& options, juce::AudioPluginInstance& offlinePlugin,
                                 std::function<bool(double)> progressCallback = nullptr);

} // namespace devpiano::exporting
