#pragma once
#include "Audio/BoundedDelayLine.h"
#include "Audio/InstrumentLayers.h"
#include "Audio/InstrumentNoteState.h"

#include "AcousticSnapshot.h"
#include "Audio/BuiltinSynthesiser.h"
#include "Audio/MetronomeProcessor.h"
#include "Audio/SyncPedalProcessor.h"
#include "PerspectiveProcessor.h"
#include "PlaybackIdentityTracker.h"
#include "RealtimeExchange.h"
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

namespace juce {
class AudioPluginInstance;
}

namespace devpiano::recording {
class RecordingEngine;
}

class AudioEngine : private juce::MidiKeyboardState::Listener {
public:
    AudioEngine();
    ~AudioEngine() override;

    void setPluginHost(PluginHost* host) noexcept;
    void setRecordingEngine(devpiano::recording::RecordingEngine* engine) noexcept;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void preparePlaybackResources();
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();
    void requestAllNotesOff() noexcept;
    void armPlaybackStartPreRoll(double sampleRate, int blockSize) noexcept;
    void sendController(int channel, int controllerType, int value);
    void dispatchPendingDisplayEvents();
    [[nodiscard]] devpiano::audio::AcousticSnapshot captureAcousticSnapshot() const noexcept;
    [[nodiscard]] std::size_t consumeRealtimeOverflowCount() noexcept;

    void setMasterGain(float newGain);
    void setAdsr(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds);
    void setPianoParameters(float brightness, float hammerHardness, float resonance);
    void setPianoTuning(bool stretchTuningEnabled, float duplexResonance);
    [[nodiscard]] bool isStretchTuningEnabled() const noexcept {
        return pendingStretchTuningEnabled.load(std::memory_order_relaxed);
    }
    [[nodiscard]] float getDuplexResonance() const noexcept {
        return pendingDuplexResonance.load(std::memory_order_relaxed);
    }
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
    void setInputTranspose(bool enabled, int semitoneOffset,
                           std::uint16_t channelFollowKeyMask = 0b1111110111111111) noexcept;
    [[nodiscard]] bool isInputTransposeEnabled() const noexcept;
    [[nodiscard]] int getInputTransposeOffset() const noexcept;
    [[nodiscard]] std::uint16_t getInputChannelFollowKeyMask() const noexcept;
    void setInstrumentLayers(const devpiano::audio::InstrumentLayers& layers) noexcept;
    [[nodiscard]] devpiano::audio::InstrumentLayers getInstrumentLayers() const noexcept;
    [[nodiscard]] int consumeLatencyOverflowCount() noexcept;
    [[nodiscard]] bool consumeLatencyFaultPending() noexcept;
    enum class BuiltinSynthTone : std::uint8_t {
        sine,
        piano,
    };
    void setBuiltinSynthTone(BuiltinSynthTone tone);
    [[nodiscard]] BuiltinSynthTone getBuiltinSynthTone() const noexcept {
        return builtinTone.load(std::memory_order_relaxed);
    }

    [[nodiscard]] PluginHost* getPluginHost() const noexcept {
        return pluginHost;
    }

    // Geometry faults are counted on the callback and diagnosed on the message thread.
    [[nodiscard]] int consumePluginBufferResizeCount() noexcept;

    // Block counts for the startup warmup and the playback-start pre-roll
    // silence windows.  Exposed as pure functions so unit tests can verify
    // the duration→block mapping (AUDIT TEST-009).
    [[nodiscard]] static int calculateWarmupBlockCount(double sampleRate, int blockSize) noexcept;
    [[nodiscard]] static int calculatePlaybackStartPreRollBlockCount(double sampleRate, int blockSize) noexcept;
    // UI-owned state: audio publishes bounded snapshots, never calls listeners.
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
    void discardWarmupInputState();
    void injectPendingAllNotesOffIfNeeded();
    bool consumePlaybackStartPreRollBlockIfNeeded();
    void recordRealtimeMidiBufferIfNeeded(int numSamples);
    void renderPlaybackEventsIfNeeded(std::int64_t blockStartSamples, int numSamples);
    void mergePerformanceInput();
    void resetPerformanceInputOwnership() noexcept;
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void enqueueLiveMidi(const juce::MidiMessage& message) noexcept;
    void collectLiveMidi(int numSamples) noexcept;
    void clearDisplayNotes() noexcept;
    void applyAcousticSnapshot(const devpiano::audio::AcousticSnapshot& snapshot, bool recordedPreset = false);
    void renderInstrumentSegment(const juce::AudioSourceChannelInfo& output, int offset, int numSamples);

    PluginHost* pluginHost = nullptr;
    devpiano::recording::RecordingEngine* recordingEngine = nullptr;
    devpiano::audio::BuiltinSynthesiser synth;
    devpiano::audio::BuiltinSynthesiser sineSynth;
    devpiano::audio::AcousticSnapshot activeAcoustic;
    devpiano::audio::BuiltinSynthesiser* activeSynth = &synth;
    float activeMasterGain = 1.0f;
    bool muteInstrumentDuringBlock = false;
    bool syncCutInBlock = false;
    struct PresetBoundary {
        const devpiano::audio::AcousticSnapshot* acoustic = nullptr;
        int sampleOffset = 0;
        std::size_t midiEventCount = 0;
    };
    std::vector<PresetBoundary> presetBoundaries;

    devpiano::audio::PlaybackIdentityTracker playbackIdentityTracker;
    std::array<std::array<std::array<bool, 128>, 16>, 2> inputNotes {};
    std::array<std::array<std::array<std::uint8_t, 3>, 16>, 2> inputPedals {};
    bool hasRecordedSoftBaseline = false;
    bool recordedSoftBaseline = false;
    struct LiveMidiEvent {
        std::array<std::uint8_t, 3> bytes {};
        std::uint8_t size = 0;
        std::uint32_t timestampMilliseconds = 0;
    };
    devpiano::audio::RealtimeQueue<LiveMidiEvent, 4096> liveMidiQueue;
    std::atomic_bool liveOverflowPending { false };
    std::atomic<std::size_t> realtimeOverflowCount { 0 };
    std::array<std::array<std::atomic<std::uint32_t>, 128>, 16> displayNotes {};
    std::array<std::array<std::uint32_t, 128>, 16> observedDisplayNotes {};
    std::uint32_t displaySequence = 0;
    bool dispatchingDisplay = false;
    juce::MidiBuffer uiDiscardMidi;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;
    juce::MidiBuffer playbackVisualMidiBuffer;
    juce::MidiBuffer playbackOwnedMidiBuffer;
    juce::AudioBuffer<float> pluginBuffer;
    juce::AudioBuffer<float> pianoBuffer;
    juce::AudioBuffer<float> pianoDelayedBuffer;
    juce::MidiBuffer pluginSegmentMidiBuffer;
    juce::MidiBuffer builtinSegmentMidiBuffer;
    std::vector<std::uint8_t> midiEventSources;
    std::vector<std::uint8_t> segmentEventSources;
    std::vector<std::uint8_t> boundaryEventSources;
    juce::MidiBuffer segmentMidiBuffer;
    juce::MidiBuffer boundaryMidiBuffer;
    juce::AudioBuffer<float> pluginView;
    juce::AudioBuffer<float> pianoView;
    juce::AudioBuffer<float> pianoDelayedView;
    devpiano::audio::BoundedStereoDelayLine pianoDelayLine;
    std::atomic<int> latencyOverflowCount { 0 };
    std::atomic<bool> latencyFaultPending { false };
    devpiano::audio::InstrumentLayers activeLayers;
    bool previousLayersEnabled = false;
    devpiano::audio::BuiltinSynthesiser* previousBuiltinSynth = nullptr;
    bool previousPluginEnabled = false;
    juce::AudioPluginInstance* previousPluginInstance = nullptr;
    devpiano::audio::InstrumentNoteState builtinNoteState;
    devpiano::audio::InstrumentNoteState pluginNoteState;
    std::array<std::array<std::uint8_t, 3>, 16> performancePedals {};
    int preparedPluginChannels = 0;
    std::size_t preparedMidiCapacity = 131072;

    devpiano::audio::SyncPedalProcessor syncPedalProcessor;
    juce::MidiBuffer syncPedalTempBuffer;
    devpiano::audio::MetronomeProcessor metronomeProcessor;
    std::atomic<float> masterGain { 1.0f };
    std::atomic<BuiltinSynthTone> builtinTone { BuiltinSynthTone::piano };
    std::atomic<float> pendingBrightness { 0.5f };
    std::atomic<float> pendingHammerHardness { 0.5f };
    std::atomic<float> pendingResonance { 0.5f };
    std::atomic<bool> pendingStretchTuningEnabled { true };
    std::atomic<float> pendingDuplexResonance { 0.15f };
    std::atomic<float> pendingAttack { 0.01f };
    std::atomic<float> pendingDecay { 0.2f };
    std::atomic<float> pendingSustain { 0.8f };
    std::atomic<float> pendingRelease { 0.3f };
    std::atomic<std::uint8_t> pendingLidPosition { 0 };
    std::atomic<std::uint8_t> pendingTemperament { 0 };
    std::atomic<double> pendingReferencePitchA4 { devpiano::audio::TemperamentEngine::kDefaultReferencePitch };
    enum ParameterMask : std::uint32_t {
        gainParameter = 1U << 0,
        adsrParameter = 1U << 1,
        pianoParameter = 1U << 2,
        toneParameter = 1U << 3,
        lidParameter = 1U << 4,
        temperamentParameter = 1U << 5,
        pitchParameter = 1U << 6,
        perspectiveParameter = 1U << 7,
        spaceParameter = 1U << 8,
        wetParameter = 1U << 9,
        noiseParameter = 1U << 10,
        feltParameter = 1U << 11,
        transposeParameter = 1U << 12,
        tuningParameter = 1U << 13,
        layersParameter = 1U << 14,
        allParameters = (1U << 15) - 1,
    };
    std::atomic<bool> pendingLayersEnabled { false };
    std::atomic<bool> pendingPianoEnabled { true };
    std::atomic<bool> pendingPluginEnabled { true };
    std::atomic<float> pendingPianoGain { 0.5f };
    std::atomic<float> pendingPluginGain { 0.5f };
    std::atomic<std::uint32_t> pendingParameterMask { allParameters };
    std::atomic<std::uint8_t> pendingSoundPerspective { 0 };
    std::atomic<std::uint8_t> pendingReverbSpace { static_cast<std::uint8_t>(ReverbSpace::chamber) };
    std::atomic<float> pendingReverbWet { 0.0f };
    std::atomic<float> pendingPedalNoiseLevel { 0.6f };
    std::atomic<float> pendingFeltAgeingAmount { 0.0f };
    devpiano::audio::RoomReverbEngine roomReverb;
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBlockSize { 128 };
    std::atomic_bool allNotesOffPending { false };
    std::atomic<int> warmupBlocksRemaining { 0 };
    std::atomic<int> playbackStartPreRollBlocksRemaining { 0 };
    std::atomic<int> pluginBufferResizeCount { 0 };
    std::atomic<bool> inputTransposeEnabled { false };
    std::atomic<int> inputTransposeOffset { 0 };
    std::atomic<std::uint16_t> inputChannelFollowKeyMask { 0b1111110111111111 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
