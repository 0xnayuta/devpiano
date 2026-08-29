#include "MidiTrackMergeEngine.h"
#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"

#include <algorithm>
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

std::vector<TrackInspection> inspectTracks(const juce::MidiFile& midiFile) {
    const auto numTracks = midiFile.getNumTracks();
    std::vector<TrackInspection> inspections;
    inspections.reserve(static_cast<std::size_t>(numTracks));

    for (int t = 0; t < numTracks; ++t) {
        TrackInspection insp;
        insp.trackIndex = t;

        const auto* track = midiFile.getTrack(t);
        if (track == nullptr) {
            inspections.push_back(std::move(insp));
            continue;
        }

        std::map<int, int> channelNoteHistogram;

        for (int i = 0; i < track->getNumEvents(); ++i) {
            const auto* eventPtr = track->getEventPointer(i);
            if (eventPtr == nullptr) {
                continue;
            }

            const auto& msg = eventPtr->message;
            if (msg.isMetaEvent()) {
                const auto metaType = msg.getMetaEventType();
                if (metaType == 3 && insp.trackName.isEmpty()) {
                    insp.trackName = msg.getTextFromTextMetaEvent().trim();
                } else if (metaType == 1 && insp.textMeta.isEmpty()) {
                    insp.textMeta = msg.getTextFromTextMetaEvent().trim();
                }
            }

            if (msg.isNoteOn(true) || msg.isNoteOff(true)) {
                ++insp.noteCount;
                if (msg.getChannel() > 0) {
                    insp.channelsPresent.insert(msg.getChannel());
                    ++channelNoteHistogram[msg.getChannel()];
                }
            }
        }

        int maxChannelCount = 0;
        for (const auto& [ch, count] : channelNoteHistogram) {
            if (count > maxChannelCount) {
                maxChannelCount = count;
                insp.primaryChannel = ch;
            }
        }

        inspections.push_back(std::move(insp));
    }

    return inspections;
}

int findNoteRichTrackIndex(const std::vector<TrackInspection>& inspections) {
    int selectedTrack = -1;
    int maxNotes = 0;

    for (const auto& insp : inspections) {
        if (insp.noteCount > maxNotes) {
            maxNotes = insp.noteCount;
            selectedTrack = insp.trackIndex;
        }
    }

    return selectedTrack;
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

    // Determine whether single-track mode or multi-track merge is active
    std::vector<int> tracksToProcess;
    if (options.singleTrackOnly && numTracks > 1) {
        const auto noteRichTrack = findNoteRichTrackIndex(trackInspections);
        if (noteRichTrack < 0) {
            DP_LOG_ERROR("MidiTrackMergeEngine: no notes found in any track for single-track mode");
            return std::nullopt;
        }
        tracksToProcess.push_back(noteRichTrack);
        DP_LOG_INFO("MidiTrackMergeEngine: singleTrackOnly active, selected track " + juce::String(noteRichTrack));
    } else {
        tracksToProcess.resize(static_cast<std::size_t>(numTracks));
        for (int t = 0; t < numTracks; ++t) {
            tracksToProcess[static_cast<std::size_t>(t)] = t;
        }
    }

    // Analyse channel distribution across tracks to decide automatic channel remapping
    std::set<int> allDistinctChannels;
    int tracksWithNotes = 0;
    for (const auto& insp : trackInspections) {
        if (insp.noteCount > 0) {
            ++tracksWithNotes;
            allDistinctChannels.insert(insp.channelsPresent.begin(), insp.channelsPresent.end());
        }
    }

    const bool shouldAutoAssignChannels
        = (!options.singleTrackOnly && options.channelStrategy == MidiChannelMappingStrategy::autoAssignIfSingleChannel
           && tracksWithNotes > 1 && allDistinctChannels.size() <= 1);
    const bool forceTrackToChannel = (options.channelStrategy == MidiChannelMappingStrategy::forceTrackToChannel);
    const bool remapChannels = shouldAutoAssignChannels || forceTrackToChannel;

    if (remapChannels) {
        DP_LOG_INFO("MidiTrackMergeEngine: channel remapping active (strategy="
                    + juce::String(static_cast<int>(options.channelStrategy))
                    + ", tracksWithNotes=" + juce::String(tracksWithNotes)
                    + ", distinctChannels=" + juce::String(static_cast<int>(allDistinctChannels.size())) + ")");
    }

    MidiFileMetadata metadata;
    metadata.tracks.reserve(static_cast<std::size_t>(numTracks));

    // Build track metadata info list
    for (int t = 0; t < numTracks; ++t) {
        const auto& insp = trackInspections[static_cast<std::size_t>(t)];
        const auto targetChannel = remapChannels ? ((t % 16) + 1) : insp.primaryChannel;

        MidiTrackInfo info;
        info.trackIndex = t;
        info.trackName = insp.trackName;
        info.noteCount = insp.noteCount;
        info.primaryChannel = insp.primaryChannel;
        info.assignedChannel = targetChannel;

        metadata.tracks.push_back(std::move(info));
    }

    // Song title extraction heuristic (SMF Spec conformant):
    // 1. Prefer Track 0 Sequence/Track Name (Meta 3) or General Text (Meta 1)
    // 2. Otherwise fall back to first non-empty Track Name (Meta 3) or Text (Meta 1) in remaining tracks
    if (numTracks > 0 && !trackInspections[0].trackName.isEmpty()) {
        metadata.songTitle = trackInspections[0].trackName;
    } else if (numTracks > 0 && !trackInspections[0].textMeta.isEmpty()) {
        metadata.songTitle = trackInspections[0].textMeta;
    } else {
        for (const auto& insp : trackInspections) {
            if (!insp.trackName.isEmpty()) {
                metadata.songTitle = insp.trackName;
                break;
            }
            if (!insp.textMeta.isEmpty()) {
                metadata.songTitle = insp.textMeta;
                break;
            }
        }
    }

    MidiTrackMergeStats stats;
    stats.trackCount = numTracks;

    // Extract global metadata (Tempo Map, Time Signature, Key Signature) across ALL tracks.
    // This ensures Conductor track 0 metadata is preserved even in singleTrackOnly mode.
    for (int t = 0; t < numTracks; ++t) {
        const auto* track = midiFile.getTrack(t);
        if (track == nullptr) {
            continue;
        }

        for (int i = 0; i < track->getNumEvents(); ++i) {
            const auto* eventPtr = track->getEventPointer(i);
            if (eventPtr == nullptr) {
                continue;
            }

            const auto& midiMsg = eventPtr->message;
            if (!midiMsg.isMetaEvent()) {
                continue;
            }

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
                int num = 4, denom = 4;
                midiMsg.getTimeSignatureInfo(num, denom);
                metadata.initialTimeSignature = MidiTimeSignature { num, denom };
            } else if (midiMsg.isKeySignatureMetaEvent() && !metadata.initialKeySignature.has_value()) {
                const auto sharpsFlats = midiMsg.getKeySignatureNumberOfSharpsOrFlats();
                const auto isMinor = !midiMsg.isKeySignatureMajorKey();
                metadata.initialKeySignature = MidiKeySignature { sharpsFlats, isMinor };
            }
        }
    }

    std::vector<PerformanceEvent> mergedEvents;

    // Estimate reservation size
    std::size_t totalEventEstimate = 0;
    for (int t : tracksToProcess) {
        if (const auto* track = midiFile.getTrack(t)) {
            totalEventEstimate += static_cast<std::size_t>(track->getNumEvents());
        }
    }
    mergedEvents.reserve(totalEventEstimate);

    int64_t maxTimestampSamples = 0;

    for (int trackIndex : tracksToProcess) {
        const auto* track = midiFile.getTrack(trackIndex);
        if (track == nullptr) {
            continue;
        }

        const auto targetChannelForTrack = (trackIndex % 16) + 1; // 1-based MIDI channel

        for (int i = 0; i < track->getNumEvents(); ++i) {
            const auto* eventPtr = track->getEventPointer(i);
            if (eventPtr == nullptr) {
                continue;
            }

            auto midiMsg = eventPtr->message;
            const auto timestampSeconds = midiMsg.getTimeStamp();

            if (midiMsg.isMetaEvent()) {
                ++stats.otherMetaEventCount;
                DP_TRACE_MIDI(devpiano::diagnostics::describeMidiMessage(midiMsg), "MidiTrackMergeEngine");
                continue;
            }
            const bool isRawNoteOn = midiMsg.isNoteOn(true);
            const bool isZeroVelocityNoteOn = isRawNoteOn && midiMsg.getVelocity() == 0;
            const bool isNoteOn = midiMsg.isNoteOn(false);
            const bool isNoteOff = midiMsg.isNoteOff(true);

            if (!isNoteOn && !isNoteOff) {
                if (midiMsg.isController()) {
                    ++stats.ccCount;
                } else if (midiMsg.isPitchWheel()) {
                    ++stats.pitchBendCount;
                } else if (midiMsg.isProgramChange()) {
                    ++stats.programChangeCount;
                } else {
                    ++stats.otherMetaEventCount;
                    DP_TRACE_MIDI(devpiano::diagnostics::describeMidiMessage(midiMsg), "MidiTrackMergeEngine");
                    continue;
                }
            } else {
                if (isZeroVelocityNoteOn) {
                    ++stats.zeroVelocityNoteOnCount;
                }
                if (isNoteOn) {
                    ++stats.noteOnCount;
                } else {
                    ++stats.noteOffCount;
                }
            }

            if (timestampSeconds < 0.0) {
                continue;
            }

            const auto timestampSamples
                = std::max<int64_t>(0, static_cast<int64_t>(std::round(timestampSeconds * targetSampleRate)));
            maxTimestampSamples = std::max(maxTimestampSamples, timestampSamples);

            if (remapChannels && midiMsg.getChannel() > 0) {
                midiMsg.setChannel(targetChannelForTrack);
            }

            PerformanceEvent ev;
            ev.timestampSamples = timestampSamples;
            ev.type = PerformanceEventType::midi;
            ev.source = RecordingEventSource::playback;
            ev.message = midiMsg;

            mergedEvents.push_back(std::move(ev));
        }
    }

    // Process tempo map bounds
    if (!metadata.tempoMap.empty()) {
        std::sort(metadata.tempoMap.begin(), metadata.tempoMap.end(),
                  [](const MidiTempoEvent& a, const MidiTempoEvent& b) noexcept {
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

    if (mergedEvents.empty()) {
        DP_LOG_ERROR("MidiTrackMergeEngine: no valid MIDI events found across processed tracks");
        return std::nullopt;
    }

    // Chronological stable sort with MIDI priority resolution for simultaneous events
    std::stable_sort(mergedEvents.begin(), mergedEvents.end(),
                     [](const PerformanceEvent& a, const PerformanceEvent& b) noexcept {
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
    take.lengthSamples = maxTimestampSamples;
    take.events = std::move(mergedEvents);

    return MidiTrackMergeResult { std::move(take), stats, std::move(metadata) };
}

} // namespace devpiano::recording
