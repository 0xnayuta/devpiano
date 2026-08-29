#include <functional>

#include "Recording/WavFileExporter.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Diagnostics/Log.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace devpiano::exporting {
namespace {
constexpr auto fallbackVoiceCount = 8;
constexpr auto wavTailSeconds = 2.0;

using devpiano::recording::addPanicMidi;
using devpiano::recording::buildRenderEvents;
using devpiano::recording::getScaledTakeLengthSamples;
using devpiano::recording::hasUsableRenderOptions;
using devpiano::recording::RenderEvent;
using devpiano::recording::scaleTimestamp;

void initialiseOfflineSynth(juce::Synthesiser& synth, const devpiano::exporting::WavExportOptions& options) {
    synth.clearSounds();
    synth.clearVoices();

    if (options.builtinTone == SettingsModel::BuiltinTone::piano) {
        synth.addSound(new PianoSynthSound());
        for (auto index = 0; index < fallbackVoiceCount; ++index) {
            auto* voice = new PianoSynthVoice();
            voice->setAdsrParameters(options.adsr);
            voice->setPianoParameters(options.pianoBrightness, options.pianoHammerHardness, options.pianoResonance);
            synth.addVoice(voice);
        }
    } else {
        synth.addSound(new SineSynthSound());
        for (auto index = 0; index < fallbackVoiceCount; ++index) {
            auto* voice = new SineSynthVoice();
            voice->setAdsrParameters(options.adsr);
            synth.addVoice(voice);
        }
    }

    synth.setCurrentPlaybackSampleRate(options.sampleRate);
}
} // namespace

bool exportTakeAsWavFile(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                         const WavExportOptions& options, const std::function<bool(double)>& progressCallback) {
    if (take.isEmpty() || take.sampleRate <= 0.0 || !hasUsableRenderOptions(options)
        || destinationFile == juce::File()) {
        DP_LOG_ERROR("[Export] WAV export rejected: empty take / invalid sample rate / unusable options / no file");
        return false;
    }

    auto parentDirectory = destinationFile.getParentDirectory();
    if (!parentDirectory.exists() && !parentDirectory.createDirectory()) {
        DP_LOG_ERROR("[Export] WAV export failed: cannot create directory " + parentDirectory.getFullPathName());
        return false;
    }

    auto fileStream = std::make_unique<juce::FileOutputStream>(destinationFile);
    if (!fileStream->openedOk()) {
        DP_LOG_ERROR("[Export] WAV export failed: cannot open output file " + destinationFile.getFullPathName());
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
        DP_LOG_ERROR("[Export] WAV export failed: WAV writer creation failed for " + destinationFile.getFullPathName());
        return false;
    }

    juce::Synthesiser synth;
    initialiseOfflineSynth(synth, options);
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

        // Master bus soft-knee ceiling guard (consistent with real-time AudioEngine)
        constexpr float kThreshold = 0.85f;
        constexpr float kCeiling = 0.98f;
        constexpr float kKnee = kCeiling - kThreshold;

        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch) {
            auto* data = audioBuffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                const auto x = data[i];
                const auto absX = std::abs(x);
                if (absX > kThreshold) {
                    const auto sign = (x >= 0.0f) ? 1.0f : -1.0f;
                    data[i] = sign * (kThreshold + kKnee * std::tanh((absX - kThreshold) / kKnee));
                }
            }
        }
        if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, numSamples)) {
            DP_LOG_ERROR("[Export] WAV export failed while writing: " + destinationFile.getFullPathName());
            return false;
        }
    }

    return true;
}

} // namespace devpiano::exporting
