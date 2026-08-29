#include "MidiTrackMergeEngine.h"
#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

namespace devpiano::recording {

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
    std::set<int> channelsPresent;
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

        for (int i = 0; i < track->getNumEvents(); ++i) {
            const auto* eventPtr = track->getEventPointer(i);
            if (eventPtr == nullptr) {
                continue;
            }

            const auto& msg = eventPtr->message;
            if (msg.isNoteOn(true) || msg.isNoteOff(true)) {
                ++insp.noteCount;
                if (msg.getChannel() > 0) {
                    insp.channelsPresent.insert(msg.getChannel());
                }
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
        = (options.channelStrategy == MidiChannelMappingStrategy::autoAssignIfSingleChannel && tracksWithNotes > 1
           && allDistinctChannels.size() <= 1);
    const bool forceTrackToChannel = (options.channelStrategy == MidiChannelMappingStrategy::forceTrackToChannel);
    const bool remapChannels = shouldAutoAssignChannels || forceTrackToChannel;

    if (remapChannels) {
        DP_LOG_INFO("MidiTrackMergeEngine: channel remapping active (strategy="
                    + juce::String(static_cast<int>(options.channelStrategy))
                    + ", tracksWithNotes=" + juce::String(tracksWithNotes)
                    + ", distinctChannels=" + juce::String(static_cast<int>(allDistinctChannels.size())) + ")");
    }

    MidiTrackMergeStats stats;
    stats.trackCount = numTracks;

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

            const auto timestampSeconds = midiMsg.getTimeStamp();
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
                + " program-change), duration=" + juce::String(stats.durationSeconds, 2) + "s");

    RecordingTake take;
    take.sampleRate = targetSampleRate;
    take.lengthSamples = maxTimestampSamples;
    take.events = std::move(mergedEvents);

    return MidiTrackMergeResult { std::move(take), stats };
}

} // namespace devpiano::recording
