#include "Recording/WavFileExporter.h"

#include "Recording/RecordingEngine.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace devpiano::exporting
{
namespace
{
constexpr auto fallbackVoiceCount = 8;
constexpr auto wavTailSeconds = 2.0;

class OfflineSineSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class OfflineSineVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<OfflineSineSound*>(sound) != nullptr;
    }

    void setAdsrParameters(const juce::ADSR::Parameters& parameters)
    {
        adsr.setParameters(parameters);
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        level = velocity * 0.2f;
        frequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        phase = 0.0;
        increment = static_cast<float>(juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / getSampleRate());

        adsr.setSampleRate(getSampleRate());
        adsr.noteOn();
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
            return;
        }

        adsr.reset();
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (!isVoiceActive())
            return;

        for (auto sample = 0; sample < numSamples; ++sample)
        {
            const auto envelope = adsr.getNextSample();
            if (envelope <= 0.0f && !adsr.isActive())
            {
                clearCurrentNote();
                break;
            }

            const auto value = static_cast<float>(std::sin(phase) * level * envelope);
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            const auto sampleIndex = startSample + sample;
            for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                outputBuffer.addSample(channel, sampleIndex, value);
        }
    }

private:
    double phase = 0.0;
    float increment = 0.0f;
    float frequency = 440.0f;
    float level = 0.0f;
    juce::ADSR adsr;
};

struct RenderEvent
{
    std::int64_t timestampSamples = 0;
    juce::MidiMessage message;
};

[[nodiscard]] bool hasUsableOptions(const WavExportOptions& options) noexcept
{
    return options.sampleRate > 0.0
        && options.numChannels > 0
        && options.blockSize > 0
        && options.bitsPerSample > 0;
}

void initialiseOfflineSynth(juce::Synthesiser& synth, double sampleRate, const juce::ADSR::Parameters& adsr)
{
    synth.clearSounds();
    synth.clearVoices();

    synth.addSound(new OfflineSineSound());
    for (auto index = 0; index < fallbackVoiceCount; ++index)
    {
        auto* voice = new OfflineSineVoice();
        voice->setAdsrParameters(adsr);
        synth.addVoice(voice);
    }

    synth.setCurrentPlaybackSampleRate(sampleRate);
}

[[nodiscard]] std::int64_t scaleTimestamp(std::int64_t timestampSamples, double ratio) noexcept
{
    if (timestampSamples <= 0)
        return 0;

    return std::max<std::int64_t>(0, static_cast<std::int64_t>(std::llround(static_cast<double>(timestampSamples) * ratio)));
}

[[nodiscard]] std::vector<RenderEvent> buildRenderEvents(const devpiano::recording::RecordingTake& take,
                                                         double targetSampleRate)
{
    const auto ratio = (take.sampleRate > 0.0 && targetSampleRate > 0.0)
                         ? targetSampleRate / take.sampleRate
                         : 1.0;

    std::vector<RenderEvent> events;
    events.reserve(take.events.size());

    for (const auto& event : take.events)
    {
        auto message = event.message;
        message.setTimeStamp(0.0);
        events.push_back({ scaleTimestamp(event.timestampSamples, ratio), message });
    }

    std::stable_sort(events.begin(), events.end(), [](const auto& lhs, const auto& rhs)
    {
        return lhs.timestampSamples < rhs.timestampSamples;
    });

    return events;
}

[[nodiscard]] std::int64_t getScaledTakeLengthSamples(const devpiano::recording::RecordingTake& take,
                                                      const std::vector<RenderEvent>& events,
                                                      double targetSampleRate) noexcept
{
    const auto ratio = (take.sampleRate > 0.0 && targetSampleRate > 0.0)
                         ? targetSampleRate / take.sampleRate
                         : 1.0;

    auto length = scaleTimestamp(take.lengthSamples, ratio);
    for (const auto& event : events)
        length = std::max(length, event.timestampSamples);

    return length;
}

void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset)
{
    for (auto channel = 1; channel <= 16; ++channel)
    {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
}
} // namespace

bool exportTakeAsWavFile(const devpiano::recording::RecordingTake& take,
                         const juce::File& destinationFile,
                         const WavExportOptions& options)
{
    if (take.isEmpty() || take.sampleRate <= 0.0 || !hasUsableOptions(options) || destinationFile == juce::File())
        return false;

    auto parentDirectory = destinationFile.getParentDirectory();
    if (!parentDirectory.exists() && !parentDirectory.createDirectory())
        return false;

    auto fileStream = std::make_unique<juce::FileOutputStream>(destinationFile);
    if (!fileStream->openedOk())
        return false;

    std::unique_ptr<juce::OutputStream> outputStream = std::move(fileStream);

    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                           .withSampleRate(options.sampleRate)
                           .withNumChannels(options.numChannels)
                           .withBitsPerSample(options.bitsPerSample);

    auto writer = wavFormat.createWriterFor(outputStream, writerOptions);

    if (writer == nullptr)
        return false;

    juce::Synthesiser synth;
    initialiseOfflineSynth(synth, options.sampleRate, options.adsr);
    auto renderEvents = buildRenderEvents(take, options.sampleRate);
    const auto scaledTakeLength = getScaledTakeLengthSamples(take, renderEvents, options.sampleRate);
    const auto tailSamples = static_cast<std::int64_t>(std::ceil(wavTailSeconds * options.sampleRate));
    const auto totalSamples = std::max<std::int64_t>(1, scaledTakeLength + tailSamples);
    const auto gain = juce::jlimit(0.0f, 1.0f, options.masterGain);

    juce::AudioBuffer<float> audioBuffer(options.numChannels, options.blockSize);
    juce::MidiBuffer midiBuffer;
    midiBuffer.ensureSize(static_cast<size_t>(juce::jlimit(256, 65536, options.blockSize * 16)));

    std::size_t eventIndex = 0;
    auto panicSent = false;

    for (std::int64_t blockStart = 0; blockStart < totalSamples; blockStart += options.blockSize)
    {
        const auto numSamples = static_cast<int>(std::min<std::int64_t>(options.blockSize, totalSamples - blockStart));
        const auto blockEnd = blockStart + numSamples;

        audioBuffer.setSize(options.numChannels, numSamples, false, false, true);
        audioBuffer.clear();
        midiBuffer.clear();

        while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples < blockEnd)
        {
            const auto& event = renderEvents[eventIndex];
            if (event.timestampSamples >= blockStart)
            {
                const auto sampleOffset = static_cast<int>(event.timestampSamples - blockStart);
                midiBuffer.addEvent(event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
            }

            ++eventIndex;
        }

        if (!panicSent && scaledTakeLength >= blockStart && scaledTakeLength < blockEnd)
        {
            addPanicMidi(midiBuffer, juce::jlimit(0, numSamples - 1, static_cast<int>(scaledTakeLength - blockStart)));
            panicSent = true;
        }

        synth.renderNextBlock(audioBuffer, midiBuffer, 0, numSamples);
        audioBuffer.applyGain(gain);

        if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, numSamples))
            return false;
    }

    return true;
}

} // namespace devpiano::exporting
