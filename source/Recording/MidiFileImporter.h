#pragma once

#include "MidiTrackMergeEngine.h"
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace devpiano::recording {

struct PerformanceEvent;
struct RecordingTake;

struct MidiImportOptions {
    // If true, merge all tracks in the MIDI file. If false, extracts only the primary note-rich track.
    bool mergeAllTracks = true;

    // Channel mapping strategy when merging multiple tracks.
    MidiChannelMappingStrategy channelStrategy = MidiChannelMappingStrategy::autoAssignIfSingleChannel;

    // Legacy compatibility option (when set to true, only the primary note-rich track is imported).
    bool ignoreOtherTracks = false;
};

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate);

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate,
                                            const MidiImportOptions& options);

} // namespace devpiano::recording
