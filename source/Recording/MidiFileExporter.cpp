#include "MidiFileExporter.h"

#include "Recording/RecordingEngine.h"

namespace devpiano::exporting
{
namespace
{
constexpr int defaultTempoMicrosecondsPerQuarterNote = 500000; // 120 BPM

double convertSamplesToTicks(std::int64_t timestampSamples,
                             double sampleRate,
                             int ppq)
{
    if (timestampSamples <= 0 || sampleRate <= 0.0 || ppq <= 0)
        return 0.0;

    const auto seconds = static_cast<double>(timestampSamples) / sampleRate;
    const auto quartersPerSecond = 1.0 / (static_cast<double>(defaultTempoMicrosecondsPerQuarterNote) / 1'000'000.0);
    return seconds * quartersPerSecond * static_cast<double>(ppq);
}
} // namespace

bool exportTakeAsMidiFile(const devpiano::recording::RecordingTake& take,
                          const juce::File& destinationFile,
                          int ppq)
{
    if (take.isEmpty() || take.sampleRate <= 0.0 || ppq <= 0)
        return false;

    juce::MidiMessageSequence sequence;

    auto tempoMessage = juce::MidiMessage::tempoMetaEvent(defaultTempoMicrosecondsPerQuarterNote);
    tempoMessage.setTimeStamp(0.0);
    sequence.addEvent(tempoMessage);

    for (const auto& event : take.events)
    {
        auto message = event.message;
        message.setTimeStamp(convertSamplesToTicks(event.timestampSamples, take.sampleRate, ppq));
        sequence.addEvent(message);
    }

    sequence.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(ppq);
    midiFile.addTrack(sequence);

    juce::FileOutputStream outStream(destinationFile);
    if (outStream.openedOk())
        return midiFile.writeTo(outStream);

    return false;
}

}
