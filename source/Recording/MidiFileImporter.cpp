#include "MidiFileImporter.h"

#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"
#include "MidiTrackMergeEngine.h"
#include "RecordingEngine.h"

namespace {

bool readMidiFile(juce::MidiFile& midiFile, const juce::File& file) {
    std::unique_ptr<juce::FileInputStream> stream { file.createInputStream() };
    if (!stream) {
        DP_LOG_ERROR("MidiFileImporter: could not open file for reading: " + file.getFullPathName());
        return false;
    }

    if (!midiFile.readFrom(*stream, true)) {
        DP_LOG_ERROR("MidiFileImporter: failed to read file: " + file.getFullPathName());
        return false;
    }

    return true;
}

} // namespace

namespace devpiano::recording {

std::optional<MidiTrackMergeResult> importMidiFileWithMetadata(const juce::File& midiFile, double targetSampleRate,
                                                               const MidiImportOptions& options) {
    if (!midiFile.exists()) {
        DP_LOG_ERROR("MidiFileImporter: file does not exist: " + midiFile.getFullPathName());
        return std::nullopt;
    }

    if (midiFile.getSize() == 0) {
        DP_LOG_ERROR("MidiFileImporter: file is empty: " + midiFile.getFullPathName());
        return std::nullopt;
    }

    juce::MidiFile file;
    if (!readMidiFile(file, midiFile)) {
        return std::nullopt;
    }

    DP_TRACE_MIDI("MidiFile imported: " + midiFile.getFileName() + ", tracks=" + juce::String(file.getNumTracks()),
                  "MidiImporter");
#if defined(JUCE_DEBUG) || defined(DEBUG)
    const auto timeFormat = file.getTimeFormat();
    if (timeFormat < 0) {
        const auto fps = -(timeFormat >> 8);
        const auto subframes = timeFormat & 0xff;
        DP_DEBUG_LOG("MidiFileImporter: SMPTE timing detected: " + juce::String(fps) + " fps, "
                     + juce::String(subframes) + " subframes/frame");
    } else {
        DP_DEBUG_LOG("MidiFileImporter: PPQ = " + juce::String(timeFormat));
    }
#endif

    // Do NOT override timeFormat - readFrom() has already set it correctly from the
    // MIDI file header. Convert native tick timestamps -> seconds across all tracks.
    file.convertTimestampTicksToSeconds();

    MidiTrackMergeOptions mergeOptions;
    mergeOptions.channelStrategy = options.channelStrategy;
    mergeOptions.singleTrackOnly = options.isSingleTrackOnly();
    auto mergeResult = MidiTrackMergeEngine::mergeTracks(file, targetSampleRate, mergeOptions);
    if (!mergeResult.has_value()) {
        DP_LOG_ERROR("MidiFileImporter: failed to merge tracks from " + midiFile.getFileName());
        return std::nullopt;
    }

    DP_LOG_INFO("MidiFileImporter: successfully imported " + midiFile.getFileName() + " ("
                + juce::String(mergeResult->stats.mergedEventCount) + " events, "
                + juce::String(mergeResult->stats.durationSeconds, 2) + "s, tracks="
                + juce::String(mergeResult->stats.trackCount) + ") | " + mergeResult->metadata.formatSummary());

    return mergeResult;
}

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate) {
    return importMidiFile(midiFile, targetSampleRate, MidiImportOptions {});
}

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate,
                                            const MidiImportOptions& options) {
    auto result = importMidiFileWithMetadata(midiFile, targetSampleRate, options);
    if (!result.has_value()) {
        return std::nullopt;
    }
    return std::move(result->take);
}

} // namespace devpiano::recording
