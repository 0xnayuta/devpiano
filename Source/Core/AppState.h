#pragma once

#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"

namespace devpiano::core
{
// Runtime aggregate state.
//
// 职责边界：
// - 表示“当前这一轮运行里，对 UI / 引擎 / 控制逻辑有意义的聚合快照”
// - 可以同时包含 persisted settings 派生出的基线值，以及运行时覆盖值
// - 本身不直接负责落盘；落盘基线请回到 SettingsModel
//
// 典型 runtime 内容：
// - 当前已加载插件、prepared 状态、editor 状态
// - 当前 MIDI 输入活动、最后一条消息
// - 当前布局对象、运行时设备状态快照
struct AudioState
{
    // Snapshot values used by UI / runtime logic.
    double sampleRate = 44100.0;
    int bufferSize = 512;
    bool hasSerializedDeviceState = false;
};

struct PerformanceState
{
    float masterGain = 0.8f;
    float adsrAttack = 0.01f;
    float adsrDecay = 0.20f;
    float adsrSustain = 0.80f;
    float adsrRelease = 0.30f;
};

struct PluginState
{
    // Mix of persisted recovery fields + runtime plugin host fields.
    juce::String searchPath;
    juce::String lastPluginName;
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

struct InputState
{
    // Persisted layout identity + runtime input activity snapshot.
    juce::String layoutId { "default.freepiano.minimal" };
    KeyboardLayout keyboardLayout = makeDefaultKeyboardLayout();
    int openMidiInputCount = 0;
    int midiActivityCount = 0;
    juce::String lastMidiMessage;
};

struct AppState
{
    AudioState audio;
    PerformanceState performance;
    PluginState plugin;
    InputState input;
};
}
