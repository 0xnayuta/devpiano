#pragma once

#include <JuceHeader.h>

#include "Plugin/PluginHost.h"
#include "UI/PluginPanel.h"

[[nodiscard]] PluginPanel::State buildPluginPanelState(const PluginHost& pluginHost,
                                                       const juce::String& lastPluginName,
                                                       bool isEditorOpen);
