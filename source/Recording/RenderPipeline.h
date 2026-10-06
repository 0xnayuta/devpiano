#pragma once

#include <array>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

#include "Audio/AcousticSnapshot.h"
#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PlaybackIdentityTracker.h"
#include "Audio/RoomReverbEngine.h"
#include "Export/WavExportOptions.h"
#include "Recording/RecordedPreset.h"
#include "Recording/RecordingEngine.h"

namespace devpiano::recording {

struct RenderEvent {
    PerformanceEventType type = PerformanceEventType::midi;
    std::uint32_t presetId = 0;
    juce::MidiMessage message;
    std::int64_t timestampSamples = 0;
};

struct RenderTimeline {
    std::vector<RenderEvent> events;
    std::vector<RecordedPreset> presets;
    std::int64_t takeLengthSamples = 0;
    std::int64_t totalSamples = 0;
};

[[nodiscard]] bool isAcousticSnapshotValid(const devpiano::audio::AcousticSnapshot& acoustic) noexcept;
[[nodiscard]] bool hasUsableRenderOptions(const devpiano::exporting::WavExportOptions& options) noexcept;

[[nodiscard]] std::optional<RenderTimeline> prepareRenderTimeline(const RecordingTake& take, double targetSampleRate,
                                                                  double tailSeconds);

void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) noexcept;

void applyAcousticSnapshotToBuiltin(devpiano::audio::BuiltinSynthesiser& pianoSynth,
                                    devpiano::audio::BuiltinSynthesiser& sineSynth,
                                    devpiano::audio::BuiltinSynthesiser*& activeSynth,
                                    devpiano::audio::RoomReverbEngine& roomReverb, float& currentMasterGain,
                                    const devpiano::audio::AcousticSnapshot& snapshot, bool applyPedalState = true);

void applyAcousticSnapshotToReverbAndGain(devpiano::audio::RoomReverbEngine& roomReverb, float& currentMasterGain,
                                          const devpiano::audio::AcousticSnapshot& snapshot);
} // namespace devpiano::recording
