#pragma once

#include "../Audio/PerspectiveProcessor.h"
#include "../Audio/RoomReverbEngine.h"
#include "../Audio/TemperamentEngine.h"
#include "../Input/TouchVelocityCurve.h"
#include "../Midi/ChannelMatrix.h"
#include "../UI/KeyboardTypes.h"
#include "Core/AppState.h"
#include "Core/KeyMapTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

// Persisted settings model.
//
// 职责边界：
// - 仅表示“需要落盘并在下次启动时恢复”的配置基线
// - 不应承载只在本次运行期间存在的瞬时状态
// - 运行态聚合请使用 devpiano::core::AppState
//
// 典型 persisted 内容：
// - 音频设备序列化状态
// - ADSR / Gain 参数
// - 上次插件搜索路径 / 上次插件名
// - 键盘布局持久化形态
struct SettingsModel {
    // 内置 fallback 音色（Phase 12-2/12-3）：模型层独立枚举，AudioEngine
    // 的 BuiltinSynthTone 与之映射（MainComponent 负责转换）。
    using BuiltinTone = devpiano::core::BuiltinTone;

    // 琴盖开合度（Phase 29-A）：现实物理声学控制，映射至 PianoSynthVoice / AudioEngine。
    enum class LidPosition : std::uint8_t {
        fullOpen = 0,
        halfStick = 1,
        closed = 2,
    };

    struct AudioSettingsView {
        double sampleRate = 48000.0;
        int bufferSize = 128;
        bool hasSerializedDeviceState = false;
    };

    struct PerformanceSettingsView {
        float masterGain = 1.0f;
        float adsrAttack = 0.01f;
        float adsrDecay = 0.20f;
        float adsrSustain = 0.80f;
        float adsrRelease = 0.30f;
        BuiltinTone builtinTone = BuiltinTone::piano;
        float pianoBrightness = 0.50f;
        float pianoHammerHardness = 0.50f;
        float pianoResonance = 0.50f;
        LidPosition lidPosition = LidPosition::fullOpen;
        devpiano::input::TouchVelocityCurve touchVelocityCurve = devpiano::input::TouchVelocityCurve::standard;
        bool unaCorda = false;
        devpiano::audio::Temperament temperament = devpiano::audio::Temperament::equal;
        double referencePitchA4 = devpiano::audio::TemperamentEngine::kDefaultReferencePitch;
        devpiano::audio::SoundPerspective soundPerspective = devpiano::audio::SoundPerspective::player;
        devpiano::audio::ReverbSpace reverbSpace = devpiano::audio::ReverbSpace::chamber;
        float reverbWet = 0.0f;
        // Mechanical action noise and physical imperfection (Phase 32-A/C)
        float pedalNoiseLevel = 0.6f;
        float feltAgeingAmount = 0.0f;
    };

    struct PluginRecoverySettingsView {
        juce::String pluginSearchPath;
        juce::String lastPluginName;
    };

    struct KeyboardDisplaySettingsView {
        devpiano::ui::KeyColourMode colourMode = devpiano::ui::KeyColourMode::classic;
        devpiano::ui::NoteDisplayMode noteDisplay = devpiano::ui::NoteDisplayMode::doReMi;
        float fadeSpeed = 0.92f;
        bool showInstrumentFilter = true;
        std::array<juce::String, 128> customKeyLabels;
        std::array<juce::Colour, 128> customKeyColours;
    };

    // Persisted audio device state (serialized XML from AudioDeviceManager)
    std::unique_ptr<juce::XmlElement> audioDeviceState;

    // Persisted audio baseline.
    // 这些值用于启动恢复与无设备时的后备值，不代表当前运行时设备一定已经采用它们。
    double sampleRate = 48000.0;
    int bufferSize = 128;

    // Persisted performance parameters.
    float masterGain = 1.0f;
    float adsrAttack = 0.01f;
    float adsrDecay = 0.20f;
    float adsrSustain = 0.80f;
    float adsrRelease = 0.30f;
    BuiltinTone builtinTone = BuiltinTone::piano;
    float pianoBrightness = 0.50f;
    float pianoHammerHardness = 0.50f;
    float pianoResonance = 0.50f;
    LidPosition lidPosition = LidPosition::fullOpen;
    devpiano::input::TouchVelocityCurve touchVelocityCurve = devpiano::input::TouchVelocityCurve::standard;
    bool unaCorda = false;
    devpiano::audio::Temperament temperament = devpiano::audio::Temperament::equal;
    double referencePitchA4 = devpiano::audio::TemperamentEngine::kDefaultReferencePitch;
    devpiano::audio::SoundPerspective soundPerspective = devpiano::audio::SoundPerspective::player;
    devpiano::audio::ReverbSpace reverbSpace = devpiano::audio::ReverbSpace::chamber;
    float reverbWet = 0.0f;
    // Mechanical action noise and physical imperfection (Phase 32-A/C)
    float pedalNoiseLevel = 0.6f;
    float feltAgeingAmount = 0.0f;

    // Persisted ,UI recovery state.
    juce::String pluginSearchPath;
    juce::String lastPluginName;
    std::unique_ptr<juce::XmlElement> knownPluginListState;
    juce::String lastActivePresetId; // last-used preset file name (without extension)
    // Persisted last MIDI import/export paths for FileChooser defaults.
    juce::String lastMidiImportPath;
    juce::String lastMidiExportPath;
    // Persisted recently-opened files list (juce::RecentlyOpenedFilesList serialized).
    juce::String recentFilesSerialized;

    // Persisted main content size. Zero means unset; startup will use preferred size.
    int mainWindowWidth = 0;
    int mainWindowHeight = 0;
    int keyboardScrollOffsetX = -1; // persisted keyboard Viewport scroll position (-1 = unset)
    // Persisted keyboard display settings (single View instance — DOC-006:
    // one declaration of the defaults, no parallel flat fields to drift).
    KeyboardDisplaySettingsView keyboardDisplay;
    bool pluginPanelExpanded = false; // persisted PluginPanel collapsed/expanded toggle
    bool qwertyVisualizerExpanded = true; // persisted QwertyVisualizer collapsed/expanded toggle
    devpiano::core::SustainPolicy sustainPolicy = devpiano::core::SustainPolicy::syncPedal;
    // Persisted UI language code ("en" | "zh-CN").
    juce::String languageCode { "en" };
    // Key signature system: global transpose state
    bool midiTranspose = false;
    int keySignature = 0; // semitone offset from C, -7..+7

    devpiano::midi::ChannelMatrix channelMatrix;

    [[nodiscard]] AudioSettingsView getAudioSettingsView() const {
        return { .sampleRate = sampleRate,
                 .bufferSize = bufferSize,
                 .hasSerializedDeviceState = audioDeviceState != nullptr };
    }

    void applyAudioSettingsView(const AudioSettingsView& view) {
        sampleRate = view.sampleRate;
        bufferSize = view.bufferSize;
    }

    void setSerializedAudioDeviceState(std::unique_ptr<juce::XmlElement> state) {
        audioDeviceState = std::move(state);
    }

    [[nodiscard]] PerformanceSettingsView getPerformanceSettingsView() const {
        return { .masterGain = masterGain,
                 .adsrAttack = adsrAttack,
                 .adsrDecay = adsrDecay,
                 .adsrSustain = adsrSustain,
                 .adsrRelease = adsrRelease,
                 .builtinTone = builtinTone,
                 .pianoBrightness = pianoBrightness,
                 .pianoHammerHardness = pianoHammerHardness,
                 .pianoResonance = pianoResonance,
                 .lidPosition = lidPosition,
                 .touchVelocityCurve = touchVelocityCurve,
                 .unaCorda = unaCorda,
                 .temperament = temperament,
                 .referencePitchA4 = referencePitchA4,
                 .soundPerspective = soundPerspective,
                 .reverbSpace = reverbSpace,
                 .reverbWet = reverbWet,
                 .pedalNoiseLevel = pedalNoiseLevel,
                 .feltAgeingAmount = feltAgeingAmount };
    }

    void applyPerformanceSettingsView(const PerformanceSettingsView& view) {
        masterGain = view.masterGain;
        adsrAttack = view.adsrAttack;
        adsrDecay = view.adsrDecay;
        adsrSustain = view.adsrSustain;
        adsrRelease = view.adsrRelease;
        builtinTone = view.builtinTone;
        pianoBrightness = view.pianoBrightness;
        pianoHammerHardness = view.pianoHammerHardness;
        pianoResonance = view.pianoResonance;
        lidPosition = view.lidPosition;
        touchVelocityCurve = view.touchVelocityCurve;
        unaCorda = view.unaCorda;
        temperament = view.temperament;
        referencePitchA4 = view.referencePitchA4;
        soundPerspective = view.soundPerspective;
        reverbSpace = view.reverbSpace;
        reverbWet = view.reverbWet;
        pedalNoiseLevel = view.pedalNoiseLevel;
        feltAgeingAmount = view.feltAgeingAmount;
    }
    [[nodiscard]] PluginRecoverySettingsView getPluginRecoverySettingsView() const {
        return { .pluginSearchPath = pluginSearchPath, .lastPluginName = lastPluginName };
    }

    void applyPluginRecoverySettingsView(const PluginRecoverySettingsView& view) {
        pluginSearchPath = view.pluginSearchPath;
        lastPluginName = view.lastPluginName;
    }

    [[nodiscard]] const KeyboardDisplaySettingsView& getKeyboardDisplaySettingsView() const {
        return keyboardDisplay;
    }

    void applyKeyboardDisplaySettingsView(const KeyboardDisplaySettingsView& view) {
        keyboardDisplay = view;
    }

    SettingsModel() = default;
    ~SettingsModel() = default;
    SettingsModel(SettingsModel&&) noexcept = default;
    SettingsModel& operator=(SettingsModel&&) noexcept = default;

    SettingsModel(const SettingsModel& other) {
        *this = other;
    }

    SettingsModel& operator=(const SettingsModel& other) {
        if (this != &other) {
            sampleRate = other.sampleRate;
            bufferSize = other.bufferSize;
            masterGain = other.masterGain;
            adsrAttack = other.adsrAttack;
            adsrDecay = other.adsrDecay;
            adsrSustain = other.adsrSustain;
            adsrRelease = other.adsrRelease;
            builtinTone = other.builtinTone;
            pianoBrightness = other.pianoBrightness;
            pianoHammerHardness = other.pianoHammerHardness;
            pianoResonance = other.pianoResonance;
            lidPosition = other.lidPosition;
            touchVelocityCurve = other.touchVelocityCurve;
            unaCorda = other.unaCorda;
            temperament = other.temperament;
            referencePitchA4 = other.referencePitchA4;
            soundPerspective = other.soundPerspective;
            reverbSpace = other.reverbSpace;
            reverbWet = other.reverbWet;
            pedalNoiseLevel = other.pedalNoiseLevel;
            feltAgeingAmount = other.feltAgeingAmount;
            pluginSearchPath = other.pluginSearchPath;
            lastPluginName = other.lastPluginName;
            lastActivePresetId = other.lastActivePresetId;
            lastMidiImportPath = other.lastMidiImportPath;
            lastMidiExportPath = other.lastMidiExportPath;
            recentFilesSerialized = other.recentFilesSerialized;
            mainWindowWidth = other.mainWindowWidth;
            mainWindowHeight = other.mainWindowHeight;
            keyboardScrollOffsetX = other.keyboardScrollOffsetX;
            keyboardDisplay = other.keyboardDisplay;
            pluginPanelExpanded = other.pluginPanelExpanded;
            qwertyVisualizerExpanded = other.qwertyVisualizerExpanded;
            languageCode = other.languageCode;
            sustainPolicy = other.sustainPolicy;
            midiTranspose = other.midiTranspose;
            keySignature = other.keySignature;
            channelMatrix = other.channelMatrix;

            audioDeviceState
                = other.audioDeviceState ? std::make_unique<juce::XmlElement>(*other.audioDeviceState) : nullptr;
            knownPluginListState = other.knownPluginListState
                ? std::make_unique<juce::XmlElement>(*other.knownPluginListState)
                : nullptr;
        }
        return *this;
    }
    // ---- Serialization methods moved to Settings/SettingsSerialization.h ----
};

// Shared KeyboardSettings assembly from the persisted view + key signature
// (QUAL-007): MainComponent and SettingsWindowManager build the same field
// subset for CustomKeyboard; keep it in one place.
[[nodiscard]] inline devpiano::ui::KeyboardSettings
makeKeyboardSettings(const SettingsModel::KeyboardDisplaySettingsView& view, int keySignature) {
    devpiano::ui::KeyboardSettings ks;
    ks.colourMode = view.colourMode;
    ks.noteDisplay = view.noteDisplay;
    ks.fadeSpeed = view.fadeSpeed;
    ks.keySignature = keySignature;
    ks.customKeyLabels = view.customKeyLabels;
    ks.customKeyColours = view.customKeyColours;
    return ks;
}
