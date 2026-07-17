#pragma once

#include <JuceHeader.h>

#include "Core/AppState.h"
#include "UI/HeaderPanel.h"

[[nodiscard]] HeaderPanel::AudioStatus buildHeaderPanelAudioStatus(const devpiano::core::AudioState& audioState);
[[nodiscard]] HeaderPanel::AudioStatus buildHeaderPanelAudioStatus(const devpiano::core::AppState& appState);
