#pragma once

#include "MidiTrackMergeEngine.h"
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace devpiano::recording {

struct PerformanceEvent;
struct RecordingTake;

struct MidiImportOptions {
    /// If true (default in Phase 26+), all tracks in the MIDI file are merged with synchronized timestamps.
    /// If false, only the primary note-rich track is imported (legacy single-track mode).
    bool mergeAllTracks = true;

    /// Channel mapping strategy when merging multiple tracks.
    MidiChannelMappingStrategy channelStrategy = MidiChannelMappingStrategy::autoAssignIfSingleChannel;

    /// Legacy compatibility alias (setting ignoreOtherTracks = true forces single-track mode).
    bool ignoreOtherTracks = false;

    /// Returns true if single-track mode is requested by either setting.
    [[nodiscard]] bool isSingleTrackOnly() const noexcept {
        return ignoreOtherTracks || !mergeAllTracks;
    }
};

// Imports a MIDI file and returns the merged RecordingTake.
std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate);

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate,
                                            const MidiImportOptions& options);

// Imports a MIDI file and returns the full merge result including metadata and stats.
std::optional<MidiTrackMergeResult> importMidiFileWithMetadata(const juce::File& midiFile, double targetSampleRate,
                                                               const MidiImportOptions& options = {});

} // namespace devpiano::recording
