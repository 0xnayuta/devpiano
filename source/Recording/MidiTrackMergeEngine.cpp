#include "MidiTrackMergeEngine.h"
#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"
#include "Recording/MidiTextDecoder.h"
#include "Recording/TimelineValidation.h"
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace devpiano::recording {

// ============================================================================
// MidiKeySignature formatting
// ============================================================================
juce::String MidiKeySignature::toString() const {
    static const char* majorKeys[] = {
        "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", // -7 .. -1
        "C", // 0
        "G",  "D",  "A",  "E",  "B",  "F#", "C#" // +1 .. +7
    };
    static const char* minorKeys[] = {
        "Ab", "Eb", "Bb", "F",  "C",  "G",  "D", // -7 .. -1
        "A", // 0
        "E",  "B",  "F#", "C#", "G#", "D#", "A#" // +1 .. +7
    };

    const int clamped = juce::jlimit(-7, 7, sharpsOrFlats);
    const int index = clamped + 7;

    if (isMinor) {
        return juce::String(minorKeys[index]) + " minor";
    }
    return juce::String(majorKeys[index]) + " major";
}

// ============================================================================
// MidiFileMetadata formatting
// ============================================================================
juce::String MidiFileMetadata::formatSummary() const {
    juce::String summary;
    if (songTitle.isNotEmpty()) {
        summary += "Title: \"" + songTitle + "\", ";
    }
    summary += "Tracks: " + juce::String(static_cast<int>(tracks.size())) + ", ";
    summary += "Initial BPM: " + juce::String(initialBpm, 1);
    if (std::abs(maxBpm - minBpm) > 0.1) {
        summary += " (range " + juce::String(minBpm, 1) + "-" + juce::String(maxBpm, 1) + ")";
    }
    if (initialTimeSignature.has_value()) {
        summary += ", TimeSig: " + initialTimeSignature->toString();
    }
    if (initialKeySignature.has_value()) {
        summary += ", Key: " + initialKeySignature->toString();
    }
    return summary;
}

// ============================================================================
// Event priority
// ============================================================================
int MidiTrackMergeEngine::getMidiEventPriority(const juce::MidiMessage& message) noexcept {
    if (message.isProgramChange()) {
        return 1;
    }
    if (message.isController() || message.isPitchWheel()) {
        return 2;
    }
    if (message.isNoteOff(true) || (message.isNoteOn(true) && message.getVelocity() == 0)) {
        return 3;
    }
    if (message.isNoteOn(false)) {
        return 4;
    }
    return 5;
}

namespace {

struct TrackInspection {
    int trackIndex = -1;
    int noteCount = 0;
    juce::String trackName;
    juce::String textMeta;
    juce::String instrumentName;
    std::set<int> channelsPresent;
    int primaryChannel = 1;
};

void updateTrackInspectionFromMessage(TrackInspection& insp, const juce::MidiMessage& msg,
                                      std::map<int, int>& channelNoteHistogram) {
    if (msg.isMetaEvent()) {
        const auto metaType = msg.getMetaEventType();
        if (metaType == 3 && insp.trackName.isEmpty()) {
            insp.trackName = MidiTextDecoder::decodeTextMetaEvent(msg).trim();
        } else if (metaType == 1 && insp.textMeta.isEmpty()) {
            insp.textMeta = MidiTextDecoder::decodeTextMetaEvent(msg).trim();
        }
        return;
    }

    if (msg.isNoteOn(true) || msg.isNoteOff(true)) {
        ++insp.noteCount;
        if (msg.getChannel() > 0) {
            insp.channelsPresent.insert(msg.getChannel());
            ++channelNoteHistogram[msg.getChannel()];
        }
    }
}

TrackInspection inspectSingleTrack(int trackIndex, const juce::MidiMessageSequence* track) {
    TrackInspection insp;
    insp.trackIndex = trackIndex;
    if (track == nullptr) {
        return insp;
    }

    std::map<int, int> channelNoteHistogram;
    for (int i = 0; i < track->getNumEvents(); ++i) {
        if (const auto* eventPtr = track->getEventPointer(i)) {
            updateTrackInspectionFromMessage(insp, eventPtr->message, channelNoteHistogram);
        }
    }

    int maxChannelCount = 0;
    for (const auto& [ch, count] : channelNoteHistogram) {
        if (count > maxChannelCount) {
            maxChannelCount = count;
            insp.primaryChannel = ch;
        }
    }
    return insp;
}

std::vector<TrackInspection> inspectTracks(const juce::MidiFile& midiFile) {
    const auto numTracks = midiFile.getNumTracks();
    std::vector<TrackInspection> inspections;
    inspections.reserve(static_cast<std::size_t>(numTracks));

    for (int t = 0; t < numTracks; ++t) {
        inspections.push_back(inspectSingleTrack(t, midiFile.getTrack(t)));
    }

    return inspections;
}

} // namespace

namespace {

struct ChannelRemapPlan {
    bool remapChannels = false;
    std::vector<int> targetChannels; // 对应每个音轨的 target channel
};

ChannelRemapPlan computeChannelRemapPlan(const std::vector<TrackInspection>& trackInspections,
                                         MidiChannelMappingStrategy strategy) {
    int tracksWithNotes = 0;
    std::set<int> allDistinctChannels;
    for (const auto& insp : trackInspections) {
        if (insp.noteCount > 0) {
            ++tracksWithNotes;
            allDistinctChannels.insert(insp.channelsPresent.begin(), insp.channelsPresent.end());
        }
    }

    const bool shouldAutoAssign = (strategy == MidiChannelMappingStrategy::autoAssignIfSingleChannel
                                   && tracksWithNotes > 1 && allDistinctChannels.size() <= 1);
    const bool forceTrack = (strategy == MidiChannelMappingStrategy::forceTrackToChannel);
    const bool remap = shouldAutoAssign || forceTrack;

    if (remap) {
        DP_LOG_INFO("MidiTrackMergeEngine: channel remapping active (strategy="
                    + juce::String(static_cast<int>(strategy)) + ", tracksWithNotes=" + juce::String(tracksWithNotes)
                    + ", distinctChannels=" + juce::String(static_cast<int>(allDistinctChannels.size())) + ")");
    }

    ChannelRemapPlan plan;
    plan.remapChannels = remap;
    plan.targetChannels.reserve(trackInspections.size());
    for (size_t t = 0; t < trackInspections.size(); ++t) {
        const auto target = remap ? (static_cast<int>(t % 16) + 1) : trackInspections[t].primaryChannel;
        plan.targetChannels.push_back(target);
    }
    return plan;
}

juce::String extractSongTitle(const std::vector<TrackInspection>& inspections) {
    if (inspections.empty()) {
        return {};
    }
    if (!inspections[0].trackName.isEmpty()) {
        return inspections[0].trackName;
    }
    if (!inspections[0].textMeta.isEmpty()) {
        return inspections[0].textMeta;
    }
    for (const auto& insp : inspections) {
        if (!insp.trackName.isEmpty()) {
            return insp.trackName;
        }
        if (!insp.textMeta.isEmpty()) {
            return insp.textMeta;
        }
    }
    return {};
}

void parseMetaEventForGlobalMetadata(const juce::MidiMessage& midiMsg, double targetSampleRate,
                                     MidiFileMetadata& metadata) {
    const auto timestampSeconds = midiMsg.getTimeStamp();
    if (midiMsg.isTempoMetaEvent()) {
        const auto secondsPerQuarter = midiMsg.getTempoSecondsPerQuarterNote();
        if (secondsPerQuarter > 0.0) {
            const auto bpm = 60.0 / secondsPerQuarter;
            const auto tsSamplesOpt = checkedSampleCount(std::round(timestampSeconds * targetSampleRate));
            if (tsSamplesOpt.has_value() && isRepresentableTimelineLength(*tsSamplesOpt, targetSampleRate)) {
                MidiTempoEvent tempoEv;
                tempoEv.timestampSamples = *tsSamplesOpt;
                tempoEv.timestampSeconds = timestampSeconds;
                tempoEv.bpm = bpm;
                metadata.tempoMap.push_back(tempoEv);
            }
        }
    } else if (midiMsg.isTimeSignatureMetaEvent() && !metadata.initialTimeSignature.has_value()) {
        int num = 4;
        int denom = 4;
        midiMsg.getTimeSignatureInfo(num, denom);
        metadata.initialTimeSignature = MidiTimeSignature { num, denom };
    } else if (midiMsg.isKeySignatureMetaEvent() && !metadata.initialKeySignature.has_value()) {
        const auto sharpsFlats = midiMsg.getKeySignatureNumberOfSharpsOrFlats();
        const auto isMinor = !midiMsg.isKeySignatureMajorKey();
        metadata.initialKeySignature = MidiKeySignature { sharpsFlats, isMinor };
    }
}

bool validateMetaMessage(const juce::MidiMessage& msg) {
    if (!msg.isMetaEvent()) {
        return true;
    }

    const auto* raw = msg.getRawData();
    const auto rawSize = msg.getRawDataSize();
    if (raw == nullptr || rawSize < 2 || raw[0] != 0xFF) {
        DP_LOG_ERROR("MidiTrackMergeEngine: malformed meta event raw data");
        return false;
    }

    const int metaType = raw[1];

    // Parse bounded raw VLQ before framework accessors to reject truncated/unterminated lengths
    std::size_t offset = 2;
    uint32_t metaLen = 0;
    int lenBytes = 0;
    bool lenValid = false;
    for (int b = 0; b < 4 && offset + static_cast<std::size_t>(b) < static_cast<std::size_t>(rawSize); ++b) {
        const uint8_t byte = raw[offset + static_cast<std::size_t>(b)];
        metaLen = (metaLen << 7) | (byte & 0x7F);
        if (!(byte & 0x80)) {
            lenBytes = b + 1;
            lenValid = true;
            break;
        }
    }
    if (!lenValid) {
        DP_LOG_ERROR("MidiTrackMergeEngine: unterminated or malformed variable-length length in meta event");
        return false;
    }
    offset += static_cast<std::size_t>(lenBytes);
    if (offset + static_cast<std::size_t>(metaLen) > static_cast<std::size_t>(rawSize)) {
        DP_LOG_ERROR("MidiTrackMergeEngine: truncated meta event payload: declared " + juce::String(metaLen)
                     + " bytes, raw size only " + juce::String(rawSize));
        return false;
    }
    const uint8_t* d = raw + offset;

    switch (metaType) {
    case 0x58: { // Time Signature
        if (metaLen != 4) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x58 time signature meta event length: " + juce::String(metaLen)
                         + " (must be 4)");
            return false;
        }
        const int num = d[0];
        const int denomExp = d[1];
        if (num <= 0) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid time signature numerator: " + juce::String(num));
            return false;
        }
        if (denomExp > 30) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid time signature denominator exponent: " + juce::String(denomExp)
                         + " (must be 0..30)");
            return false;
        }
        return true;
    }
    case 0x51: { // Set Tempo
        if (metaLen != 3) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x51 tempo meta event length: " + juce::String(metaLen)
                         + " (must be 3)");
            return false;
        }
        const uint32_t us
            = (static_cast<uint32_t>(d[0]) << 16) | (static_cast<uint32_t>(d[1]) << 8) | static_cast<uint32_t>(d[2]);
        if (us == 0) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid tempo: 0 microseconds per quarter note");
            return false;
        }
        return true;
    }
    case 0x59: { // Key Signature
        if (metaLen != 2) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x59 key signature meta event length: " + juce::String(metaLen)
                         + " (must be 2)");
            return false;
        }
        const int8_t sf = static_cast<int8_t>(d[0]);
        const uint8_t mi = d[1];
        if (sf < -7 || sf > 7) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid key signature sharps/flats: " + juce::String(sf));
            return false;
        }
        if (mi > 1) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid key signature mode: " + juce::String(mi));
            return false;
        }
        return true;
    }
    case 0x20: { // Channel Prefix
        if (metaLen != 1) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x20 channel prefix length: " + juce::String(metaLen));
            return false;
        }
        return true;
    }
    case 0x21: { // MIDI Port
        if (metaLen != 1) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x21 port length: " + juce::String(metaLen));
            return false;
        }
        return true;
    }
    case 0x2F: { // End of Track
        if (metaLen != 0) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x2F end of track length: " + juce::String(metaLen));
            return false;
        }
        return true;
    }
    case 0x00: { // Sequence Number
        if (metaLen != 0 && metaLen != 2) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x00 sequence number length: " + juce::String(metaLen));
            return false;
        }
        return true;
    }
    case 0x54: { // SMPTE Offset
        if (metaLen != 5) {
            DP_LOG_ERROR("MidiTrackMergeEngine: invalid 0x54 SMPTE offset length: " + juce::String(metaLen));
            return false;
        }
        return true;
    }
    default:
        return true;
    }
}

bool validateMidiFileForMerge(const juce::MidiFile& midiFile, double targetSampleRate) {
    if (!isSupportedTimelineSampleRate(targetSampleRate)) {
        DP_LOG_ERROR("MidiTrackMergeEngine: unsupported target sample rate: " + juce::String(targetSampleRate));
        return false;
    }

    const auto numTracks = midiFile.getNumTracks();
    if (numTracks <= 0) {
        DP_LOG_ERROR("MidiTrackMergeEngine: no tracks in MIDI file");
        return false;
    }

    for (int t = 0; t < numTracks; ++t) {
        const auto* track = midiFile.getTrack(t);
        if (track == nullptr) {
            DP_LOG_ERROR("MidiTrackMergeEngine: null track sequence at index " + juce::String(t));
            return false;
        }

        for (int i = 0; i < track->getNumEvents(); ++i) {
            const auto* eventPtr = track->getEventPointer(i);
            if (eventPtr == nullptr) {
                DP_LOG_ERROR("MidiTrackMergeEngine: null event pointer in track " + juce::String(t));
                return false;
            }
            const auto& msg = eventPtr->message;
            const auto ts = msg.getTimeStamp();
            if (!std::isfinite(ts) || ts < 0.0) {
                DP_LOG_ERROR("MidiTrackMergeEngine: nonfinite or negative timestamp in track " + juce::String(t) + ": "
                             + juce::String(ts));
                return false;
            }
            const auto sampleOpt = checkedSampleCount(std::round(ts * targetSampleRate));
            if (!sampleOpt.has_value() || !isRepresentableTimelineLength(*sampleOpt, targetSampleRate)) {
                DP_LOG_ERROR("MidiTrackMergeEngine: unrepresentable timestamp in track " + juce::String(t) + ": "
                             + juce::String(ts) + "s");
                return false;
            }
            if (msg.isMetaEvent()) {
                if (!validateMetaMessage(msg)) {
                    return false;
                }
            }
        }
    }

    return true;
}

void extractGlobalMetadata(const juce::MidiFile& midiFile, double targetSampleRate, MidiFileMetadata& metadata) {
    const auto numTracks = midiFile.getNumTracks();
    for (int t = 0; t < numTracks; ++t) {
        const auto* track = midiFile.getTrack(t);
        if (track == nullptr) {
            continue;
        }

        for (int i = 0; i < track->getNumEvents(); ++i) {
            if (const auto* eventPtr = track->getEventPointer(i)) {
                if (eventPtr->message.isMetaEvent()) {
                    parseMetaEventForGlobalMetadata(eventPtr->message, targetSampleRate, metadata);
                }
            }
        }
    }

    if (!metadata.tempoMap.empty()) {
        std::ranges::sort(metadata.tempoMap, [](const MidiTempoEvent& a, const MidiTempoEvent& b) noexcept {
            return a.timestampSamples < b.timestampSamples;
        });

        metadata.initialBpm = metadata.tempoMap.front().bpm;
        metadata.minBpm = metadata.tempoMap.front().bpm;
        metadata.maxBpm = metadata.tempoMap.front().bpm;

        for (const auto& tempo : metadata.tempoMap) {
            metadata.minBpm = std::min(metadata.minBpm, tempo.bpm);
            metadata.maxBpm = std::max(metadata.maxBpm, tempo.bpm);
        }
    }
}

struct TrackEventMergeContext {
    int trackIndex = 0;
    double targetSampleRate = 44100.0;
    const ChannelRemapPlan& remapPlan;
    std::vector<PerformanceEvent>& mergedEvents;
    MidiTrackMergeStats& stats;
    int64_t& maxTimestampSamples;
};

bool updateStatsForNonNoteMessage(const juce::MidiMessage& msg, MidiTrackMergeStats& stats) {
    if (msg.isController()) {
        ++stats.ccCount;
        return true;
    }
    if (msg.isPitchWheel()) {
        ++stats.pitchBendCount;
        return true;
    }
    if (msg.isProgramChange()) {
        ++stats.programChangeCount;
        return true;
    }
    ++stats.otherMetaEventCount;
    DP_TRACE_MIDI(devpiano::diagnostics::describeMidiMessage(msg), "MidiTrackMergeEngine");
    return false;
}

bool processTrackEvent(juce::MidiMessage midiMsg, TrackEventMergeContext& ctx) {
    const auto timestampSeconds = midiMsg.getTimeStamp();
    if (!std::isfinite(timestampSeconds) || timestampSeconds < 0.0) {
        return false;
    }

    const auto sampleCountOpt = checkedSampleCount(std::round(timestampSeconds * ctx.targetSampleRate));
    if (!sampleCountOpt.has_value() || !isRepresentableTimelineLength(*sampleCountOpt, ctx.targetSampleRate)) {
        return false;
    }
    const auto timestampSamples = *sampleCountOpt;

    if (midiMsg.isMetaEvent()) {
        ++ctx.stats.otherMetaEventCount;
        DP_TRACE_MIDI(devpiano::diagnostics::describeMidiMessage(midiMsg), "MidiTrackMergeEngine");
        return true;
    }

    const bool isRawNoteOn = midiMsg.isNoteOn(true);
    const bool isZeroVelocityNoteOn = isRawNoteOn && midiMsg.getVelocity() == 0;
    const bool isNoteOn = midiMsg.isNoteOn(false);
    const bool isNoteOff = midiMsg.isNoteOff(true);

    if (!isNoteOn && !isNoteOff) {
        if (!updateStatsForNonNoteMessage(midiMsg, ctx.stats)) {
            return true;
        }
    } else {
        if (isZeroVelocityNoteOn) {
            ++ctx.stats.zeroVelocityNoteOnCount;
        }
        if (isNoteOn) {
            ++ctx.stats.noteOnCount;
        } else {
            ++ctx.stats.noteOffCount;
        }
    }

    ctx.maxTimestampSamples = std::max(ctx.maxTimestampSamples, timestampSamples);

    if (ctx.remapPlan.remapChannels && midiMsg.getChannel() > 0) {
        const auto targetChannel = ctx.remapPlan.targetChannels[static_cast<size_t>(ctx.trackIndex)];
        midiMsg.setChannel(targetChannel);
    }

    PerformanceEvent ev;
    ev.timestampSamples = timestampSamples;
    ev.type = PerformanceEventType::midi;
    ev.source = RecordingEventSource::playback;
    ev.message = midiMsg;

    ctx.mergedEvents.push_back(std::move(ev));
    return true;
}

std::optional<std::vector<PerformanceEvent>> collectTrackEvents(const juce::MidiFile& midiFile, double targetSampleRate,
                                                                const ChannelRemapPlan& remapPlan,
                                                                MidiTrackMergeStats& stats,
                                                                int64_t& maxTimestampSamples) {
    const auto numTracks = midiFile.getNumTracks();
    std::size_t totalEventEstimate = 0;
    for (int t = 0; t < numTracks; ++t) {
        if (const auto* track = midiFile.getTrack(t)) {
            totalEventEstimate += static_cast<std::size_t>(track->getNumEvents());
        }
    }

    std::vector<PerformanceEvent> mergedEvents;
    mergedEvents.reserve(totalEventEstimate);

    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex) {
        const auto* track = midiFile.getTrack(trackIndex);
        if (track == nullptr) {
            continue;
        }

        TrackEventMergeContext ctx {
            trackIndex, targetSampleRate, remapPlan, mergedEvents, stats, maxTimestampSamples
        };

        for (int i = 0; i < track->getNumEvents(); ++i) {
            if (const auto* eventPtr = track->getEventPointer(i)) {
                if (!processTrackEvent(eventPtr->message, ctx)) {
                    return std::nullopt;
                }
            }
        }
    }

    return mergedEvents;
}

} // namespace

std::optional<MidiTrackMergeResult> MidiTrackMergeEngine::mergeTracks(const juce::MidiFile& midiFile,
                                                                      double targetSampleRate,
                                                                      const MidiTrackMergeOptions& options) {
    if (!validateMidiFileForMerge(midiFile, targetSampleRate)) {
        return std::nullopt;
    }

    const auto numTracks = midiFile.getNumTracks();
    const auto trackInspections = inspectTracks(midiFile);
    const auto remapPlan = computeChannelRemapPlan(trackInspections, options.channelStrategy);

    MidiFileMetadata metadata;
    metadata.tracks.reserve(static_cast<std::size_t>(numTracks));
    for (int t = 0; t < numTracks; ++t) {
        const auto& insp = trackInspections[static_cast<std::size_t>(t)];
        MidiTrackInfo info;
        info.trackIndex = t;
        info.trackName = insp.trackName;
        info.noteCount = insp.noteCount;
        info.primaryChannel = insp.primaryChannel;
        info.assignedChannel = remapPlan.targetChannels[static_cast<size_t>(t)];
        metadata.tracks.push_back(std::move(info));
    }

    metadata.songTitle = extractSongTitle(trackInspections);
    extractGlobalMetadata(midiFile, targetSampleRate, metadata);

    MidiTrackMergeStats stats;
    stats.trackCount = numTracks;
    int64_t maxTimestampSamples = 0;

    auto mergedEventsOpt = collectTrackEvents(midiFile, targetSampleRate, remapPlan, stats, maxTimestampSamples);
    if (!mergedEventsOpt.has_value()) {
        DP_LOG_ERROR("MidiTrackMergeEngine: failed to collect track events due to invalid timestamp");
        return std::nullopt;
    }
    auto mergedEvents = std::move(*mergedEventsOpt);
    if (mergedEvents.empty()) {
        DP_LOG_ERROR("MidiTrackMergeEngine: no valid MIDI events found across processed tracks");
        return std::nullopt;
    }

    // Chronological stable sort with MIDI priority resolution for simultaneous events
    std::ranges::stable_sort(mergedEvents, [](const PerformanceEvent& a, const PerformanceEvent& b) noexcept {
        if (a.timestampSamples != b.timestampSamples) {
            return a.timestampSamples < b.timestampSamples;
        }
        return getMidiEventPriority(a.message) < getMidiEventPriority(b.message);
    });

    stats.maxTimestampSamples = maxTimestampSamples;
    stats.durationSeconds = static_cast<double>(maxTimestampSamples) / targetSampleRate;
    stats.mergedEventCount = static_cast<int>(mergedEvents.size());

    DP_LOG_INFO("MidiTrackMergeEngine: merged " + juce::String(stats.mergedEventCount) + " events from "
                + juce::String(numTracks) + " tracks (" + juce::String(stats.noteOnCount) + " note-on, "
                + juce::String(stats.noteOffCount) + " note-off, " + juce::String(stats.ccCount) + " CC, "
                + juce::String(stats.pitchBendCount) + " pitch-bend, " + juce::String(stats.programChangeCount)
                + " program-change), duration=" + juce::String(stats.durationSeconds, 2) + "s | "
                + metadata.formatSummary());

    RecordingTake take;
    take.sampleRate = targetSampleRate;
    take.lengthSamples = std::max<std::int64_t>(1, maxTimestampSamples);
    take.events = std::move(mergedEvents);

    return MidiTrackMergeResult { std::move(take), stats, std::move(metadata) };
}

} // namespace devpiano::recording
