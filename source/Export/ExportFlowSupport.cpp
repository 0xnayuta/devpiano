#include "ExportFlowSupport.h"

#include "Recording/RecordingEngine.h"

namespace devpiano::exporting {
namespace {
[[nodiscard]] juce::String getExtension(ExportFileType type) {
    switch (type) {
    case ExportFileType::midi:
        return ".mid";
    case ExportFileType::wav:
        return ".wav";
    }

    return {};
}
} // namespace

juce::File makeDefaultRecordingExportFile(ExportFileType type, const juce::File& directory, juce::Time now) {
    return directory.getChildFile("recording_" + now.toISO8601(false).replaceCharacters(":", "-") + getExtension(type));
}

juce::File getLastMidiExportDirectory(const SettingsModel& settings) {
    if (settings.lastMidiExportPath.isNotEmpty()) {
        auto lastFile = juce::File(settings.lastMidiExportPath);
        if (lastFile.existsAsFile()) {
            return lastFile.getParentDirectory();
        }

        if (lastFile.isDirectory()) {
            return lastFile;
        }

        const auto parent = lastFile.getParentDirectory();
        if (parent.isDirectory()) {
            return parent;
        }
    }

    return juce::File::getCurrentWorkingDirectory();
}

juce::File getLastMidiImportDirectory(const SettingsModel& settings) {
    if (settings.lastMidiImportPath.isNotEmpty()) {
        const auto lastFile = juce::File(settings.lastMidiImportPath);
        if (lastFile.exists()) {
            return lastFile.getParentDirectory();
        }
    }

    return juce::File {};
}

bool canExportTake(const devpiano::recording::RecordingTake& take) {
    return !take.isEmpty();
}

WavExportOptions buildWavExportOptions(const devpiano::recording::RecordingTake& take,
                                       const SettingsModel::PerformanceSettingsView& performance,
                                       double runtimeSampleRate, int runtimeBlockSize) {
    double exportSampleRate = 44100.0;
    if (runtimeSampleRate > 0.0) {
        exportSampleRate = runtimeSampleRate;
    } else if (take.sampleRate > 0.0) {
        exportSampleRate = take.sampleRate;
    }

    WavExportOptions options;
    options.sampleRate = exportSampleRate;
    options.numChannels = 2;
    options.blockSize = juce::jmax(1, runtimeBlockSize);
    options.masterGain = performance.masterGain;
    options.adsr.attack = performance.adsrAttack;
    options.adsr.decay = performance.adsrDecay;
    options.adsr.sustain = performance.adsrSustain;
    options.adsr.release = performance.adsrRelease;
    options.builtinTone = performance.builtinTone;
    options.pianoBrightness = performance.pianoBrightness;
    options.pianoHammerHardness = performance.pianoHammerHardness;
    options.pianoResonance = performance.pianoResonance;
    return options;
}

juce::String makeExportLogPrefix(ExportFileType type) {
    switch (type) {
    case ExportFileType::midi:
        return "[Export] MIDI";
    case ExportFileType::wav:
        return "[Export] WAV";
    }

    return "[Export]";
}

} // namespace devpiano::exporting
