#include "Recording/RenderPipeline.h"

#include "Recording/RecordingEngine.h"
#include "Recording/TimelineValidation.h"

#include <algorithm>
#include <cmath>

namespace devpiano::recording {

bool hasUsableRenderOptions(const devpiano::exporting::WavExportOptions& options) noexcept {
    return isSupportedTimelineSampleRate(options.sampleRate) && options.numChannels > 0 && options.blockSize > 0
        && options.bitsPerSample > 0;
}

std::optional<RenderTimeline> prepareRenderTimeline(const RecordingTake& take, double targetSampleRate,
                                                    double tailSeconds) {
    if (!isSupportedTimelineSampleRate(take.sampleRate) || !isSupportedTimelineSampleRate(targetSampleRate)
        || take.lengthSamples < 0 || !std::isfinite(tailSeconds) || tailSeconds < 0.0) {
        return std::nullopt;
    }

    const auto ratio = targetSampleRate / take.sampleRate;
    const auto scaledLength = checkedScaleSamples(take.lengthSamples, ratio);
    const auto tailSamples = checkedSampleCount(std::ceil(tailSeconds * targetSampleRate));
    if (!scaledLength.has_value() || !tailSamples.has_value()) {
        return std::nullopt;
    }

    auto lastTimestamp = std::int64_t { -1 };
    for (const auto& event : take.events) {
        if (event.timestampSamples < 0 || event.timestampSamples > take.lengthSamples) {
            return std::nullopt;
        }
        lastTimestamp = std::max(lastTimestamp, event.timestampSamples);
    }
    auto takeLength = *scaledLength;
    if (lastTimestamp >= 0) {
        const auto scaled = checkedScaleSamples(lastTimestamp, ratio);
        const auto eventEnd = scaled.has_value() ? checkedAddSamples(*scaled, 1) : std::nullopt;
        if (!eventEnd.has_value()) {
            return std::nullopt;
        }
        takeLength = std::max(takeLength, *eventEnd);
    }

    const auto totalSamples = checkedAddSamples(takeLength, *tailSamples);
    if (!totalSamples.has_value()) {
        return std::nullopt;
    }

    RenderTimeline timeline;
    timeline.takeLengthSamples = takeLength;
    timeline.totalSamples = std::max<std::int64_t>(1, *totalSamples);
    timeline.events.reserve(take.events.size());
    for (const auto& event : take.events) {
        auto message = event.message;
        message.setTimeStamp(0.0);
        timeline.events.push_back({ std::move(message), *checkedScaleSamples(event.timestampSamples, ratio) });
    }
    std::ranges::stable_sort(
        timeline.events, [](const auto& lhs, const auto& rhs) { return lhs.timestampSamples < rhs.timestampSamples; });
    return timeline;
}

void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) noexcept {
    for (auto channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
}

} // namespace devpiano::recording
