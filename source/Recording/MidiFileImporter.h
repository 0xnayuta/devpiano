#pragma once

#include "MidiTrackMergeEngine.h"
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace devpiano::recording {

struct PerformanceEvent;
struct RecordingTake;

struct MidiImportOptions {
    /// Channel mapping strategy when merging multiple tracks.
    MidiChannelMappingStrategy channelStrategy = MidiChannelMappingStrategy::autoAssignIfSingleChannel;
};

// Imports a MIDI file and returns the merged RecordingTake.
std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate);

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate,
                                            const MidiImportOptions& options);

// Imports a MIDI file and returns the full merge result including metadata and stats.
std::optional<MidiTrackMergeResult> importMidiFileWithMetadata(const juce::File& midiFile, double targetSampleRate,
                                                               const MidiImportOptions& options = {});

} // namespace devpiano::recording
