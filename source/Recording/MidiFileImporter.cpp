#include "MidiFileImporter.h"

#include "Diagnostics/DebugLog.h"
#include "Diagnostics/MidiTrace.h"
#include "RecordingEngine.h"

namespace
{

struct TrackNoteStats
{
    int trackIndex = -1;
    int noteOnCount = 0;
    int noteOffCount = 0;
    int zeroVelocityNoteOnCount = 0;

    [[nodiscard]] int totalNoteEvents() const noexcept { return noteOnCount + noteOffCount; }
};

bool readMidiFile(juce::MidiFile& midiFile, const juce::File& file)
{
    std::unique_ptr<juce::FileInputStream> stream { file.createInputStream() };
    if (!stream)
    {
        DP_LOG_ERROR("MidiFileImporter: could not open file for reading: " + file.getFullPathName());
        return false;
    }

    if (!midiFile.readFrom(*stream, true))
    {
        DP_LOG_ERROR("MidiFileImporter: failed to read file: " + file.getFullPathName());
        return false;
    }

    return true;
}

TrackNoteStats countTrackNoteEvents(const juce::MidiMessageSequence& track, int trackIndex)
{
    TrackNoteStats stats;
    stats.trackIndex = trackIndex;

    for (int i = 0; i < track.getNumEvents(); ++i)
    {
        const auto* msg = track.getEventPointer(i);
        if (msg == nullptr)
            continue;

        const auto& midiMsg = msg->message;
        const auto isRawNoteOn = midiMsg.isNoteOn(true);
        const auto isZeroVelocityNoteOn = isRawNoteOn && midiMsg.getVelocity() == 0.0f;
        const auto isNoteOn = midiMsg.isNoteOn(false);
        const auto isNoteOff = midiMsg.isNoteOff(true);

        if (isZeroVelocityNoteOn)
            ++stats.zeroVelocityNoteOnCount;

        if (isNoteOn)
            ++stats.noteOnCount;
        else if (isNoteOff)
            ++stats.noteOffCount;
    }

    return stats;
}

std::vector<TrackNoteStats> collectTrackNoteStats(const juce::MidiFile& midiFile)
{
    std::vector<TrackNoteStats> stats;
    stats.reserve(static_cast<std::size_t>(midiFile.getNumTracks()));

    for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
    {
        const auto* track = midiFile.getTrack(trackIndex);
        if (track == nullptr)
        {
            stats.push_back({ .trackIndex = trackIndex });
            continue;
        }

        stats.push_back(countTrackNoteEvents(*track, trackIndex));
    }

    return stats;
}

int getTotalNoteEvents(const std::vector<TrackNoteStats>& stats)
{
    int total = 0;
    for (const auto& trackStats : stats)
        total += trackStats.totalNoteEvents();
    return total;
}

int chooseNoteRichTrack(const std::vector<TrackNoteStats>& stats, int preferredTrack)
{
    juce::ignoreUnused(preferredTrack);

    auto selectedTrack = -1;
    auto selectedNoteCount = 0;

    for (const auto& trackStats : stats)
    {
        const auto noteCount = trackStats.totalNoteEvents();
        if (noteCount > selectedNoteCount)
        {
            selectedTrack = trackStats.trackIndex;
            selectedNoteCount = noteCount;
        }
    }

    return selectedTrack;
}

void logTrackNoteStats(const std::vector<TrackNoteStats>& stats)
{
    for (const auto& trackStats : stats)
    {
        DP_LOG_INFO("MidiFileImporter: track " + juce::String(trackStats.trackIndex)
                    + " notes=" + juce::String(trackStats.totalNoteEvents())
                    + " (on=" + juce::String(trackStats.noteOnCount)
                    + ", off=" + juce::String(trackStats.noteOffCount)
                    + ", zero-velocity-on=" + juce::String(trackStats.zeroVelocityNoteOnCount) + ")");
    }
}

} // namespace

namespace devpiano::recording
{

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile,
                                            double targetSampleRate)
{
    return importMidiFile(midiFile, targetSampleRate, MidiImportOptions {});
}

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile,
                                            double targetSampleRate,
                                            const MidiImportOptions& options)
{
    if (!midiFile.exists())
    {
        DP_LOG_ERROR("MidiFileImporter: file does not exist: " + midiFile.getFullPathName());
        return std::nullopt;
    }

    if (midiFile.getSize() == 0)
    {
        DP_LOG_ERROR("MidiFileImporter: file is empty: " + midiFile.getFullPathName());
        return std::nullopt;
    }

    juce::MidiFile file;
    if (!readMidiFile(file, midiFile))
        return std::nullopt;

    DP_TRACE_MIDI("MidiFile imported: " + midiFile.getFileName() + ", tracks=" + juce::String(file.getNumTracks()), "MidiImporter");
    const auto timeFormat = file.getTimeFormat();
    if (timeFormat < 0)
    {
        const auto fps = -(timeFormat >> 8);
        const auto subframes = timeFormat & 0xff;
        DP_DEBUG_LOG("MidiFileImporter: SMPTE timing detected: " + juce::String(fps)
                     + " fps, " + juce::String(subframes) + " subframes/frame");
    }
    else
    {
        DP_DEBUG_LOG("MidiFileImporter: PPQ = " + juce::String(timeFormat));
    }

    // Do NOT override timeFormat - readFrom() has already set it correctly from the
    // MIDI file header. The timeFormat encodes either PPQ (positive) or SMPTE
    // (negative frames-per-second << 8 | subframes-per-frame). Overwriting it would
    // corrupt timestamps for files that use non-960 PPQ or SMPTE timing.
    // Leave timeFormat untouched and just convert ticks -> seconds using the
    // file's native timing.

    file.convertTimestampTicksToSeconds();

    const auto numTracks = file.getNumTracks();
    if (numTracks == 0)
    {
        DP_LOG_ERROR("MidiFileImporter: no tracks found in file");
        return std::nullopt;
    }

    const auto trackStats = collectTrackNoteStats(file);
    const auto totalNoteEventsAllTracks = getTotalNoteEvents(trackStats);
    DP_LOG_INFO("MidiFileImporter: " + juce::String(numTracks) + " tracks, "
                + juce::String(totalNoteEventsAllTracks) + " total note events across all tracks");
    logTrackNoteStats(trackStats);

    const auto selectedTrackIndex = chooseNoteRichTrack(trackStats, options.preferTrack);
    if (selectedTrackIndex < 0)
    {
        DP_LOG_ERROR("MidiFileImporter: no note events found in any track");
        return std::nullopt;
    }

    if (options.preferTrack != selectedTrackIndex)
        DP_LOG_INFO("MidiFileImporter: auto-selected track " + juce::String(selectedTrackIndex)
                    + " instead of preferred track " + juce::String(options.preferTrack));

    if (numTracks > 1 && options.ignoreOtherTracks)
        DP_LOG_INFO("MidiFileImporter: imported track " + juce::String(selectedTrackIndex)
                    + ", " + juce::String(numTracks - 1) + " other tracks ignored");

    const auto* track = file.getTrack(selectedTrackIndex);
    if (track == nullptr)
    {
        DP_LOG_ERROR("MidiFileImporter: selected track " + juce::String(selectedTrackIndex) + " not found");
        return std::nullopt;
    }

    std::vector<PerformanceEvent> events;
    events.reserve(static_cast<std::size_t>(track->getNumEvents()));

    int64_t lastTimestampSamples = 0;
    int noteOnCount = 0;
    int noteOffCount = 0;
    int zeroVelocityNoteOnCount = 0;

    for (int i = 0; i < track->getNumEvents(); ++i)
    {
        const auto* msg = track->getEventPointer(i);
        if (msg == nullptr)
            continue;

        const auto& midiMsg = msg->message;

        // Note On with velocity 0 is technically a Note Off in MIDI spec.
        const bool isNoteOnWithVelocityZero = midiMsg.isNoteOn(true) && midiMsg.getVelocity() == 0.0f;
        const bool isNoteOnWithVelocityNonZero = midiMsg.isNoteOn(false);
        const bool isNoteOffEvent = midiMsg.isNoteOff(true);

        if (!isNoteOnWithVelocityNonZero && !isNoteOffEvent)
        {
            // Trace non-note events (CC, pitch bend, etc.) for Phase 6-5 MIDI import diagnostics
            DP_TRACE_MIDI(devpiano::diagnostics::describeMidiMessage(midiMsg), "MidiImporter");
            continue;
        }

        if (isNoteOnWithVelocityZero)
            ++zeroVelocityNoteOnCount;

        if (midiMsg.isNoteOn())
            ++noteOnCount;
        else
            ++noteOffCount;

        const auto timestampSeconds = midiMsg.getTimeStamp();
        if (timestampSeconds < 0.0)
            continue;

        const auto timestampSamples = static_cast<int64_t>(timestampSeconds * targetSampleRate);
        lastTimestampSamples = std::max(lastTimestampSamples, timestampSamples);

        PerformanceEvent ev;
        ev.timestampSamples = timestampSamples;
        ev.source = RecordingEventSource::playback;
        ev.message = midiMsg;
        events.push_back(std::move(ev));
    }

    DP_LOG_INFO("MidiFileImporter: track " + juce::String(selectedTrackIndex) + " collected " + juce::String(noteOnCount)
                + " note-on, " + juce::String(noteOffCount) + " note-off, "
                + juce::String(zeroVelocityNoteOnCount) + " zero-velocity note-on -> note-off");

    if (events.empty())
    {
        DP_LOG_ERROR("MidiFileImporter: no note events found in selected track " + juce::String(selectedTrackIndex));
        return std::nullopt;
    }

    RecordingTake take;
    take.sampleRate = targetSampleRate;
    take.lengthSamples = lastTimestampSamples;
    take.events = std::move(events);

    DP_LOG_INFO("MidiFileImporter: imported successfully, "
                + juce::String(static_cast<int>(take.events.size())) + " events, "
                + "duration = " + juce::String(take.durationSeconds(), 2) + " seconds");

    return take;
}

} // namespace devpiano::recording
