#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

class PluginHost;

namespace devpiano::recording {
class RecordingEngine;
}

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine() = default;

    void setPluginHost(PluginHost* host) noexcept;
    void setRecordingEngine(devpiano::recording::RecordingEngine* engine) noexcept;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();
    void requestAllNotesOff() noexcept;
    void armPlaybackStartPreRoll(double sampleRate, int blockSize) noexcept;
    void sendController(int channel, int controllerType, int value);

    void setMasterGain(float newGain);
    void setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds);
    void setPianoParameters(float brightness, float hammerHardness, float resonance);
    enum class LidPosition : std::uint8_t {
        fullOpen = 0,
        halfStick = 1,
        closed = 2,
    };
    void setLidPosition(LidPosition position);
    [[nodiscard]] LidPosition getLidPosition() const noexcept {
        return pianoLidPosition;
    }
    void setPlaybackTranspose(bool enabled, int semitoneOffset,
                              std::uint16_t channelFollowKeyMask = 0b1111110111111111) noexcept;
    [[nodiscard]] bool isPlaybackTransposeEnabled() const noexcept;
    [[nodiscard]] int getPlaybackTransposeOffset() const noexcept;
    [[nodiscard]] std::uint16_t getPlaybackChannelFollowKeyMask() const noexcept;
    // sine 可切换回退。切换会重建 synth voice 注册；Synthesiser 内部锁（processNextBlock
    // 与 clearVoices/addVoice 共用）保护 voice 生命周期，消息线程调用安全，
    // 音频线程仅短暂阻塞等待当前块渲染完成。
    enum class BuiltinSynthTone {
        sine,
        piano,
    };
    void setBuiltinSynthTone(BuiltinSynthTone tone);
    [[nodiscard]] BuiltinSynthTone getBuiltinSynthTone() const noexcept {
        return builtinTone;
    }

    [[nodiscard]] PluginHost* getPluginHost() const noexcept {
        return pluginHost;
    }

    // Consume the count of pluginBuffer safety-net resizes that happened in
    // audio callbacks (framework contract violation; the callback itself only
    // increments an atomic — logging happens on the message thread, ERR-002).
    [[nodiscard]] int consumePluginBufferResizeCount() noexcept;

    // Block counts for the startup warmup and the playback-start pre-roll
    // silence windows.  Exposed as pure functions so unit tests can verify
    // the duration→block mapping (AUDIT TEST-009).
    [[nodiscard]] static int calculateWarmupBlockCount(double sampleRate, int blockSize) noexcept;
    [[nodiscard]] static int calculatePlaybackStartPreRollBlockCount(double sampleRate, int blockSize) noexcept;

    juce::MidiKeyboardState& getKeyboardState() noexcept {
        return keyboardState;
    }
    juce::MidiMessageCollector& getMidiCollector() noexcept {
        return midiCollector;
    }

private:
    void rebuildSynth();
    void updateAdsrOnVoices();
    void updatePianoParametersOnVoices();
    void discardWarmupInputState();
    bool consumeWarmupBlockIfNeeded();
    void injectPendingAllNotesOffIfNeeded();
    bool consumePlaybackStartPreRollBlockIfNeeded();
    void recordRealtimeMidiBufferIfNeeded(int numSamples);
    void renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples);

    PluginHost* pluginHost = nullptr;
    devpiano::recording::RecordingEngine* recordingEngine = nullptr;
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;
    juce::MidiBuffer playbackVisualMidiBuffer;
    juce::AudioBuffer<float> pluginBuffer;

    juce::ADSR::Parameters adsrParameters;
    std::atomic<float> masterGain { 1.0f };
    BuiltinSynthTone builtinTone = BuiltinSynthTone::piano;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    LidPosition pianoLidPosition = LidPosition::fullOpen;
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentBlockSize { 512 };
    std::atomic_bool allNotesOffPending { false };
    std::atomic<int> warmupBlocksRemaining { 0 };
    std::atomic<int> playbackStartPreRollBlocksRemaining { 0 };
    std::atomic<int> pluginBufferResizeCount { 0 };
    std::atomic<bool> playbackTransposeEnabled { false };
    std::atomic<int> playbackTransposeOffset { 0 };
    std::atomic<std::uint16_t> playbackChannelFollowKeyMask { 0b1111110111111111 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
