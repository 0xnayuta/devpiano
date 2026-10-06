#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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

    // Cold configuration and container lifecycle
    void clearVoices();
    [[nodiscard]] int getNumVoices() const noexcept;
    [[nodiscard]] juce::SynthesiserVoice* getVoice(int index) const noexcept;
    juce::SynthesiserVoice* addVoice(juce::SynthesiserVoice* newVoice);
    void removeVoice(int index);

    void clearSounds();
    [[nodiscard]] int getNumSounds() const noexcept;
    [[nodiscard]] juce::SynthesiserSound::Ptr getSound(int index) const noexcept;
    juce::SynthesiserSound* addSound(const juce::SynthesiserSound::Ptr& newSound);
    void removeSound(int index);

    void setCurrentPlaybackSampleRate(double newRate) override;
    [[nodiscard]] double getSampleRate() const noexcept;

    void setMinimumRenderingSubdivisionSize(int numSamples, bool shouldBeStrict = false) noexcept;
    void setNoteStealingEnabled(bool shouldSteal) noexcept;
    [[nodiscard]] bool isNoteStealingEnabled() const noexcept;

    // Audio-thread realtime rendering (lock-free)
    void renderNextBlock(juce::AudioBuffer<float>& outputAudio, const juce::MidiBuffer& inputMidi, int startSample,
                         int numSamples);
    void renderNextBlock(juce::AudioBuffer<double>& outputAudio, const juce::MidiBuffer& inputMidi, int startSample,
                         int numSamples);

    void renderVoices(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples) override;
    void renderVoices(juce::AudioBuffer<double>& outputAudio, int startSample, int numSamples) override;

    // Realtime MIDI events and voice dispatch (lock-free)
    void handleMidiEvent(const juce::MidiMessage& m) override;
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;
    void allNotesOff(int midiChannel, bool allowTailOff) override;

    void handlePitchWheel(int midiChannel, int wheelValue) override;
    void handleController(int midiChannel, int controllerNumber, int controllerValue) override;
    void controller(int midiChannel, int controllerNumber, int controllerValue);
    void handleSoftPedal(int midiChannel, bool isDown) override;
    void handleSustainPedal(int midiChannel, bool isDown) override;
    void handleSostenutoPedal(int midiChannel, bool isDown) override;
    void handleAftertouch(int midiChannel, int midiNoteNumber, int aftertouchValue) override;
    void handleChannelPressure(int midiChannel, int channelPressureValue) override;
    void handleProgramChange(int midiChannel, int programNumber) override;

    // Voice allocation / stealing heuristics (lock-free, zero allocation)
    [[nodiscard]] juce::SynthesiserVoice* findFreeVoice(juce::SynthesiserSound* soundToPlay, int midiChannel,
                                                        int midiNoteNumber, bool stealIfNoneAvailable) const override;
    [[nodiscard]] juce::SynthesiserVoice* findVoiceToSteal(juce::SynthesiserSound* soundToPlay, int midiChannel,
                                                           int midiNoteNumber) const override;

    // Soft pedal state inspection and explicit control
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

    [[nodiscard]] bool isSustainPedalDown(int midiChannel = 1) const noexcept {
        const auto ch = (midiChannel >= 1 && midiChannel <= 16) ? static_cast<std::size_t>(midiChannel) : 1;
        return sustainPedalByChannel[ch];
    }

    [[nodiscard]] bool isSostenutoPedalDown(int midiChannel = 1) const noexcept {
        const auto ch = (midiChannel >= 1 && midiChannel <= 16) ? static_cast<std::size_t>(midiChannel) : 1;
        return sostenutoPedalByChannel[ch];
    }

private:
    template <typename FloatType>
    void processNextBlock(juce::AudioBuffer<FloatType>& outputAudio, const juce::MidiBuffer& midiData, int startSample,
                          int numSamples);

    void handleMidiMetadata(const juce::MidiMessageMetadata& metadata) noexcept;
    void dispatchRawMidi(const uint8_t* data, int numBytes) noexcept;

    void setSoftPedalInternal(int midiChannel, bool isDown, float amount) noexcept;
    void applySoftPedalToVoice(juce::SynthesiserVoice* voice, int midiChannel) noexcept;

    double currentPlaybackSampleRate = 0.0;
    int minimumSubBlockSize = 1;
    bool subBlockSubdivisionIsStrict = true;
    bool noteStealing = true;

    std::array<SoftPedalState, 17> softPedalByChannel {};
    std::array<bool, 17> sustainPedalByChannel {};
    std::array<bool, 17> sostenutoPedalByChannel {};
};

} // namespace devpiano::audio
