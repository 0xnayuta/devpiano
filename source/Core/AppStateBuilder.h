#pragma once

#include <JuceHeader.h>

#include "AppState.h"
#include "../Settings/SettingsModel.h"

class KeyboardMidiMapper;
class MidiRouter;
class PluginHost;

namespace devpiano::core
{
// Bridge layer between persisted settings and runtime aggregate state.
//
// 设计意图：
// - createPersistedAppState(): 只从 SettingsModel 提取“可持久化基线”
// - applyRuntime*State(): 再叠加本次运行期间才存在的覆盖层
//
// Header 中保留 persisted/runtime overlay 小型 helper；跨模块 runtime snapshot 构建在 .cpp 中实现。
struct RuntimePluginState
{
    juce::String currentPluginName;
    juce::StringArray availablePluginNames;
    juce::String lastScanSummary;
    juce::String lastLoadError;
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    bool supportsVst3 = false;
    bool hasLoadedPlugin = false;
    bool isPrepared = false;
    bool isEditorOpen = false;
};

struct RuntimeAudioState
{
    bool hasLiveDevice = false;
    double sampleRate = 0.0;
    int bufferSize = 0;
    juce::String backendName;
    juce::String deviceName;
    juce::String availableBufferSizesText;
    juce::String restoreOutcome;
    juce::String mismatchReasons;
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
                         .hasSerializedDeviceState = audio.hasSerializedDeviceState,
                         .hasLiveDevice = false,
                         .backendName = {},
                         .deviceName = {},
                         .availableBufferSizesText = {},
                         .restoreOutcome = {},
                         .mismatchReasons = {} },
              .performance = { .masterGain = performance.masterGain,
                               .adsrAttack = performance.adsrAttack,
                               .adsrDecay = performance.adsrDecay,
                               .adsrSustain = performance.adsrSustain,
                               .adsrRelease = performance.adsrRelease },
              .plugin = { .searchPath = plugin.pluginSearchPath,
                          .lastPluginName = plugin.lastPluginName,
                          .currentPluginName = {},
                          .availablePluginNames = {},
                          .lastScanSummary = {},
                          .lastLoadError = {},
                          .preparedSampleRate = 0.0,
                          .preparedBlockSize = 0,
                          .supportsVst3 = false,
                          .hasLoadedPlugin = false,
                          .isPrepared = false,
                          .isEditorOpen = false },
              .input = { .layoutId = input.layoutId,
                         .keyboardLayout = SettingsModel::keyMapToLayout(input.keyMap, input.layoutId),
                         .openMidiInputCount = 0,
                         .midiActivityCount = 0,
                         .lastMidiMessage = {} } };
}

// 叠加运行时插件宿主状态。
inline void applyRuntimePluginState(AppState& appState, const RuntimePluginState& runtime)
{
    appState.plugin.currentPluginName = runtime.currentPluginName;
    appState.plugin.availablePluginNames = runtime.availablePluginNames;
    appState.plugin.lastScanSummary = runtime.lastScanSummary;
    appState.plugin.lastLoadError = runtime.lastLoadError;
    appState.plugin.preparedSampleRate = runtime.preparedSampleRate;
    appState.plugin.preparedBlockSize = runtime.preparedBlockSize;
    appState.plugin.supportsVst3 = runtime.supportsVst3;
    appState.plugin.hasLoadedPlugin = runtime.hasLoadedPlugin;
    appState.plugin.isPrepared = runtime.isPrepared;
    appState.plugin.isEditorOpen = runtime.isEditorOpen;
}

inline void applyRuntimeAudioState(AppState& appState, const RuntimeAudioState& runtime)
{
    appState.audio.hasLiveDevice = runtime.hasLiveDevice;
    if (runtime.sampleRate > 0.0)
        appState.audio.sampleRate = runtime.sampleRate;
    if (runtime.bufferSize > 0)
        appState.audio.bufferSize = runtime.bufferSize;
    appState.audio.backendName = runtime.backendName;
    appState.audio.deviceName = runtime.deviceName;
    appState.audio.availableBufferSizesText = runtime.availableBufferSizesText;
    appState.audio.restoreOutcome = runtime.restoreOutcome;
    appState.audio.mismatchReasons = runtime.mismatchReasons;
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

// 一次性组装 persisted + runtime 的完整 AppState 快照。
[[nodiscard]] inline AppState buildAppState(const SettingsModel& settings,
                                            const RuntimeAudioState& audioRuntime,
                                            const RuntimePluginState& pluginRuntime,
                                            const RuntimeInputState& inputRuntime)
{
    auto appState = createPersistedAppState(settings);
    applyRuntimeAudioState(appState, audioRuntime);
    applyRuntimePluginState(appState, pluginRuntime);
    applyRuntimeInputState(appState, inputRuntime);
    return appState;
}

// Runtime snapshot helpers read live app objects and are intended for message-thread UI refresh paths.
[[nodiscard]] RuntimeAudioState buildRuntimeAudioStateSnapshot(const SettingsModel& settings,
                                                               const juce::AudioDeviceManager& deviceManager);

[[nodiscard]] RuntimePluginState buildRuntimePluginStateSnapshot(const PluginHost& pluginHost,
                                                                 bool isEditorOpen);

[[nodiscard]] RuntimeInputState buildRuntimeInputStateSnapshot(const KeyboardMidiMapper& keyboardMidiMapper,
                                                               const MidiRouter& midiRouter,
                                                               int midiActivityCount,
                                                               const juce::String& lastMidiMessage);

[[nodiscard]] AppState buildCurrentAppStateSnapshot(const SettingsModel& settings,
                                                    const juce::AudioDeviceManager& deviceManager,
                                                    const PluginHost& pluginHost,
                                                    bool isEditorOpen,
                                                    const KeyboardMidiMapper& keyboardMidiMapper,
                                                    const MidiRouter& midiRouter,
                                                    int midiActivityCount,
                                                    const juce::String& lastMidiMessage);
}
