#include <functional>

#include "Audio/PianoSynthVoice.h"
#include "Audio/RoomReverbEngine.h"
#include "Audio/SineSynthVoice.h"
#include "Diagnostics/Log.h"
#include "Export/ExportFlowSupport.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace devpiano::exporting {
namespace {
constexpr auto fallbackVoiceCount = 8;
constexpr auto wavTailSeconds = 2.0;

using devpiano::recording::addPanicMidi;
using devpiano::recording::hasUsableRenderOptions;
using devpiano::recording::prepareRenderTimeline;

void initialiseOfflineSynth(juce::Synthesiser& synth, const devpiano::exporting::WavExportOptions& options) {
    synth.clearSounds();
    synth.clearVoices();

    if (options.builtinTone == SettingsModel::BuiltinTone::piano) {
        synth.addSound(new PianoSynthSound());
        for (auto index = 0; index < fallbackVoiceCount; ++index) {
            auto* voice = new PianoSynthVoice();
            voice->setVoiceIndex(index);
            voice->setAdsrParameters(options.adsr);
            voice->setPianoParameters(options.pianoBrightness, options.pianoHammerHardness, options.pianoResonance);
            voice->setLidPosition(static_cast<PianoSynthVoice::LidPosition>(options.lidPosition));
            voice->setTemperament(options.temperament);
            voice->setReferencePitchA4(options.referencePitchA4);
            voice->setSoundPerspective(options.soundPerspective);
            voice->setPedalNoiseLevel(options.pedalNoiseLevel);
            voice->setFeltAgeingAmount(options.feltAgeingAmount);
            synth.addVoice(voice);
        }
    } else {
        synth.addSound(new SineSynthSound());
        for (auto index = 0; index < fallbackVoiceCount; ++index) {
            auto* voice = new SineSynthVoice();
            voice->setAdsrParameters(options.adsr);
            voice->setTemperament(options.temperament);
            voice->setReferencePitchA4(options.referencePitchA4);
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

    auto timeline = prepareRenderTimeline(take, options.sampleRate, wavTailSeconds);
    if (!timeline.has_value()) {
        DP_LOG_ERROR("[Export] WAV export rejected: invalid or unrepresentable timeline");
        return false;
    }
    const auto& renderEvents = timeline->events;
    const auto scaledTakeLength = timeline->takeLengthSamples;
    const auto totalSamples = timeline->totalSamples;

    auto parentDirectory = destinationFile.getParentDirectory();
    if (!parentDirectory.exists() && !parentDirectory.createDirectory()) {
        DP_LOG_ERROR("[Export] WAV export failed: cannot create directory " + parentDirectory.getFullPathName());
        return false;
    }

    juce::TemporaryFile temporaryFile(destinationFile);
    auto fileStream = std::make_unique<juce::FileOutputStream>(temporaryFile.getFile());
    if (!fileStream->openedOk()) {
        DP_LOG_ERROR("[Export] WAV export failed: cannot open output file " + destinationFile.getFullPathName());
        return false;
    }

    auto* const fileOutput = fileStream.get();
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
    devpiano::audio::RoomReverbEngine roomReverb;
    roomReverb.prepare(options.sampleRate);
    roomReverb.setSpace(options.reverbSpace);
    roomReverb.setWetLevel(options.reverbWet);
    initialiseOfflineSynth(synth, options);
    const auto gain = juce::jlimit(0.0f, 1.0f, options.masterGain);

    juce::AudioBuffer<float> audioBuffer(options.numChannels, options.blockSize);
    juce::MidiBuffer midiBuffer;
    midiBuffer.ensureSize(
        static_cast<size_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(options.blockSize) * 16, 256, 65536)));

    std::size_t eventIndex = 0;
    auto panicSent = false;

    for (std::int64_t blockStart = 0; blockStart < totalSamples;) {
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
        if (options.numChannels >= 2 && options.reverbWet > 1e-4f) {
            roomReverb.processStereo(audioBuffer.getWritePointer(0), audioBuffer.getWritePointer(1), numSamples);
        }
        audioBuffer.applyGain(gain);
        applyMasterSoftLimiter(audioBuffer, numSamples);
        if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, numSamples)) {
            DP_LOG_ERROR("[Export] WAV export failed while writing: " + destinationFile.getFullPathName());
            return false;
        }
        blockStart = blockEnd;
    }

    if (!writer->flush()) {
        DP_LOG_ERROR("[Export] WAV export failed while finalising: " + destinationFile.getFullPathName());
        return false;
    }
    fileOutput->flush();
    if (fileOutput->getStatus().failed()) {
        DP_LOG_ERROR("[Export] WAV export failed while flushing: " + destinationFile.getFullPathName());
        return false;
    }
    writer.reset();

    if (progressCallback && !progressCallback(1.0)) {
        return false;
    }
    if (!temporaryFile.overwriteTargetFileWithTemporary()) {
        DP_LOG_ERROR("[Export] WAV export failed while replacing: " + destinationFile.getFullPathName());
        return false;
    }

    return true;
}

} // namespace devpiano::exporting
