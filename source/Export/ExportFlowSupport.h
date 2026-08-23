#pragma once

#include "Export/WavExportOptions.h"
#include "Settings/SettingsModel.h"
#include <juce_core/juce_core.h>

namespace devpiano::recording {
struct RecordingTake;
}

namespace devpiano::exporting {

enum class ExportFileType : std::uint8_t { midi, wav };

[[nodiscard]] juce::File makeDefaultRecordingExportFile(ExportFileType type,
                                                        const juce::File& directory
                                                        = juce::File::getCurrentWorkingDirectory(),
                                                        juce::Time now = juce::Time::getCurrentTime());

// Resolve the default directory for the MIDI-export FileChooser from the
// persisted last-export path: an existing file yields its parent directory, an
// existing directory yields itself, a stale path falls back to its parent (if
// that exists), and an empty setting falls back to the CWD.
[[nodiscard]] juce::File getLastMidiExportDirectory(const SettingsModel& settings);

// Resolve the default directory for the MIDI-import FileChooser.  Only an
// existing path is honoured (its parent directory); anything else yields an
// empty File (FileChooser uses its own default).
[[nodiscard]] juce::File getLastMidiImportDirectory(const SettingsModel& settings);

[[nodiscard]] bool canExportTake(const devpiano::recording::RecordingTake& take);

[[nodiscard]] WavExportOptions buildWavExportOptions(const devpiano::recording::RecordingTake& take,
                                                     const SettingsModel::PerformanceSettingsView& performance,
                                                     double runtimeSampleRate, int runtimeBlockSize);

[[nodiscard]] juce::String makeExportLogPrefix(ExportFileType type);

} // namespace devpiano::exporting
