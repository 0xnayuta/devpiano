#include "MidiTrackMergeEngine.h"
#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"
#include "Recording/RenderPipeline.h"
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
            insp.trackName = msg.getTextFromTextMetaEvent().trim();
        } else if (metaType == 1 && insp.textMeta.isEmpty()) {
            insp.textMeta = msg.getTextFromTextMetaEvent().trim();
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
            const auto tsSamples = std::max<int64_t>(
                0, static_cast<int64_t>(std::round(std::max(0.0, timestampSeconds) * targetSampleRate)));

            MidiTempoEvent tempoEv;
            tempoEv.timestampSamples = tsSamples;
            tempoEv.timestampSeconds = std::max(0.0, timestampSeconds);
            tempoEv.bpm = bpm;
            metadata.tempoMap.push_back(tempoEv);
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

void processTrackEvent(juce::MidiMessage midiMsg, TrackEventMergeContext& ctx) {
    const auto timestampSeconds = midiMsg.getTimeStamp();
    if (timestampSeconds < 0.0) {
        return;
    }

    if (midiMsg.isMetaEvent()) {
        ++ctx.stats.otherMetaEventCount;
        DP_TRACE_MIDI(devpiano::diagnostics::describeMidiMessage(midiMsg), "MidiTrackMergeEngine");
        return;
    }

    const bool isRawNoteOn = midiMsg.isNoteOn(true);
    const bool isZeroVelocityNoteOn = isRawNoteOn && midiMsg.getVelocity() == 0;
    const bool isNoteOn = midiMsg.isNoteOn(false);
    const bool isNoteOff = midiMsg.isNoteOff(true);

    if (!isNoteOn && !isNoteOff) {
        if (!updateStatsForNonNoteMessage(midiMsg, ctx.stats)) {
            return;
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

    const auto timestampSamples = clampToInt64(timestampSeconds * ctx.targetSampleRate);
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
}

std::vector<PerformanceEvent> collectTrackEvents(const juce::MidiFile& midiFile, double targetSampleRate,
                                                 const ChannelRemapPlan& remapPlan, MidiTrackMergeStats& stats,
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
                processTrackEvent(eventPtr->message, ctx);
            }
        }
    }

    return mergedEvents;
}

} // namespace

std::optional<MidiTrackMergeResult> MidiTrackMergeEngine::mergeTracks(const juce::MidiFile& midiFile,
                                                                      double targetSampleRate,
                                                                      const MidiTrackMergeOptions& options) {
    const auto numTracks = midiFile.getNumTracks();
    if (numTracks <= 0 || targetSampleRate <= 0.0) {
        DP_LOG_ERROR("MidiTrackMergeEngine: invalid input — tracks=" + juce::String(numTracks)
                     + ", sampleRate=" + juce::String(targetSampleRate));
        return std::nullopt;
    }

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

    auto mergedEvents = collectTrackEvents(midiFile, targetSampleRate, remapPlan, stats, maxTimestampSamples);
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
