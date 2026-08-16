#include <functional>

#include "Recording/WavFileExporter.h"

#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace devpiano::exporting {
namespace {
constexpr auto fallbackVoiceCount = 8;
constexpr auto wavTailSeconds = 2.0;

class OfflineSineSound final : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override {
        return true;
    }
    bool appliesToChannel(int) override {
        return true;
    }
};

class OfflineSineVoice final : public juce::SynthesiserVoice {
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<OfflineSineSound*>(sound) != nullptr;
    }

    void setAdsrParameters(const juce::ADSR::Parameters& parameters) {
        adsr.setParameters(parameters);
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        level = velocity * 0.2f;
        frequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        phase = 0.0;
        increment
            = static_cast<float>(juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / getSampleRate());

        adsr.setSampleRate(getSampleRate());
        adsr.noteOn();
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) {
            adsr.noteOff();
            return;
        }

        adsr.reset();
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {
    }
    void controllerMoved(int, int) override {
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        if (!isVoiceActive()) {
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto envelope = adsr.getNextSample();
            if (envelope <= 0.0f && !adsr.isActive()) {
                clearCurrentNote();
                break;
            }

            const auto value = static_cast<float>(std::sin(phase) * level * envelope);
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi) {
                phase -= juce::MathConstants<double>::twoPi;
            }

            const auto sampleIndex = startSample + sample;
            for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
                outputBuffer.addSample(channel, sampleIndex, value);
            }
        }
    }

private:
    double phase = 0.0;
    float increment = 0.0f;
    float frequency = 440.0f;
    float level = 0.0f;
    juce::ADSR adsr;
};

using devpiano::recording::addPanicMidi;
using devpiano::recording::buildRenderEvents;
using devpiano::recording::getScaledTakeLengthSamples;
using devpiano::recording::hasUsableRenderOptions;
using devpiano::recording::RenderEvent;
using devpiano::recording::scaleTimestamp;

void initialiseOfflineSynth(juce::Synthesiser& synth, double sampleRate, const juce::ADSR::Parameters& adsr) {
    synth.clearSounds();
    synth.clearVoices();

    synth.addSound(new OfflineSineSound());
    for (auto index = 0; index < fallbackVoiceCount; ++index) {
        auto* voice = new OfflineSineVoice();
        voice->setAdsrParameters(adsr);
        synth.addVoice(voice);
    }

    synth.setCurrentPlaybackSampleRate(sampleRate);
}
} // namespace

bool exportTakeAsWavFile(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                         const WavExportOptions& options, const std::function<bool(double)>& progressCallback) {
    if (take.isEmpty() || take.sampleRate <= 0.0 || !hasUsableRenderOptions(options)
        || destinationFile == juce::File()) {
        return false;
    }

    auto parentDirectory = destinationFile.getParentDirectory();
    if (!parentDirectory.exists() && !parentDirectory.createDirectory()) {
        return false;
    }

    auto fileStream = std::make_unique<juce::FileOutputStream>(destinationFile);
    if (!fileStream->openedOk()) {
        return false;
    }

    std::unique_ptr<juce::OutputStream> outputStream = std::move(fileStream);

    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate(options.sampleRate)
                             .withNumChannels(options.numChannels)
                             .withBitsPerSample(options.bitsPerSample);

    auto writer = wavFormat.createWriterFor(outputStream, writerOptions);

    if (writer == nullptr) {
        return false;
    }

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

    for (std::int64_t blockStart = 0; blockStart < totalSamples; blockStart += options.blockSize) {
        if (progressCallback
            && !progressCallback(static_cast<double>(blockStart) / static_cast<double>(totalSamples))) {
            return false;
        }

        const auto numSamples = static_cast<int>(std::min<std::int64_t>(options.blockSize, totalSamples - blockStart));
        const auto blockEnd = blockStart + numSamples;

        audioBuffer.setSize(options.numChannels, numSamples, false, false, true);
        audioBuffer.clear();
        midiBuffer.clear();

        while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples < blockEnd) {
            const auto& event = renderEvents[eventIndex];
            if (event.timestampSamples >= blockStart) {
                const auto sampleOffset = static_cast<int>(event.timestampSamples - blockStart);
                midiBuffer.addEvent(event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
            }

            ++eventIndex;
        }

        if (!panicSent && scaledTakeLength >= blockStart && scaledTakeLength < blockEnd) {
            addPanicMidi(midiBuffer, juce::jlimit(0, numSamples - 1, static_cast<int>(scaledTakeLength - blockStart)));
            panicSent = true;
        }

        synth.renderNextBlock(audioBuffer, midiBuffer, 0, numSamples);
        audioBuffer.applyGain(gain);

        if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, numSamples)) {
            return false;
        }
    }

    return true;
}

} // namespace devpiano::exporting
