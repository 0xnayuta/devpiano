#pragma once

#include <JuceHeader.h>

#include "Core/AppState.h"
#include "Plugin/PluginHost.h"
#include "UI/PluginPanel.h"

[[nodiscard]] PluginPanel::State buildPluginPanelState(const PluginHost& pluginHost,
                                                       const juce::String& lastPluginName,
                                                       bool isEditorOpen);
[[nodiscard]] PluginPanel::State buildPluginPanelState(const devpiano::core::PluginState& pluginState);
[[nodiscard]] PluginPanel::State buildPluginPanelState(const devpiano::core::AppState& appState);
