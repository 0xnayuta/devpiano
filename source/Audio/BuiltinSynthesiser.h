#pragma once

#include <array>
#include <cstddef>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

class BuiltinSynthesiser : public juce::Synthesiser {
public:
    struct SoftPedalState {
        bool isDown = false;
        float amount = 0.0f;
    };

    BuiltinSynthesiser();
    ~BuiltinSynthesiser() override = default;

    void handleController(int midiChannel, int controllerNumber, int controllerValue) override;
    void handleSoftPedal(int midiChannel, bool isDown) override;
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;

    [[nodiscard]] bool isSoftPedalDown(int midiChannel = 1) const noexcept {
        const auto ch = (midiChannel >= 1 && midiChannel <= 16) ? static_cast<std::size_t>(midiChannel) : 1;
        return softPedalByChannel[ch].isDown;
    }

    [[nodiscard]] float getSoftPedalAmount(int midiChannel = 1) const noexcept {
        const auto ch = (midiChannel >= 1 && midiChannel <= 16) ? static_cast<std::size_t>(midiChannel) : 1;
        return softPedalByChannel[ch].amount;
    }

    void setSoftPedal(int midiChannel, bool isDown, float amount = 1.0f) noexcept;
    void resetSoftPedalState() noexcept;

private:
    void setSoftPedalInternal(int midiChannel, bool isDown, float amount) noexcept;
    void applySoftPedalToVoice(juce::SynthesiserVoice* voice, int midiChannel) noexcept;

    std::array<SoftPedalState, 17> softPedalByChannel {};
};

} // namespace devpiano::audio
