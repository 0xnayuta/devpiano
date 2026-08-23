#pragma once

#include <juce_core/juce_core.h>
// ============================================================================
// 插件面板视图模型状态。
//
// 原为 PluginPanel::State；Phase 11d 删除 PluginPanel 组件类后抽出，
// 使 PluginPanelStateBuilder 不再依赖该组件类。与 KeyboardTypes.h 等
// UI 状态类型一致，置于 devpiano::ui 命名空间。
// ============================================================================

namespace devpiano::ui {

struct PluginPanelState {
    juce::StringArray availablePluginNames;
    juce::StringArray instrumentPluginNames;
    juce::StringArray effectPluginNames;
    juce::String preferredSelection;
    juce::String pluginListText;
    juce::String availableFormatsDescription;
    juce::String lastScanSummary;
    juce::String currentPluginName;
    juce::String lastLoadError;
    juce::String lastPluginName;
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    bool supportsVst3 = false;
    bool hasLoadedPlugin = false;
    bool isPrepared = false;
    bool isEditorOpen = false;
    bool isCurrentlyScanning = false;
    int scanPluginCount = 0;
    int scanFailedCount = 0;
    juce::String scanningPluginName;
};

} // namespace devpiano::ui
