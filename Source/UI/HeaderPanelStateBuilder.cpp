#include "HeaderPanelStateBuilder.h"

HeaderPanel::MidiStatus buildHeaderPanelMidiStatus(int openInputCount,
                                                   int activityCount,
                                                   const juce::String& lastMessage)
{
    return { .openInputCount = openInputCount,
             .activityCount = activityCount,
             .lastMessage = lastMessage };
}

HeaderPanel::MidiStatus buildHeaderPanelMidiStatus(const devpiano::core::InputState& inputState)
{
    return buildHeaderPanelMidiStatus(inputState.openMidiInputCount,
                                      inputState.midiActivityCount,
                                      inputState.lastMidiMessage);
}

HeaderPanel::MidiStatus buildHeaderPanelMidiStatus(const devpiano::core::AppState& appState)
{
    return buildHeaderPanelMidiStatus(appState.input);
}
