#include "MidiFileExporter.h"

#include "Recording/RecordingEngine.h"
#include "Recording/TimelineValidation.h"

#include <cmath>
#include <limits>

namespace devpiano::exporting {
namespace {
constexpr int defaultTempoMicrosecondsPerQuarterNote = 500000; // 120 BPM

double convertSamplesToTicks(std::int64_t timestampSamples, double sampleRate, int ppq) {
    if (timestampSamples <= 0 || sampleRate <= 0.0 || ppq <= 0) {
        return 0.0;
    }

    const auto seconds = static_cast<double>(timestampSamples) / sampleRate;
    const auto quartersPerSecond = 1.0 / (static_cast<double>(defaultTempoMicrosecondsPerQuarterNote) / 1'000'000.0);
    return seconds * quartersPerSecond * static_cast<double>(ppq);
}
} // namespace

bool exportTakeAsMidiFile(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile, int ppq) {
    if (take.isEmpty() || !devpiano::recording::isSupportedTimelineSampleRate(take.sampleRate)
        || !devpiano::recording::isRepresentableTimelineLength(take.lengthSamples, take.sampleRate) || ppq <= 0
        || ppq > 32767 || destinationFile == juce::File()) {
        return false;
    }

    juce::MidiMessageSequence sequence;

    auto tempoMessage = juce::MidiMessage::tempoMetaEvent(defaultTempoMicrosecondsPerQuarterNote);
    tempoMessage.setTimeStamp(0.0);
    sequence.addEvent(tempoMessage);

    for (const auto& event : take.events) {
        if (event.type != devpiano::recording::PerformanceEventType::midi || event.message.getRawDataSize() == 0
            || event.message.isSysEx()) {
            continue;
        }

        if (event.timestampSamples < 0 || event.timestampSamples > take.lengthSamples) {
            return false;
        }
        auto message = event.message;
        message.setTimeStamp(convertSamplesToTicks(event.timestampSamples, take.sampleRate, ppq));
        sequence.addEvent(message);
    }
    auto previousTick = 0.0;
    for (int index = 0; index < sequence.getNumEvents(); ++index) {
        const auto tick = std::round(sequence.getEventPointer(index)->message.getTimeStamp());
        if (!std::isfinite(tick) || tick < previousTick || tick > std::numeric_limits<int>::max()
            || tick - previousTick > 0x0fffffff) {
            return false;
        }
        previousTick = tick;
    }

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(ppq);
    midiFile.addTrack(sequence);

    juce::TemporaryFile temporaryFile(destinationFile);
    {
        juce::FileOutputStream outStream(temporaryFile.getFile());
        if (!outStream.openedOk() || !midiFile.writeTo(outStream)) {
            return false;
        }
        outStream.flush();
        if (outStream.getStatus().failed()) {
            return false;
        }
    }

    return temporaryFile.overwriteTargetFileWithTemporary();
}

}
