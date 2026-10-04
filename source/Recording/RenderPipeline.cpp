#include "Recording/RenderPipeline.h"

#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Audio/TemperamentEngine.h"
#include "Recording/RecordingEngine.h"
#include "Recording/TimelineValidation.h"

#include <algorithm>
#include <cmath>

namespace devpiano::recording {

bool isAcousticSnapshotValid(const devpiano::audio::AcousticSnapshot& acoustic) noexcept {
    return std::isfinite(acoustic.masterGain) && std::isfinite(acoustic.referencePitchA4)
        && acoustic.referencePitchA4 > 0.0 && std::isfinite(acoustic.adsr.attack) && std::isfinite(acoustic.adsr.decay)
        && std::isfinite(acoustic.adsr.sustain) && std::isfinite(acoustic.adsr.release)
        && std::isfinite(acoustic.brightness) && std::isfinite(acoustic.hammerHardness)
        && std::isfinite(acoustic.resonance) && std::isfinite(acoustic.reverbWet)
        && std::isfinite(acoustic.pedalNoiseLevel) && std::isfinite(acoustic.feltAgeingAmount);
}

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

    for (const auto& preset : take.presets) {
        if (!isAcousticSnapshotValid(preset.acoustic)) {
            return std::nullopt;
        }
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
        if (event.type == PerformanceEventType::presetChange) {
            if (event.presetId >= take.presets.size()) {
                return std::nullopt;
            }
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
    timeline.presets = take.presets;
    timeline.events.reserve(take.events.size());
    for (const auto& event : take.events) {
        RenderEvent renderEvent;
        renderEvent.type = event.type;
        renderEvent.presetId = event.presetId;
        renderEvent.timestampSamples = *checkedScaleSamples(event.timestampSamples, ratio);
        if (event.type == PerformanceEventType::midi) {
            renderEvent.message = event.message;
            renderEvent.message.setTimeStamp(0.0);
        }
        timeline.events.push_back(std::move(renderEvent));
    }
    std::ranges::stable_sort(timeline.events, [](const auto& lhs, const auto& rhs) {
        if (lhs.timestampSamples != rhs.timestampSamples) {
            return lhs.timestampSamples < rhs.timestampSamples;
        }
        if (lhs.type != rhs.type) {
            return lhs.type == PerformanceEventType::presetChange;
        }
        return false;
    });
    return timeline;
}

void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) noexcept {
    for (auto channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
}
void applyAcousticSnapshotToBuiltin(devpiano::audio::BuiltinSynthesiser& pianoSynth,
                                    devpiano::audio::BuiltinSynthesiser& sineSynth,
                                    devpiano::audio::BuiltinSynthesiser*& activeSynth,
                                    devpiano::audio::RoomReverbEngine& roomReverb, float& currentMasterGain,
                                    const devpiano::audio::AcousticSnapshot& snapshot, bool applyPedalState) {
    auto* targetSynth = (snapshot.builtinTone == devpiano::core::BuiltinTone::sine) ? &sineSynth : &pianoSynth;
    if (activeSynth != targetSynth) {
        if (activeSynth != nullptr) {
            activeSynth->allNotesOff(0, false);
        }
        activeSynth = targetSynth;
    }

    const auto clampedBrightness = juce::jlimit(0.0f, 1.0f, snapshot.brightness);
    const auto clampedHardness = juce::jlimit(0.0f, 1.0f, snapshot.hammerHardness);
    const auto clampedResonance = juce::jlimit(0.0f, 1.0f, snapshot.resonance);
    const auto lidPos
        = static_cast<PianoSynthVoice::LidPosition>(juce::jlimit<std::uint8_t>(0, 2, snapshot.lidPosition));
    const auto refPitch = devpiano::audio::TemperamentEngine::clampReferencePitch(snapshot.referencePitchA4);
    const auto pedalNoise = juce::jlimit(0.0f, 1.0f, snapshot.pedalNoiseLevel);
    const auto feltAgeing = juce::jlimit(0.0f, 1.0f, snapshot.feltAgeingAmount);

    for (int i = 0; i < pianoSynth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<PianoSynthVoice*>(pianoSynth.getVoice(i))) {
            voice->setPianoParameters(clampedBrightness, clampedHardness, clampedResonance);
            voice->setLidPosition(lidPos);
            voice->setTemperament(snapshot.temperament);
            voice->setReferencePitchA4(refPitch);
            voice->setSoundPerspective(snapshot.soundPerspective);
            voice->setPedalNoiseLevel(pedalNoise);
            voice->setFeltAgeingAmount(feltAgeing);
            voice->setAdsrParameters(snapshot.adsr);
        }
    }
    if (applyPedalState) {
        pianoSynth.setSoftPedal(0, snapshot.unaCorda, snapshot.unaCorda ? 1.0f : 0.0f);
    }

    for (int i = 0; i < sineSynth.getNumVoices(); ++i) {
        if (auto* sineVoice = dynamic_cast<SineSynthVoice*>(sineSynth.getVoice(i))) {
            sineVoice->setTemperament(snapshot.temperament);
            sineVoice->setReferencePitchA4(refPitch);
            sineVoice->setAdsrParameters(snapshot.adsr);
        }
    }

    roomReverb.setSpace(snapshot.reverbSpace);
    roomReverb.setWetLevel(snapshot.reverbWet);

    currentMasterGain = juce::jlimit(0.0f, 1.0f, snapshot.masterGain);
}

void applyAcousticSnapshotToReverbAndGain(devpiano::audio::RoomReverbEngine& roomReverb, float& currentMasterGain,
                                          const devpiano::audio::AcousticSnapshot& snapshot) {
    roomReverb.setSpace(snapshot.reverbSpace);
    roomReverb.setWetLevel(snapshot.reverbWet);
    currentMasterGain = juce::jlimit(0.0f, 1.0f, snapshot.masterGain);
}

} // namespace devpiano::recording
