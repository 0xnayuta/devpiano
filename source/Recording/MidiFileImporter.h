#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace devpiano::recording {

struct PerformanceEvent;
struct RecordingTake;

struct MidiImportOptions {
    bool ignoreOtherTracks = true;
};

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate);

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate,
                                            const MidiImportOptions& options);

} // namespace devpiano::recording