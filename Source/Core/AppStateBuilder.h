#pragma once

#include <JuceHeader.h>

#include "Core/AppState.h"
#include "Settings/SettingsModel.h"

namespace devpiano::core
{
// Bridge layer between persisted settings and runtime aggregate state.
//
// 设计意图：
// - createPersistedAppState(): 只从 SettingsModel 提取“可持久化基线”
// - applyRuntime*State(): 再叠加本次运行期间才存在的覆盖层
//
// 当前先保持为轻量 header-only helper，避免过早引入额外类型迁移成本。
struct RuntimePluginState
{
    juce::String currentPluginName;
    juce::StringArray availablePluginNames;
    juce::String pluginListText;
    juce::String availableFormatsDescription;
    juce::String lastScanSummary;
    juce::String lastLoadError;
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    bool supportsVst3 = false;
    bool hasLoadedPlugin = false;
    bool isPrepared = false;
    bool isEditorOpen = false;
};

struct RuntimeInputState
{
    KeyboardLayout keyboardLayout = makeDefaultKeyboardLayout();
    int openMidiInputCount = 0;
    int midiActivityCount = 0;
    juce::String lastMidiMessage;
};

// 从 persisted settings 创建 AppState 基线。
// 不读取 PluginHost / MidiRouter / EditorWindow 等运行态对象。
[[nodiscard]] inline AppState createPersistedAppState(const SettingsModel& settings)
{
    const auto audio = settings.getAudioSettingsView();
    const auto performance = settings.getPerformanceSettingsView();
    const auto plugin = settings.getPluginRecoverySettingsView();
    const auto input = settings.getInputMappingSettingsView();

    return { .audio = { .sampleRate = audio.sampleRate,
                        .bufferSize = audio.bufferSize,
                        .hasSerializedDeviceState = audio.hasSerializedDeviceState },
             .performance = { .masterGain = performance.masterGain,
                              .adsrAttack = performance.adsrAttack,
                              .adsrDecay = performance.adsrDecay,
                              .adsrSustain = performance.adsrSustain,
                              .adsrRelease = performance.adsrRelease },
             .plugin = { .searchPath = plugin.pluginSearchPath,
                         .lastPluginName = plugin.lastPluginName },
             .input = { .layoutId = input.layoutId,
                        .keyboardLayout = SettingsModel::keyMapToLayout(input.keyMap) } };
}

// 叠加运行时插件宿主状态。
inline void applyRuntimePluginState(AppState& appState, const RuntimePluginState& runtime)
{
    appState.plugin.currentPluginName = runtime.currentPluginName;
    appState.plugin.availablePluginNames = runtime.availablePluginNames;
    appState.plugin.pluginListText = runtime.pluginListText;
    appState.plugin.availableFormatsDescription = runtime.availableFormatsDescription;
    appState.plugin.lastScanSummary = runtime.lastScanSummary;
    appState.plugin.lastLoadError = runtime.lastLoadError;
    appState.plugin.preparedSampleRate = runtime.preparedSampleRate;
    appState.plugin.preparedBlockSize = runtime.preparedBlockSize;
    appState.plugin.supportsVst3 = runtime.supportsVst3;
    appState.plugin.hasLoadedPlugin = runtime.hasLoadedPlugin;
    appState.plugin.isPrepared = runtime.isPrepared;
    appState.plugin.isEditorOpen = runtime.isEditorOpen;
}

// 叠加运行时输入活动状态。
inline void applyRuntimeInputState(AppState& appState, const RuntimeInputState& runtime)
{
    appState.input.layoutId = runtime.keyboardLayout.id;
    appState.input.keyboardLayout = runtime.keyboardLayout;
    appState.input.openMidiInputCount = runtime.openMidiInputCount;
    appState.input.midiActivityCount = runtime.midiActivityCount;
    appState.input.lastMidiMessage = runtime.lastMidiMessage;
}
}
