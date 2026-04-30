#pragma once

#include <JuceHeader.h>

#include <optional>
#include <vector>

namespace devpiano::recording
{

struct PerformanceEvent;
struct RecordingTake;

struct MidiImportOptions
{
    bool ignoreOtherTracks = true;
    int preferTrack = 0;
};

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile,
                                            double targetSampleRate);

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile,
                                            double targetSampleRate,
                                            const MidiImportOptions& options);

} // namespace devpiano::recording