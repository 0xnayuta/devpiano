#pragma once

#include <JuceHeader.h>


#include "Core/ChannelMatrix.h"
#include "Core/KeyboardTypes.h"

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
    struct AudioSettingsView {
        double sampleRate = 44100.0;
        int bufferSize = 512;
        bool hasSerializedDeviceState = false;
    };

    struct PerformanceSettingsView {
        float masterGain = 0.8f;
        float adsrAttack = 0.01f;
        float adsrDecay = 0.20f;
        float adsrSustain = 0.80f;
        float adsrRelease = 0.30f;
    };

    struct PluginRecoverySettingsView {
        juce::String pluginSearchPath;
        juce::String lastPluginName;
    };

    struct KeyboardDisplaySettingsView {
        devpiano::ui::KeyColourMode colourMode = devpiano::ui::KeyColourMode::classic;
        devpiano::ui::NoteDisplayMode noteDisplay = devpiano::ui::NoteDisplayMode::doReMi;
        float fadeSpeed = 0.92f;
        bool resizableWindow = true;
        bool showInstrumentFilter = true;
        std::array<juce::String, 128> customKeyLabels;
        std::array<juce::Colour, 128> customKeyColours;
    };

    // Persisted audio device state (serialized XML from AudioDeviceManager)
    std::unique_ptr<juce::XmlElement> audioDeviceState;

    // Persisted audio baseline.
    // 这些值用于启动恢复与无设备时的后备值，不代表当前运行时设备一定已经采用它们。
    double sampleRate = 44100.0;
    int bufferSize = 512;

    // Persisted performance parameters.
    float masterGain = 0.8f;
    float adsrAttack = 0.01f;
    float adsrDecay = 0.20f;
    float adsrSustain = 0.80f;
    float adsrRelease = 0.30f;

    // Persisted ,UI recovery state.
    juce::String pluginSearchPath;
    juce::String lastPluginName;
    std::unique_ptr<juce::XmlElement> knownPluginListState;
    juce::String lastActivePresetId;  // last-used preset file name (without extension)
    // Persisted last MIDI import/export paths for FileChooser defaults.
    juce::String lastMidiImportPath;
    juce::String lastMidiExportPath;
    // Persisted recently-opened files list (juce::RecentlyOpenedFilesList serialized).
    juce::String recentFilesSerialized;

    // Persisted main content size. Zero means unset; startup will use preferred size.
    int mainWindowWidth = 0;
    int mainWindowHeight = 0;
    int keyboardScrollOffsetX = -1; // persisted keyboard Viewport scroll position (-1 = unset)
    // Persisted keyboard display settings.
    devpiano::ui::KeyColourMode keyboardColourMode = devpiano::ui::KeyColourMode::classic;
    devpiano::ui::NoteDisplayMode keyboardNoteDisplay = devpiano::ui::NoteDisplayMode::doReMi;
    float keyboardFadeSpeed = 0.92f;
    bool resizableWindow = true;
    bool showInstrumentFilter = true;
    // Persisted per-key custom labels and colours (sparse, stored as ValueTree XML).
    std::array<juce::String, 128> customKeyLabels;
    std::array<juce::Colour, 128> customKeyColours;
    // Persisted UI language code ("en" | "zh-CN").
    juce::String languageCode { "en" };
    // Key signature system: global transpose state
    bool midiTranspose = false;
    int keySignature = 0;  // semitone offset from C, -7..+7

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
                 .adsrRelease = adsrRelease };
    }

    void applyPerformanceSettingsView(const PerformanceSettingsView& view) {
        masterGain = view.masterGain;
        adsrAttack = view.adsrAttack;
        adsrDecay = view.adsrDecay;
        adsrSustain = view.adsrSustain;
        adsrRelease = view.adsrRelease;
    }

    [[nodiscard]] PluginRecoverySettingsView getPluginRecoverySettingsView() const {
        return { .pluginSearchPath = pluginSearchPath, .lastPluginName = lastPluginName };
    }

    void applyPluginRecoverySettingsView(const PluginRecoverySettingsView& view) {
        pluginSearchPath = view.pluginSearchPath;
        lastPluginName = view.lastPluginName;
    }


    [[nodiscard]] KeyboardDisplaySettingsView getKeyboardDisplaySettingsView() const {
        return { .colourMode = keyboardColourMode,
                 .noteDisplay = keyboardNoteDisplay,
                 .fadeSpeed = keyboardFadeSpeed,
                 .resizableWindow = resizableWindow,
                 .showInstrumentFilter = showInstrumentFilter,
                 .customKeyLabels = customKeyLabels,
                 .customKeyColours = customKeyColours };
    }

    void applyKeyboardDisplaySettingsView(const KeyboardDisplaySettingsView& view) {
        keyboardColourMode = view.colourMode;
        keyboardNoteDisplay = view.noteDisplay;
        keyboardFadeSpeed = view.fadeSpeed;
        resizableWindow = view.resizableWindow;
        showInstrumentFilter = view.showInstrumentFilter;
        customKeyLabels = view.customKeyLabels;
        customKeyColours = view.customKeyColours;
    }

    // ---- Serialization methods moved to Settings/SettingsSerialization.h ----
};
