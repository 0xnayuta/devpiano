#pragma once

#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

#include "Export/WavExportOptions.h"

namespace devpiano::recording {
struct RecordingTake;
}

namespace devpiano::recording {

struct RenderEvent {
    juce::MidiMessage message;
    std::int64_t timestampSamples = 0;
};
struct RenderTimeline {
    std::vector<RenderEvent> events;
    std::int64_t takeLengthSamples = 0;
    std::int64_t totalSamples = 0;
};

[[nodiscard]] bool hasUsableRenderOptions(const devpiano::exporting::WavExportOptions& options) noexcept;

[[nodiscard]] std::optional<RenderTimeline> prepareRenderTimeline(const RecordingTake& take, double targetSampleRate,
                                                                  double tailSeconds);

// 向 midiBuffer 注入全 16 通道 panic 控制器事件（CC64 延音 / CC120 全关 / all-notes-off）。
void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) noexcept;

} // namespace devpiano::recording
