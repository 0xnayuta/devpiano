#pragma once

#include <JuceHeader.h>

#include <optional>
#include <string>

namespace devpiano::recording
{

struct RecordingTake;

// --- .devpiano JSON format constants ---

namespace performance_file
{
    constexpr int currentVersion = 1;
    constexpr const char* formatIdentifier = "devpiano-performance";

    // Top-level keys
    constexpr const char* keyVersion       = "version";
    constexpr const char* keyFormat        = "format";
    constexpr const char* keySampleRate    = "sampleRate";
    constexpr const char* keyLengthSamples = "lengthSamples";
    constexpr const char* keyMetadata      = "metadata";
    constexpr const char* keyEvents        = "events";

    // Metadata keys
    constexpr const char* keyCreatedAt = "createdAt";
    constexpr const char* keyTitle     = "title";
    constexpr const char* keyNotes     = "notes";

    // Event keys
    constexpr const char* keyTimestampSamples = "timestampSamples";
    constexpr const char* keySource           = "source";
    constexpr const char* keyMidiData         = "midiData";

    // Source enum strings
    constexpr const char* sourceComputerKeyboard = "computerKeyboard";
    constexpr const char* sourceExternalMidi     = "externalMidi";
    constexpr const char* sourceRealtimeMidiBuffer = "realtimeMidiBuffer";
    constexpr const char* sourcePlayback         = "playback";
} // namespace performance_file

// --- Metadata for .devpiano files ---

struct PerformanceFileMetadata
{
    juce::String createdAt;   // ISO 8601
    juce::String title;
    juce::String notes;
};

// --- Save / Load API ---

// Serialise a RecordingTake to a .devpiano JSON file.
// Returns true on success.
bool savePerformanceFile(const RecordingTake& take,
                         const juce::File& destinationFile,
                         const PerformanceFileMetadata& metadata = {});

// Deserialise a .devpiano JSON file into a RecordingTake.
// Returns std::nullopt on failure (parse error, wrong format, missing fields).
std::optional<RecordingTake> loadPerformanceFile(const juce::File& sourceFile);

// --- Low-level serialisation (for testing / reuse) ---

// Serialise a RecordingTake to a JSON string.
juce::String serialiseTakeToJson(const RecordingTake& take,
                                 const PerformanceFileMetadata& metadata = {});

// Deserialise a JSON string into a RecordingTake.
// Returns std::nullopt on failure.
std::optional<RecordingTake> deserialiseTakeFromJson(const juce::String& json);

} // namespace devpiano::recording
