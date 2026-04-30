#pragma once

#include <JuceHeader.h>

#include "Recording/WavFileExporter.h"
#include "Settings/SettingsModel.h"

namespace devpiano::recording
{
struct RecordingTake;
}

namespace devpiano::exporting
{

enum class ExportFileType
{
    midi,
    wav
};

[[nodiscard]] juce::File makeDefaultRecordingExportFile(ExportFileType type,
                                                        juce::File directory = juce::File::getCurrentWorkingDirectory(),
                                                        juce::Time now = juce::Time::getCurrentTime());

[[nodiscard]] bool canExportTake(const devpiano::recording::RecordingTake& take);

[[nodiscard]] WavExportOptions buildWavExportOptions(const devpiano::recording::RecordingTake& take,
                                                     const SettingsModel::PerformanceSettingsView& performance,
                                                     double runtimeSampleRate,
                                                     int runtimeBlockSize);

[[nodiscard]] juce::String makeExportLogPrefix(ExportFileType type);

} // namespace devpiano::exporting
