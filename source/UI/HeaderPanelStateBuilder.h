#pragma once

#include <JuceHeader.h>

#include "Core/AppState.h"
#include "UI/HeaderPanel.h"

[[nodiscard]] HeaderPanel::MidiStatus buildHeaderPanelMidiStatus(int openInputCount,
                                                                 int activityCount,
                                                                 const juce::String& lastMessage);
[[nodiscard]] HeaderPanel::MidiStatus buildHeaderPanelMidiStatus(const devpiano::core::InputState& inputState);
[[nodiscard]] HeaderPanel::MidiStatus buildHeaderPanelMidiStatus(const devpiano::core::AppState& appState);
