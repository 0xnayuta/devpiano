#include "ExportFlowSupport.h"

#include "Recording/RecordingEngine.h"

namespace devpiano::exporting
{
namespace
{
[[nodiscard]] juce::String getExtension(ExportFileType type)
{
    switch (type)
    {
        case ExportFileType::midi: return ".mid";
        case ExportFileType::wav: return ".wav";
    }

    return {};
}
} // namespace

juce::File makeDefaultRecordingExportFile(ExportFileType type,
                                          juce::File directory,
                                          juce::Time now)
{
    return directory.getChildFile("recording_"
                                  + now.toISO8601(false).replaceCharacters(":", "-")
                                  + getExtension(type));
}

bool canExportTake(const devpiano::recording::RecordingTake& take)
{
    return ! take.isEmpty();
}

WavExportOptions buildWavExportOptions(const devpiano::recording::RecordingTake& take,
                                       const SettingsModel::PerformanceSettingsView& performance,
                                       double runtimeSampleRate,
                                       int runtimeBlockSize)
{
    const auto exportSampleRate = runtimeSampleRate > 0.0
                                      ? runtimeSampleRate
                                      : (take.sampleRate > 0.0 ? take.sampleRate : 44100.0);

    WavExportOptions options;
    options.sampleRate = exportSampleRate;
    options.numChannels = 2;
    options.blockSize = juce::jmax(1, runtimeBlockSize);
    options.masterGain = performance.masterGain;
    options.adsr.attack = performance.adsrAttack;
    options.adsr.decay = performance.adsrDecay;
    options.adsr.sustain = performance.adsrSustain;
    options.adsr.release = performance.adsrRelease;
    return options;
}

juce::String makeExportLogPrefix(ExportFileType type)
{
    switch (type)
    {
        case ExportFileType::midi: return "[Export] MIDI";
        case ExportFileType::wav: return "[Export] WAV";
    }

    return "[Export]";
}

} // namespace devpiano::exporting
