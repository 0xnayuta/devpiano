#pragma once

#include "Audio/BuiltinSynthesiser.h"
#include "Audio/MetronomeProcessor.h"
#include "Audio/SyncPedalProcessor.h"
#include "PerspectiveProcessor.h"
#include "RoomReverbEngine.h"
#include "TemperamentEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

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
        return static_cast<LidPosition>(pendingLidPosition.load(std::memory_order_relaxed));
    }
    using Temperament = devpiano::audio::Temperament;
    void setTemperament(Temperament temperament);
    [[nodiscard]] Temperament getTemperament() const noexcept {
        return static_cast<Temperament>(pendingTemperament.load(std::memory_order_relaxed));
    }
    void setReferencePitchA4(double pitch);
    [[nodiscard]] double getReferencePitchA4() const noexcept {
        return pendingReferencePitchA4.load(std::memory_order_relaxed);
    }
    using SoundPerspective = devpiano::audio::SoundPerspective;
    void setSoundPerspective(SoundPerspective perspective);
    [[nodiscard]] SoundPerspective getSoundPerspective() const noexcept {
        return static_cast<SoundPerspective>(pendingSoundPerspective.load(std::memory_order_relaxed));
    }
    using ReverbSpace = devpiano::audio::ReverbSpace;
    void setReverbSpace(ReverbSpace space);
    [[nodiscard]] ReverbSpace getReverbSpace() const noexcept {
        return static_cast<ReverbSpace>(pendingReverbSpace.load(std::memory_order_relaxed));
    }
    void setReverbWet(float wetLevel);
    [[nodiscard]] float getReverbWet() const noexcept {
        return pendingReverbWet.load(std::memory_order_relaxed);
    }
    void setPedalNoiseLevel(float level);
    [[nodiscard]] float getPedalNoiseLevel() const noexcept {
        return pendingPedalNoiseLevel.load(std::memory_order_relaxed);
    }
    void setFeltAgeingAmount(float amount);
    [[nodiscard]] float getFeltAgeingAmount() const noexcept {
        return pendingFeltAgeingAmount.load(std::memory_order_relaxed);
    }
    void setPlaybackTranspose(bool enabled, int semitoneOffset,
                              std::uint16_t channelFollowKeyMask = 0b1111110111111111) noexcept;
    [[nodiscard]] bool isPlaybackTransposeEnabled() const noexcept;
    [[nodiscard]] int getPlaybackTransposeOffset() const noexcept;
    [[nodiscard]] std::uint16_t getPlaybackChannelFollowKeyMask() const noexcept;
    enum class BuiltinSynthTone : std::uint8_t {
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
    /// Shared keyboard state tracking active notes across UI, computer keyboard,
    /// and playback. Exposed as mutable reference per JUCE design so CustomKeyboard
    /// can register listeners and synchronize key highlighting with audio callbacks.
    juce::MidiKeyboardState& getKeyboardState() noexcept {
        return keyboardState;
    }

    // ── Sustain Pedal Policy & Syncopated Legato Pedal (Phase 34-C) ──
    void setSustainPolicy(devpiano::core::SustainPolicy policy) noexcept {
        syncPedalProcessor.setPolicy(policy);
    }
    [[nodiscard]] devpiano::core::SustainPolicy getSustainPolicy() const noexcept {
        return syncPedalProcessor.getPolicy();
    }
    void setSustainPedalDown(bool isDown) noexcept {
        syncPedalProcessor.setPedalDown(isDown);
    }
    [[nodiscard]] bool isSustainPedalDown() const noexcept {
        return syncPedalProcessor.isPedalDown();
    }
    [[nodiscard]] bool isSyncPedalCutPending() const noexcept {
        return syncPedalProcessor.isCutPending();
    }
    void resetSyncPedal() noexcept {
        syncPedalProcessor.reset();
    }
    // ── Metronome (Phase 35-A) ──
    void setMetronomeEnabled(bool enabled) noexcept {
        metronomeProcessor.setEnabled(enabled);
    }
    [[nodiscard]] bool isMetronomeEnabled() const noexcept {
        return metronomeProcessor.isEnabled();
    }
    void setMetronomeBpm(double bpm) noexcept {
        metronomeProcessor.setBpm(bpm);
    }
    [[nodiscard]] double getMetronomeBpm() const noexcept {
        return metronomeProcessor.getBpm();
    }
    void setMetronomeTimeSignature(devpiano::core::TimeSignature sig) noexcept {
        metronomeProcessor.setTimeSignature(sig);
    }
    [[nodiscard]] devpiano::core::TimeSignature getMetronomeTimeSignature() const noexcept {
        return metronomeProcessor.getTimeSignature();
    }
    void setMetronomeVolume(float volume) noexcept {
        metronomeProcessor.setVolume(volume);
    }
    [[nodiscard]] float getMetronomeVolume() const noexcept {
        return metronomeProcessor.getVolume();
    }
    [[nodiscard]] int getMetronomeCurrentBeatNumber() const noexcept {
        return metronomeProcessor.getCurrentBeatNumber();
    }
    [[nodiscard]] bool getMetronomeIsDownbeat() const noexcept {
        return metronomeProcessor.getIsDownbeat();
    }
    [[nodiscard]] std::uint32_t getMetronomeBeatSequence() const noexcept {
        return metronomeProcessor.getBeatSequence();
    }
    devpiano::audio::MetronomeProcessor& getMetronomeProcessor() noexcept {
        return metronomeProcessor;
    }
    [[nodiscard]] const devpiano::audio::MetronomeProcessor& getMetronomeProcessor() const noexcept {
        return metronomeProcessor;
    }

private:
    void rebuildSynth();
    void applyPendingParametersIfNeeded();
    void updateAdsrOnVoices();
    void updatePianoParametersOnVoices();
    void discardWarmupInputState();
    void injectPendingAllNotesOffIfNeeded();
    bool consumePlaybackStartPreRollBlockIfNeeded();
    void recordRealtimeMidiBufferIfNeeded(int numSamples);
    void renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples);

    PluginHost* pluginHost = nullptr;
    devpiano::recording::RecordingEngine* recordingEngine = nullptr;
    devpiano::audio::BuiltinSynthesiser synth;

    struct PlaybackIdentityTracker {
        static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFF;
        static constexpr std::size_t kDefaultCapacity = 1024;

        struct Node {
            std::uint8_t outputPitch = 0;
            std::uint32_t next = kInvalidIndex;
        };

        struct Queue {
            std::uint32_t head = kInvalidIndex;
            std::uint32_t tail = kInvalidIndex;
        };

        std::vector<Node> pool;
        std::uint32_t freeListHead = kInvalidIndex;
        std::uint64_t currentGeneration = 0;

        std::array<std::array<Queue, 128>, 16> sourceQueues {};
        std::array<std::array<std::size_t, 128>, 16> outputHolders {};

        PlaybackIdentityTracker() {
            pool.resize(kDefaultCapacity);
            resetOwnership();
        }

        void prepare(std::uint64_t generation, std::size_t noteOnCount) {
            const auto requiredCapacity = std::max(noteOnCount, kDefaultCapacity);
            if (currentGeneration == generation && pool.size() >= requiredCapacity) {
                return;
            }
            currentGeneration = generation;
            if (pool.size() < requiredCapacity) {
                pool.resize(requiredCapacity);
            }
            resetOwnership();
        }

        void resetOwnership() noexcept {
            for (auto& ch : sourceQueues) {
                ch.fill({ kInvalidIndex, kInvalidIndex });
            }
            for (auto& ch : outputHolders) {
                ch.fill(0);
            }
            initFreeList();
        }

        void resetChannel(int channel) noexcept {
            const auto chIdx = juce::jlimit(0, 15, channel - 1);
            for (int pitch = 0; pitch < 128; ++pitch) {
                auto& q = sourceQueues[chIdx][pitch];
                auto curr = q.head;
                while (curr != kInvalidIndex && curr < pool.size()) {
                    const auto next = pool[curr].next;
                    freeNode(curr);
                    curr = next;
                }
                q = { kInvalidIndex, kInvalidIndex };
                outputHolders[chIdx][pitch] = 0;
            }
        }

        void initFreeList() noexcept {
            if (pool.empty()) {
                freeListHead = kInvalidIndex;
                return;
            }
            for (std::size_t i = 0; i + 1 < pool.size(); ++i) {
                pool[i].next = static_cast<std::uint32_t>(i + 1);
            }
            pool.back().next = kInvalidIndex;
            freeListHead = 0;
        }

        std::uint32_t allocateNode() noexcept {
            if (freeListHead == kInvalidIndex) {
                return kInvalidIndex;
            }
            const auto idx = freeListHead;
            freeListHead = pool[idx].next;
            pool[idx].next = kInvalidIndex;
            return idx;
        }

        void freeNode(std::uint32_t idx) noexcept {
            if (idx == kInvalidIndex || idx >= pool.size()) {
                return;
            }
            pool[idx].next = freeListHead;
            freeListHead = idx;
        }

        std::optional<std::uint8_t> noteOn(int sourceChannel, int sourcePitch, int candidateOutputPitch) noexcept {
            const auto chIdx = juce::jlimit(0, 15, sourceChannel - 1);
            const auto pitchIdx = juce::jlimit(0, 127, sourcePitch);
            const auto outPitch = static_cast<std::uint8_t>(juce::jlimit(0, 127, candidateOutputPitch));

            const auto nodeIdx = allocateNode();
            if (nodeIdx == kInvalidIndex) {
                return std::nullopt;
            }
            pool[nodeIdx].outputPitch = outPitch;
            pool[nodeIdx].next = kInvalidIndex;

            auto& q = sourceQueues[chIdx][pitchIdx];
            if (q.tail == kInvalidIndex) {
                q.head = q.tail = nodeIdx;
            } else {
                pool[q.tail].next = nodeIdx;
                q.tail = nodeIdx;
            }

            outputHolders[chIdx][outPitch]++;
            return outPitch;
        }

        struct NoteOffResult {
            std::uint8_t outputPitch = 0;
            bool shouldEmit = false;
            bool matched = false;
        };

        NoteOffResult noteOff(int sourceChannel, int sourcePitch) noexcept {
            const auto chIdx = juce::jlimit(0, 15, sourceChannel - 1);
            const auto pitchIdx = juce::jlimit(0, 127, sourcePitch);
            auto& q = sourceQueues[chIdx][pitchIdx];

            if (q.head == kInvalidIndex) {
                return { 0, false, false };
            }

            const auto nodeIdx = q.head;
            q.head = pool[nodeIdx].next;
            if (q.head == kInvalidIndex) {
                q.tail = kInvalidIndex;
            }

            const auto outPitch = pool[nodeIdx].outputPitch;
            freeNode(nodeIdx);

            auto& holders = outputHolders[chIdx][outPitch];
            bool shouldEmit = false;
            if (holders > 0) {
                holders--;
                if (holders == 0) {
                    shouldEmit = true;
                }
            } else {
                shouldEmit = true;
            }

            return { outPitch, shouldEmit, true };
        }
    };

    PlaybackIdentityTracker playbackIdentityTracker;
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;
    juce::MidiBuffer playbackVisualMidiBuffer;
    juce::MidiBuffer playbackTransposedMidiBuffer;
    juce::AudioBuffer<float> pluginBuffer;

    devpiano::audio::SyncPedalProcessor syncPedalProcessor;
    juce::MidiBuffer syncPedalTempBuffer;
    devpiano::audio::MetronomeProcessor metronomeProcessor;
    juce::ADSR::Parameters adsrParameters;
    std::atomic<float> masterGain { 1.0f };
    BuiltinSynthTone builtinTone = BuiltinSynthTone::piano;
    std::atomic<float> pendingBrightness { 0.5f };
    std::atomic<float> pendingHammerHardness { 0.5f };
    std::atomic<float> pendingResonance { 0.5f };
    std::atomic<float> pendingAttack { 0.01f };
    std::atomic<float> pendingDecay { 0.2f };
    std::atomic<float> pendingSustain { 0.8f };
    std::atomic<float> pendingRelease { 0.3f };
    std::atomic<std::uint8_t> pendingLidPosition { 0 };
    std::atomic<std::uint8_t> pendingTemperament { 0 };
    std::atomic<double> pendingReferencePitchA4 { devpiano::audio::TemperamentEngine::kDefaultReferencePitch };
    std::atomic<bool> parametersNeedUpdate { true };
    std::atomic<std::uint8_t> pendingSoundPerspective { 0 };
    std::atomic<std::uint8_t> pendingReverbSpace { static_cast<std::uint8_t>(ReverbSpace::chamber) };
    std::atomic<float> pendingReverbWet { 0.0f };
    std::atomic<float> pendingPedalNoiseLevel { 0.6f };
    std::atomic<float> pendingFeltAgeingAmount { 0.0f };
    devpiano::audio::RoomReverbEngine roomReverb;
    ReverbSpace pianoReverbSpace = ReverbSpace::chamber;
    float pianoReverbWet = 0.0f;
    float pianoPedalNoiseLevel = 0.6f;
    float pianoFeltAgeingAmount = 0.0f;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    LidPosition pianoLidPosition = LidPosition::fullOpen;
    Temperament pianoTemperament = Temperament::equal;
    double pianoReferencePitchA4 = devpiano::audio::TemperamentEngine::kDefaultReferencePitch;
    std::atomic<double> currentSampleRate { 48000.0 };
    SoundPerspective pianoSoundPerspective = SoundPerspective::player;
    std::atomic<int> currentBlockSize { 128 };
    std::atomic_bool allNotesOffPending { false };
    std::atomic<int> warmupBlocksRemaining { 0 };
    std::atomic<int> playbackStartPreRollBlocksRemaining { 0 };
    std::atomic<int> pluginBufferResizeCount { 0 };
    std::atomic<bool> playbackTransposeEnabled { false };
    std::atomic<int> playbackTransposeOffset { 0 };
    std::atomic<std::uint16_t> playbackChannelFollowKeyMask { 0b1111110111111111 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
