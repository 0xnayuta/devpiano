#pragma once

#include "RecordingEngine.h"
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace devpiano::recording {

// ============================================================================
// Multi-Track MIDI Channel Mapping Strategy
// ============================================================================
enum class MidiChannelMappingStrategy : std::uint8_t {
    // Keep the original MIDI message channel (1-16) as encoded in the file.
    passThrough = 0,

    // When all tracks encode their notes on a single channel (typically Ch 1),
    // automatically assign each track index to an independent channel (1-16).
    // If tracks already use distinct channels, pass-through is retained.
    autoAssignIfSingleChannel = 1,

    // Force-map each track index modulo 16 to channel (trackIndex % 16 + 1).
    forceTrackToChannel = 2,
};

// ============================================================================
// Time Signature & Key Signature Metadata
// ============================================================================
struct MidiTimeSignature {
    int numerator = 4;
    int denominator = 4;

    [[nodiscard]] juce::String toString() const {
        return juce::String(numerator) + "/" + juce::String(denominator);
    }
};

struct MidiKeySignature {
    int sharpsOrFlats = 0; // -7 .. +7 (negative for flats, positive for sharps)
    bool isMinor = false;

    [[nodiscard]] juce::String toString() const;
};

struct MidiTempoEvent {
    std::int64_t timestampSamples = 0;
    double timestampSeconds = 0.0;
    double bpm = 120.0;
};

struct MidiTrackInfo {
    int trackIndex = 0;
    juce::String trackName;
    int noteCount = 0;
    int primaryChannel = 1; // 1-based (1-16)
    int assignedChannel = 1; // 1-based channel assigned after mapping strategy
};

// ============================================================================
// Comprehensive Multi-Track MIDI File Metadata
// ============================================================================
struct MidiFileMetadata {
    juce::String songTitle;
    juce::String copyright;
    std::vector<MidiTrackInfo> tracks;
    std::vector<MidiTempoEvent> tempoMap;
    std::optional<MidiTimeSignature> initialTimeSignature;
    std::optional<MidiKeySignature> initialKeySignature;
    double initialBpm = 120.0;
    double minBpm = 120.0;
    double maxBpm = 120.0;

    [[nodiscard]] juce::String formatSummary() const;
};

// ============================================================================
// Merge Options
// ============================================================================
struct MidiTrackMergeOptions {
    MidiChannelMappingStrategy channelStrategy = MidiChannelMappingStrategy::autoAssignIfSingleChannel;

    // When true, extract only the single track with the highest note density
    // (legacy single-track mode for compatibility).
    bool singleTrackOnly = false;
};

// ============================================================================
// Merge Statistics
// ============================================================================
struct MidiTrackMergeStats {
    int trackCount = 0;
    int noteOnCount = 0;
    int noteOffCount = 0;
    int zeroVelocityNoteOnCount = 0;
    int ccCount = 0;
    int pitchBendCount = 0;
    int programChangeCount = 0;
    int otherMetaEventCount = 0;
    int mergedEventCount = 0;
    std::int64_t maxTimestampSamples = 0;
    double durationSeconds = 0.0;
};

// ============================================================================
// Merge Result
// ============================================================================
struct MidiTrackMergeResult {
    RecordingTake take;
    MidiTrackMergeStats stats;
    MidiFileMetadata metadata;
};

// ============================================================================
// Multi-Track Timeline Merge Engine
// ============================================================================
class MidiTrackMergeEngine {
public:
    // Merges all tracks in a juce::MidiFile (with timestamps converted to seconds)
    // into a single unified chronological timeline (RecordingTake) with metadata.
    [[nodiscard]] static std::optional<MidiTrackMergeResult>
    mergeTracks(const juce::MidiFile& midiFile, double targetSampleRate, const MidiTrackMergeOptions& options = {});

    // Helper to evaluate relative MIDI event priority when timestamps are equal.
    // Order: Program Change (1) -> Controller (2) -> Note Off (3) -> Note On (4) -> Others (5).
    [[nodiscard]] static int getMidiEventPriority(const juce::MidiMessage& message) noexcept;
};

} // namespace devpiano::recording
