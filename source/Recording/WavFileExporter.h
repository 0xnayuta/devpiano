#pragma once

#include <JuceHeader.h>

#include <functional>

namespace devpiano::recording {
struct RecordingTake;
}

#include "Export/WavExportOptions.h"
namespace devpiano::exporting {

// Exports a RecordingTake as a WAV file using the built-in sine synth.
// If progressCallback is provided, it is called with progress in [0, 1] after each block.
// Return false from the callback to cancel the export.
bool exportTakeAsWavFile(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                         const WavExportOptions& options, std::function<bool(double)> progressCallback = nullptr);

} // namespace devpiano::exporting
