#pragma once

#include "Plugin/PluginHost.h"
#include "UI/PluginTypes.h"
#include <juce_core/juce_core.h>

[[nodiscard]] devpiano::ui::PluginPanelState
buildPluginPanelState(const PluginHost& pluginHost, const juce::String& lastPluginName, bool isEditorOpen);
