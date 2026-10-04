#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include <cmath>

namespace devpiano::audio {

BuiltinSynthesiser::BuiltinSynthesiser() {
    setMinimumRenderingSubdivisionSize(1, true);
}

void BuiltinSynthesiser::handleController(int midiChannel, int controllerNumber, int controllerValue) {
    if (controllerNumber == 67) {
        const bool isDown = (controllerValue >= 64);
        const float amount = isDown ? juce::jlimit(0.0f, 1.0f, static_cast<float>(controllerValue) / 127.0f) : 0.0f;
        setSoftPedalInternal(midiChannel, isDown, amount);
        const juce::ScopedLock sl(lock);
        for (auto* voice : voices) {
            if (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
                voice->controllerMoved(67, controllerValue);
            }
        }
        return;
    }
    if (controllerNumber == 121) {
        if (midiChannel <= 0) {
            resetSoftPedalState();
        } else if (midiChannel <= 16) {
            softPedalByChannel[static_cast<std::size_t>(midiChannel)] = {};
        }
        const juce::ScopedLock sl(lock);
        for (auto* voice : voices) {
            if (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
                voice->controllerMoved(67, 0);
            }
        }
    }

    juce::Synthesiser::handleController(midiChannel, controllerNumber, controllerValue);
}

void BuiltinSynthesiser::handleSoftPedal(int midiChannel, bool isDown) {
    const float amount = isDown ? 1.0f : 0.0f;
    setSoftPedalInternal(midiChannel, isDown, amount);
    juce::Synthesiser::handleSoftPedal(midiChannel, isDown);
}

void BuiltinSynthesiser::noteOn(int midiChannel, int midiNoteNumber, float velocity) {
    const juce::ScopedLock sl(lock);

    for (auto* sound : sounds) {
        if (sound->appliesToNote(midiNoteNumber) && sound->appliesToChannel(midiChannel)) {
            for (auto* voice : voices) {
                if (voice->getCurrentlyPlayingNote() == midiNoteNumber
                    && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
                    stopVoice(voice, 1.0f, true);
                }
            }

            auto* voice = findFreeVoice(sound, midiChannel, midiNoteNumber, isNoteStealingEnabled());
            if (voice != nullptr) {
                applySoftPedalToVoice(voice, midiChannel);
                startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
            }
        }
    }
}

void BuiltinSynthesiser::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) {
    const juce::ScopedLock sl(lock);
    for (auto* voice : voices) {
        if (voice->getCurrentlyPlayingNote() == midiNoteNumber
            && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
            voice->setKeyDown(false);
            if (!voice->isSustainPedalDown() && !voice->isSostenutoPedalDown()) {
                stopVoice(voice, velocity, allowTailOff);
            }
        }
    }
}

void BuiltinSynthesiser::setSoftPedal(int midiChannel, bool isDown, float amount) noexcept {
    setSoftPedalInternal(midiChannel, isDown, amount);
    const juce::ScopedLock sl(lock);
    const auto ccVal = isDown ? juce::jlimit(64, 127, static_cast<int>(std::round(amount * 127.0f))) : 0;
    for (auto* voice : voices) {
        if (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
            voice->controllerMoved(67, ccVal);
        }
    }
}

void BuiltinSynthesiser::resetSoftPedalState() noexcept {
    softPedalByChannel.fill({});
}

void BuiltinSynthesiser::setSoftPedalInternal(int midiChannel, bool isDown, float amount) noexcept {
    const auto clampedAmount = isDown ? juce::jlimit(0.0f, 1.0f, amount) : 0.0f;
    if (midiChannel <= 0) {
        for (std::size_t ch = 1; ch <= 16; ++ch) {
            softPedalByChannel[ch] = { isDown, clampedAmount };
        }
    } else if (midiChannel <= 16) {
        softPedalByChannel[static_cast<std::size_t>(midiChannel)] = { isDown, clampedAmount };
    }
}

void BuiltinSynthesiser::applySoftPedalToVoice(juce::SynthesiserVoice* voice, int midiChannel) noexcept {
    if (auto* pianoVoice = dynamic_cast<PianoSynthVoice*>(voice)) {
        const auto ch = (midiChannel >= 1 && midiChannel <= 16) ? static_cast<std::size_t>(midiChannel) : 1;
        const auto& state = softPedalByChannel[ch];
        pianoVoice->setSoftPedalDown(state.isDown, state.amount);
    }
}

} // namespace devpiano::audio
