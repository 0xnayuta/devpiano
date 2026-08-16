#include "Recording/RenderPipeline.h"

#include "Recording/RecordingEngine.h"

#include <algorithm>
#include <cmath>

namespace devpiano::recording {

std::int64_t scaleTimestamp(std::int64_t timestampSamples, double ratio) noexcept {
    if (timestampSamples <= 0) {
        return 0;
    }

    return std::max<std::int64_t>(
        0, static_cast<std::int64_t>(std::llround(static_cast<double>(timestampSamples) * ratio)));
}

bool hasUsableRenderOptions(const devpiano::exporting::WavExportOptions& options) noexcept {
    return options.sampleRate > 0.0 && options.numChannels > 0 && options.blockSize > 0 && options.bitsPerSample > 0;
}

std::vector<RenderEvent> buildRenderEvents(const RecordingTake& take, double targetSampleRate) {
    const auto ratio = (take.sampleRate > 0.0 && targetSampleRate > 0.0) ? targetSampleRate / take.sampleRate : 1.0;

    std::vector<RenderEvent> events;
    events.reserve(take.events.size());

    for (const auto& perfEvent : take.events) {
        auto message = perfEvent.message;
        message.setTimeStamp(0.0);
        events.push_back({ message, scaleTimestamp(perfEvent.timestampSamples, ratio) });
    }

    std::ranges::stable_sort(
        events, [](const auto& lhs, const auto& rhs) { return lhs.timestampSamples < rhs.timestampSamples; });

    return events;
}

std::int64_t getScaledTakeLengthSamples(const RecordingTake& take, const std::vector<RenderEvent>& events,
                                        double targetSampleRate) noexcept {
    const auto ratio = (take.sampleRate > 0.0 && targetSampleRate > 0.0) ? targetSampleRate / take.sampleRate : 1.0;

    const auto scaledLengthFromTake = scaleTimestamp(take.lengthSamples, ratio);
    const auto lastEventEnd = events.empty() ? std::int64_t { 0 } : events.back().timestampSamples + 1;

    return std::max(scaledLengthFromTake, lastEventEnd);
}

void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) noexcept {
    for (auto channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
}

} // namespace devpiano::recording
