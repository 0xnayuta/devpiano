#include <functional>

#include "Audio/BuiltinSynthesiser.h"
#include "Audio/InstrumentNoteState.h"
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
using devpiano::recording::applyAcousticSnapshotToBuiltin;
using devpiano::recording::hasUsableRenderOptions;
using devpiano::recording::initialiseOfflineSynths;
using devpiano::recording::PerformanceEventType;
using devpiano::recording::prepareRenderTimeline;
} // namespace

bool exportTakeAsWavFile(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                         const WavExportOptions& options, const std::function<bool(double)>& progressCallback) {
    if (take.isEmpty() || take.sampleRate <= 0.0 || !hasUsableRenderOptions(options)
        || destinationFile == juce::File()) {
        DP_LOG_ERROR("[Export] WAV export rejected: empty take / invalid sample rate / unusable options / no file");
        return false;
    }

    const bool anyDualLayerRequiresPlugin = (options.layers.enabled && options.layers.pluginEnabled)
        || std::ranges::any_of(take.presets, [](const auto& p) {
                                                return p.acoustic.layers.enabled && p.acoustic.layers.pluginEnabled;
                                            });
    if (anyDualLayerRequiresPlugin) {
        DP_LOG_ERROR("[Export] WAV export rejected: dual layer requires hosted plugin, but exportTakeAsWavFile only "
                     "renders built-in synths");
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

    devpiano::audio::PlaybackIdentityTracker identityTracker;
    identityTracker.prepare(0, static_cast<std::size_t>(std::ranges::count_if(take.events, [](const auto& event) {
                                return event.type == PerformanceEventType::midi && event.message.isNoteOn();
                            })));

    devpiano::audio::BuiltinSynthesiser pianoSynth;
    devpiano::audio::BuiltinSynthesiser sineSynth;
    devpiano::audio::BuiltinSynthesiser* activeSynth = nullptr;
    devpiano::audio::RoomReverbEngine roomReverb;
    roomReverb.setSpace(options.reverbSpace);
    roomReverb.setWetLevel(options.reverbWet);
    roomReverb.prepare(options.sampleRate);

    initialiseOfflineSynths(pianoSynth, sineSynth, options);
    devpiano::audio::InstrumentLayers activeLayers = options.layers;
    if (activeLayers.enabled) {
        activeSynth = &pianoSynth;
    } else {
        activeSynth = (options.builtinTone == SettingsModel::BuiltinTone::sine) ? &sineSynth : &pianoSynth;
    }

    float currentMasterGain = juce::jlimit(0.0f, 1.0f, options.masterGain);
    bool hasSoftBaseline = false;
    bool softBaseline = false;

    juce::AudioBuffer<float> audioBuffer(options.numChannels, options.blockSize);
    juce::MidiBuffer segmentMidiBuffer;
    segmentMidiBuffer.ensureSize(
        static_cast<size_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(options.blockSize) * 16, 256, 65536)));
    juce::MidiBuffer instrumentMidiBuffer;
    instrumentMidiBuffer.ensureSize(
        static_cast<size_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(options.blockSize) * 16, 256, 65536)));
    devpiano::audio::InstrumentNoteState instrumentNotes;
    std::array<std::array<std::uint8_t, 3>, 16> pedalState {};
    bool previousDual = activeLayers.enabled;
    bool previousEnabled = !activeLayers.enabled || activeLayers.pianoEnabled;
    auto* previousSynth = activeSynth;

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

        for (int segStart = 0; segStart < numSamples;) {
            const auto segAbsStart = blockStart + segStart;
            segmentMidiBuffer.clear();

            while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples == segAbsStart
                   && renderEvents[eventIndex].type == PerformanceEventType::presetChange) {
                const auto& ev = renderEvents[eventIndex];
                if (ev.presetId < timeline->presets.size()) {
                    const auto& preset = timeline->presets[ev.presetId];
                    const auto applySoftBaseline = !hasSoftBaseline || softBaseline != preset.acoustic.unaCorda;
                    applyAcousticSnapshotToBuiltin(pianoSynth, sineSynth, activeSynth, roomReverb, currentMasterGain,
                                                   preset.acoustic, false);
                    if (applySoftBaseline) {
                        for (int channel = 1; channel <= 16; ++channel) {
                            segmentMidiBuffer.addEvent(
                                juce::MidiMessage::controllerEvent(channel, 67, preset.acoustic.unaCorda ? 127 : 0), 0);
                        }
                    }
                    hasSoftBaseline = true;
                    softBaseline = preset.acoustic.unaCorda;
                    activeLayers = preset.acoustic.layers;
                }
                ++eventIndex;
            }

            auto nextPresetOffset = numSamples;
            for (std::size_t scan = eventIndex; scan < renderEvents.size(); ++scan) {
                if (renderEvents[scan].timestampSamples >= blockEnd) {
                    break;
                }
                if (renderEvents[scan].type == PerformanceEventType::presetChange) {
                    nextPresetOffset = static_cast<int>(renderEvents[scan].timestampSamples - blockStart);
                    break;
                }
            }

            const auto segEnd = nextPresetOffset;
            const auto segLen = segEnd - segStart;
            const auto segAbsEnd = blockStart + segEnd;

            while (eventIndex < renderEvents.size() && renderEvents[eventIndex].timestampSamples < segAbsEnd) {
                const auto& event = renderEvents[eventIndex];
                if (event.type == PerformanceEventType::presetChange) {
                    break;
                }
                if (event.timestampSamples >= segAbsStart) {
                    const auto sampleOffset = static_cast<int>(event.timestampSamples - segAbsStart);
                    const auto clampedOffset = juce::jlimit(0, segLen - 1, sampleOffset);
                    const auto& msg = event.message;

                    if (msg.isNoteOn()) {
                        const auto ch = msg.getChannel();
                        const auto sourceNote = msg.getNoteNumber();
                        const auto finalOutputPitch = identityTracker.noteOn(ch, sourceNote, sourceNote);
                        if (finalOutputPitch.has_value()) {
                            segmentMidiBuffer.addEvent(juce::MidiMessage::noteOn(ch,
                                                                                 static_cast<int>(*finalOutputPitch),
                                                                                 msg.getFloatVelocity()),
                                                       clampedOffset);
                        }
                    } else if (msg.isNoteOff()) {
                        const auto ch = msg.getChannel();
                        const auto sourceNote = msg.getNoteNumber();
                        const auto result = identityTracker.noteOff(ch, sourceNote);
                        if (result.shouldEmit) {
                            segmentMidiBuffer.addEvent(
                                juce::MidiMessage::noteOff(ch, result.outputPitch, msg.getFloatVelocity()),
                                clampedOffset);
                        }
                    } else {
                        if (msg.isController()) {
                            const auto ctrl = msg.getControllerNumber();
                            if (ctrl == 120 || ctrl == 123) {
                                identityTracker.resetChannel(msg.getChannel());
                            }
                        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                            identityTracker.resetChannel(msg.getChannel());
                        }
                        segmentMidiBuffer.addEvent(msg, clampedOffset);
                    }
                }
                ++eventIndex;
            }

            if (!panicSent && scaledTakeLength >= segAbsStart && scaledTakeLength < segAbsEnd) {
                const auto offset = juce::jlimit(0, segLen - 1, static_cast<int>(scaledTakeLength - segAbsStart));
                addPanicMidi(segmentMidiBuffer, offset);
                identityTracker.resetOwnership();
                panicSent = true;
            }

            juce::AudioBuffer<float> segmentAudioBuffer(audioBuffer.getArrayOfWritePointers(), options.numChannels,
                                                        segStart, segLen);
            segmentAudioBuffer.clear();

            const auto enabled = !activeLayers.enabled || activeLayers.pianoEnabled;
            const auto transition = previousDual != activeLayers.enabled || previousSynth != activeSynth;
            instrumentMidiBuffer.clear();
            if (transition || (previousEnabled && !enabled)) {
                pianoSynth.allNotesOff(0, false);
                sineSynth.allNotesOff(0, false);
                instrumentNotes.reset();
            }
            if (enabled && (transition || !previousEnabled)) {
                for (std::size_t channel = 0; channel < 16; ++channel) {
                    for (std::size_t pedal = 0; pedal < 3; ++pedal) {
                        devpiano::audio::appendOrderedMidi(
                            instrumentMidiBuffer,
                            juce::MidiMessage::controllerEvent(static_cast<int>(channel) + 1,
                                                               pedal == 0 ? 64 : static_cast<int>(pedal) + 65,
                                                               pedalState[channel][pedal]),
                            0);
                    }
                }
            }
            previousDual = activeLayers.enabled;
            previousEnabled = enabled;
            previousSynth = activeSynth;
            for (const auto metadata : segmentMidiBuffer) {
                if (enabled) {
                    instrumentNotes.append(instrumentMidiBuffer, metadata, 1);
                }
                if (metadata.numBytes == 3 && (metadata.data[0] & 0xf0) == 0xb0) {
                    auto& state = pedalState[metadata.data[0] & 0x0f];
                    const auto controller = metadata.data[1];
                    if (controller == 64 || controller == 66 || controller == 67) {
                        state[controller == 64 ? 0 : controller - 65] = metadata.data[2];
                    } else if (controller == 121) {
                        state.fill(0);
                    }
                }
            }
            if (enabled) {
                activeSynth->renderNextBlock(segmentAudioBuffer, instrumentMidiBuffer, 0, segLen);
                if (activeLayers.enabled) {
                    segmentAudioBuffer.applyGain(juce::jlimit(0.0f, 1.0f, activeLayers.pianoGain));
                }
            }

            if (options.numChannels >= 2 && roomReverb.getWetLevel() > 1e-4f) {
                roomReverb.processStereo(segmentAudioBuffer.getWritePointer(0), segmentAudioBuffer.getWritePointer(1),
                                         segLen);
            }
            segmentAudioBuffer.applyGain(currentMasterGain);

            segStart = segEnd;
        }

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
