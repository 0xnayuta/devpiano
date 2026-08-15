#pragma once

#include <JuceHeader.h>

#include "Plugin/PluginHost.h"
#include "UI/PluginTypes.h"

[[nodiscard]] devpiano::ui::PluginPanelState
buildPluginPanelState(const PluginHost& pluginHost, const juce::String& lastPluginName, bool isEditorOpen);
