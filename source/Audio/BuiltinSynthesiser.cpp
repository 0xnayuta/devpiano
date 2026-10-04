#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include <algorithm>
#include <cmath>

namespace devpiano::audio {

BuiltinSynthesiser::BuiltinSynthesiser() {
    setMinimumRenderingSubdivisionSize(1, true);
    for (std::size_t i = 0; i < 16; ++i) {
        lastPitchWheelValues[i] = 0x2000;
    }
}

void BuiltinSynthesiser::clearVoices() {
    allNotesOff(0, false);
    voices.clear();
}

int BuiltinSynthesiser::getNumVoices() const noexcept {
    return voices.size();
}

juce::SynthesiserVoice* BuiltinSynthesiser::getVoice(int index) const noexcept {
    return (index >= 0 && index < voices.size()) ? voices[index] : nullptr;
}

juce::SynthesiserVoice* BuiltinSynthesiser::addVoice(juce::SynthesiserVoice* newVoice) {
    if (newVoice != nullptr) {
        if (currentPlaybackSampleRate > 0.0) {
            newVoice->setCurrentPlaybackSampleRate(currentPlaybackSampleRate);
        }
        return voices.add(newVoice);
    }
    return nullptr;
}

void BuiltinSynthesiser::removeVoice(int index) {
    voices.remove(index);
}

void BuiltinSynthesiser::clearSounds() {
    sounds.clear();
}

int BuiltinSynthesiser::getNumSounds() const noexcept {
    return sounds.size();
}

juce::SynthesiserSound::Ptr BuiltinSynthesiser::getSound(int index) const noexcept {
    return (index >= 0 && index < sounds.size()) ? sounds[index] : nullptr;
}

juce::SynthesiserSound* BuiltinSynthesiser::addSound(const juce::SynthesiserSound::Ptr& newSound) {
    return sounds.add(newSound);
}

void BuiltinSynthesiser::removeSound(int index) {
    sounds.remove(index);
}

void BuiltinSynthesiser::setCurrentPlaybackSampleRate(double newRate) {
    if (std::abs(currentPlaybackSampleRate - newRate) > 1e-6) {
        allNotesOff(0, false);
        currentPlaybackSampleRate = newRate;
        for (auto* voice : voices) {
            if (voice != nullptr) {
                voice->setCurrentPlaybackSampleRate(newRate);
            }
        }
    }
}

double BuiltinSynthesiser::getSampleRate() const noexcept {
    return currentPlaybackSampleRate;
}

void BuiltinSynthesiser::setMinimumRenderingSubdivisionSize(int numSamples, bool shouldBeStrict) noexcept {
    jassert(numSamples > 0);
    minimumSubBlockSize = (numSamples > 0) ? numSamples : 1;
    subBlockSubdivisionIsStrict = shouldBeStrict;
}

void BuiltinSynthesiser::setNoteStealingEnabled(bool shouldSteal) noexcept {
    noteStealing = shouldSteal;
}

bool BuiltinSynthesiser::isNoteStealingEnabled() const noexcept {
    return noteStealing;
}

void BuiltinSynthesiser::renderNextBlock(juce::AudioBuffer<float>& outputAudio, const juce::MidiBuffer& inputMidi,
                                         int startSample, int numSamples) {
    processNextBlock(outputAudio, inputMidi, startSample, numSamples);
}

void BuiltinSynthesiser::renderNextBlock(juce::AudioBuffer<double>& outputAudio, const juce::MidiBuffer& inputMidi,
                                         int startSample, int numSamples) {
    processNextBlock(outputAudio, inputMidi, startSample, numSamples);
}

void BuiltinSynthesiser::renderVoices(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    for (auto* voice : voices) {
        if (voice != nullptr) {
            voice->renderNextBlock(buffer, startSample, numSamples);
        }
    }
}

void BuiltinSynthesiser::renderVoices(juce::AudioBuffer<double>& buffer, int startSample, int numSamples) {
    for (auto* voice : voices) {
        if (voice != nullptr) {
            voice->renderNextBlock(buffer, startSample, numSamples);
        }
    }
}

template <typename FloatType>
void BuiltinSynthesiser::processNextBlock(juce::AudioBuffer<FloatType>& outputAudio, const juce::MidiBuffer& midiData,
                                          int startSample, int numSamples) {
    if (numSamples <= 0) {
        for (auto it = midiData.findNextSamplePosition(startSample); it != midiData.cend(); ++it) {
            const auto metadata = *it;
            if (metadata.samplePosition > startSample) {
                break;
            }
            handleMidiMetadata(metadata);
        }
        return;
    }

    const int targetChannels = outputAudio.getNumChannels();
    const int blockEndSample = startSample + numSamples;
    auto midiIterator = midiData.findNextSamplePosition(startSample);
    bool firstEvent = true;

    for (; numSamples > 0; ++midiIterator) {
        if (midiIterator == midiData.cend()) {
            if (targetChannels > 0) {
                renderVoices(outputAudio, startSample, numSamples);
            }
            return;
        }

        const auto metadata = *midiIterator;
        const int samplesToNextMidiMessage = metadata.samplePosition - startSample;

        if (samplesToNextMidiMessage >= numSamples) {
            if (targetChannels > 0) {
                renderVoices(outputAudio, startSample, numSamples);
            }
            if (samplesToNextMidiMessage == numSamples) {
                handleMidiMetadata(metadata);
            }
            break;
        }

        if (samplesToNextMidiMessage < ((firstEvent && !subBlockSubdivisionIsStrict) ? 1 : minimumSubBlockSize)) {
            handleMidiMetadata(metadata);
            continue;
        }

        firstEvent = false;

        if (targetChannels > 0 && samplesToNextMidiMessage > 0) {
            renderVoices(outputAudio, startSample, samplesToNextMidiMessage);
        }

        handleMidiMetadata(metadata);
        startSample += samplesToNextMidiMessage;
        numSamples -= samplesToNextMidiMessage;
    }

    for (; midiIterator != midiData.cend(); ++midiIterator) {
        const auto metadata = *midiIterator;
        if (metadata.samplePosition > blockEndSample) {
            break;
        }
        handleMidiMetadata(metadata);
    }
}

void BuiltinSynthesiser::handleMidiMetadata(const juce::MidiMessageMetadata& metadata) noexcept {
    dispatchRawMidi(metadata.data, metadata.numBytes);
}

void BuiltinSynthesiser::handleMidiEvent(const juce::MidiMessage& m) {
    dispatchRawMidi(m.getRawData(), m.getRawDataSize());
}

void BuiltinSynthesiser::dispatchRawMidi(const uint8_t* data, int numBytes) noexcept {
    if (data == nullptr || numBytes < 1 || numBytes > 3) {
        return;
    }

    const uint8_t status = data[0];
    if (status < 0x80 || status >= 0xF0) {
        return;
    }

    const int channel = (status & 0x0F) + 1;
    const uint8_t messageType = status & 0xF0;

    switch (messageType) {
    case 0x80: {
        if (numBytes >= 3) {
            const int note = data[1] & 0x7F;
            const float velocity = static_cast<float>(data[2] & 0x7F) / 127.0f;
            noteOff(channel, note, velocity, true);
        }
        break;
    }
    case 0x90: {
        if (numBytes >= 3) {
            const int note = data[1] & 0x7F;
            const int velByte = data[2] & 0x7F;
            if (velByte == 0) {
                noteOff(channel, note, 0.0f, true);
            } else {
                const float velocity = static_cast<float>(velByte) / 127.0f;
                noteOn(channel, note, velocity);
            }
        }
        break;
    }
    case 0xA0: {
        if (numBytes >= 3) {
            const int note = data[1] & 0x7F;
            const int pressure = data[2] & 0x7F;
            handleAftertouch(channel, note, pressure);
        }
        break;
    }
    case 0xB0: {
        if (numBytes >= 3) {
            const int controllerNumber = data[1] & 0x7F;
            const int controllerValue = data[2] & 0x7F;
            handleController(channel, controllerNumber, controllerValue);
        }
        break;
    }
    case 0xC0: {
        if (numBytes >= 2) {
            const int programNumber = data[1] & 0x7F;
            handleProgramChange(channel, programNumber);
        }
        break;
    }
    case 0xD0: {
        if (numBytes >= 2) {
            const int pressure = data[1] & 0x7F;
            handleChannelPressure(channel, pressure);
        }
        break;
    }
    case 0xE0: {
        if (numBytes >= 3) {
            const int lsb = data[1] & 0x7F;
            const int msb = data[2] & 0x7F;
            const int wheelValue = (lsb & 0x7F) | ((msb & 0x7F) << 7);
            if (channel >= 1 && channel <= 16) {
                lastPitchWheelValues[channel - 1] = wheelValue;
            }
            handlePitchWheel(channel, wheelValue);
        }
        break;
    }
    default:
        break;
    }
}

void BuiltinSynthesiser::noteOn(int midiChannel, int midiNoteNumber, float velocity) {
    if (midiChannel < 1 || midiChannel > 16) {
        return;
    }
    if (velocity <= 0.0f) {
        noteOff(midiChannel, midiNoteNumber, 0.0f, true);
        return;
    }
    for (auto* sound : sounds) {
        if (sound != nullptr && sound->appliesToNote(midiNoteNumber) && sound->appliesToChannel(midiChannel)) {
            for (auto* voice : voices) {
                if (voice != nullptr && voice->getCurrentlyPlayingNote() == midiNoteNumber
                    && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
                    stopVoice(voice, 1.0f, true);
                }
            }

            auto* voice = findFreeVoice(sound, midiChannel, midiNoteNumber, noteStealing);
            if (voice != nullptr) {
                applySoftPedalToVoice(voice, midiChannel);
                startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
                voice->setSustainPedalDown(sustainPedalByChannel[static_cast<std::size_t>(midiChannel)]);
            }
        }
    }
}

void BuiltinSynthesiser::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) {
    for (auto* voice : voices) {
        if (voice != nullptr && voice->getCurrentlyPlayingNote() == midiNoteNumber
            && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
            voice->setKeyDown(false);
            if (!voice->isSustainPedalDown() && !voice->isSostenutoPedalDown()) {
                stopVoice(voice, velocity, allowTailOff);
            }
        }
    }
}

void BuiltinSynthesiser::allNotesOff(int midiChannel, bool allowTailOff) {
    for (auto* voice : voices) {
        if (voice != nullptr && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
            voice->setKeyDown(false);
            voice->setSustainPedalDown(false);
            voice->setSostenutoPedalDown(false);
            voice->stopNote(1.0f, allowTailOff);
        }
    }

    if (midiChannel <= 0) {
        sustainPedalByChannel.fill(false);
        sostenutoPedalByChannel.fill(false);
    } else if (midiChannel <= 16) {
        sustainPedalByChannel[static_cast<std::size_t>(midiChannel)] = false;
        sostenutoPedalByChannel[static_cast<std::size_t>(midiChannel)] = false;
    }
}

void BuiltinSynthesiser::handlePitchWheel(int midiChannel, int wheelValue) {
    if (midiChannel >= 1 && midiChannel <= 16) {
        lastPitchWheelValues[midiChannel - 1] = wheelValue;
    }
    for (auto* voice : voices) {
        if (voice != nullptr && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
            voice->pitchWheelMoved(wheelValue);
        }
    }
}

void BuiltinSynthesiser::handleController(int midiChannel, int controllerNumber, int controllerValue) {
    switch (controllerNumber) {
    case 64:
        handleSustainPedal(midiChannel, controllerValue >= 64);
        break;
    case 66:
        handleSostenutoPedal(midiChannel, controllerValue >= 64);
        break;
    case 67: {
        const bool isDown = (controllerValue >= 64);
        const float amount = isDown ? juce::jlimit(0.0f, 1.0f, static_cast<float>(controllerValue) / 127.0f) : 0.0f;
        setSoftPedalInternal(midiChannel, isDown, amount);
        for (auto* voice : voices) {
            if (voice != nullptr
                && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
                voice->controllerMoved(67, controllerValue);
            }
        }
        return;
    }
    case 120:
        allNotesOff(midiChannel, false);
        return;
    case 121: {
        if (midiChannel <= 0) {
            resetSoftPedalState();
        } else if (midiChannel <= 16) {
            softPedalByChannel[static_cast<std::size_t>(midiChannel)] = {};
        }
        for (auto* voice : voices) {
            if (voice != nullptr
                && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
                voice->controllerMoved(67, 0);
            }
        }
        break;
    }
    case 123:
        allNotesOff(midiChannel, true);
        return;
    default:
        break;
    }

    for (auto* voice : voices) {
        if (voice != nullptr && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
            voice->controllerMoved(controllerNumber, controllerValue);
        }
    }
}

void BuiltinSynthesiser::controller(int midiChannel, int controllerNumber, int controllerValue) {
    handleController(midiChannel, controllerNumber, controllerValue);
}

void BuiltinSynthesiser::handleSoftPedal(int midiChannel, bool isDown) {
    const float amount = isDown ? 1.0f : 0.0f;
    setSoftPedalInternal(midiChannel, isDown, amount);
    const int ccVal = isDown ? 127 : 0;
    for (auto* voice : voices) {
        if (voice != nullptr && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
            voice->controllerMoved(67, ccVal);
        }
    }
}

void BuiltinSynthesiser::handleSustainPedal(int midiChannel, bool isDown) {
    if (midiChannel <= 0) {
        for (std::size_t ch = 1; ch <= 16; ++ch) {
            handleSustainPedal(static_cast<int>(ch), isDown);
        }
        return;
    }

    if (midiChannel > 16) {
        return;
    }

    sustainPedalByChannel[static_cast<std::size_t>(midiChannel)] = isDown;

    if (isDown) {
        for (auto* voice : voices) {
            if (voice != nullptr && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)
                && voice->isKeyDown()) {
                voice->setSustainPedalDown(true);
            }
        }
    } else {
        for (auto* voice : voices) {
            if (voice != nullptr && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
                voice->setSustainPedalDown(false);
                if (!voice->isKeyDown() && !voice->isSostenutoPedalDown()) {
                    stopVoice(voice, 1.0f, true);
                }
            }
        }
    }
}

void BuiltinSynthesiser::handleSostenutoPedal(int midiChannel, bool isDown) {
    if (midiChannel <= 0) {
        for (std::size_t ch = 1; ch <= 16; ++ch) {
            handleSostenutoPedal(static_cast<int>(ch), isDown);
        }
        return;
    }

    if (midiChannel > 16) {
        return;
    }

    sostenutoPedalByChannel[static_cast<std::size_t>(midiChannel)] = isDown;

    for (auto* voice : voices) {
        if (voice != nullptr && voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel)) {
            if (isDown) {
                if (voice->isKeyDown()) {
                    voice->setSostenutoPedalDown(true);
                }
            } else {
                if (voice->isSostenutoPedalDown()) {
                    voice->setSostenutoPedalDown(false);
                    if (!voice->isKeyDown() && !voice->isSustainPedalDown()) {
                        stopVoice(voice, 1.0f, true);
                    }
                }
            }
        }
    }
}

void BuiltinSynthesiser::handleAftertouch(int midiChannel, int midiNoteNumber, int aftertouchValue) {
    for (auto* voice : voices) {
        if (voice != nullptr && voice->getCurrentlyPlayingNote() == midiNoteNumber
            && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
            voice->aftertouchChanged(aftertouchValue);
        }
    }
}

void BuiltinSynthesiser::handleChannelPressure(int midiChannel, int channelPressureValue) {
    for (auto* voice : voices) {
        if (voice != nullptr && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
            voice->channelPressureChanged(channelPressureValue);
        }
    }
}

void BuiltinSynthesiser::handleProgramChange([[maybe_unused]] int midiChannel, [[maybe_unused]] int programNumber) {
}

juce::SynthesiserVoice* BuiltinSynthesiser::findFreeVoice(juce::SynthesiserSound* soundToPlay, int midiChannel,
                                                          int midiNoteNumber, bool stealIfNoneAvailable) const {
    for (auto* voice : voices) {
        if (voice != nullptr && !voice->isVoiceActive() && voice->canPlaySound(soundToPlay)) {
            return voice;
        }
    }

    if (stealIfNoneAvailable) {
        return findVoiceToSteal(soundToPlay, midiChannel, midiNoteNumber);
    }

    return nullptr;
}

juce::SynthesiserVoice* BuiltinSynthesiser::findVoiceToSteal(juce::SynthesiserSound* soundToPlay, int /*midiChannel*/,
                                                             int midiNoteNumber) const {
    if (voices.isEmpty()) {
        return nullptr;
    }

    juce::SynthesiserVoice* low = nullptr;
    juce::SynthesiserVoice* top = nullptr;

    constexpr std::size_t kMaxCandidateVoices = 128;
    std::array<juce::SynthesiserVoice*, kMaxCandidateVoices> usableVoices {};
    std::size_t usableCount = 0;

    for (auto* voice : voices) {
        if (voice != nullptr && voice->canPlaySound(soundToPlay)) {
            if (usableCount < kMaxCandidateVoices) {
                usableVoices[usableCount++] = voice;
            }

            if (!voice->isPlayingButReleased()) {
                const auto note = voice->getCurrentlyPlayingNote();
                if (low == nullptr || note < low->getCurrentlyPlayingNote()) {
                    low = voice;
                }
                if (top == nullptr || note > top->getCurrentlyPlayingNote()) {
                    top = voice;
                }
            }
        }
    }

    if (usableCount == 0) {
        return nullptr;
    }

    std::sort(usableVoices.begin(), usableVoices.begin() + usableCount,
              [](const juce::SynthesiserVoice* a, const juce::SynthesiserVoice* b) noexcept {
                  return a->wasStartedBefore(*b);
              });

    if (top == low) {
        top = nullptr;
    }

    for (std::size_t i = 0; i < usableCount; ++i) {
        auto* voice = usableVoices[i];
        if (voice->getCurrentlyPlayingNote() == midiNoteNumber) {
            return voice;
        }
    }

    for (std::size_t i = 0; i < usableCount; ++i) {
        auto* voice = usableVoices[i];
        if (voice != low && voice != top && voice->isPlayingButReleased()) {
            return voice;
        }
    }

    for (std::size_t i = 0; i < usableCount; ++i) {
        auto* voice = usableVoices[i];
        if (voice != low && voice != top && !voice->isKeyDown()) {
            return voice;
        }
    }

    for (std::size_t i = 0; i < usableCount; ++i) {
        auto* voice = usableVoices[i];
        if (voice != low && voice != top) {
            return voice;
        }
    }

    if (top != nullptr) {
        return top;
    }

    return low;
}

void BuiltinSynthesiser::setSoftPedal(int midiChannel, bool isDown, float amount) noexcept {
    setSoftPedalInternal(midiChannel, isDown, amount);
    const auto ccVal = isDown ? juce::jlimit(64, 127, static_cast<int>(std::round(amount * 127.0f))) : 0;
    for (auto* voice : voices) {
        if (voice != nullptr && (midiChannel <= 0 || voice->juce::SynthesiserVoice::isPlayingChannel(midiChannel))) {
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

template void BuiltinSynthesiser::processNextBlock<float>(juce::AudioBuffer<float>&, const juce::MidiBuffer&, int, int);
template void BuiltinSynthesiser::processNextBlock<double>(juce::AudioBuffer<double>&, const juce::MidiBuffer&, int,
                                                           int);

} // namespace devpiano::audio
