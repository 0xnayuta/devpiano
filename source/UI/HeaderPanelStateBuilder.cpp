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

HeaderPanel::AudioStatus buildHeaderPanelAudioStatus(const devpiano::core::AudioState& audioState)
{
    auto summary = juce::String("Audio: ");
    summary << (audioState.backendName.isNotEmpty() ? audioState.backendName : "(no backend)");
    summary << " / " << (audioState.deviceName.isNotEmpty() ? audioState.deviceName : "(no device)");

    if (audioState.sampleRate > 0.0)
        summary << " @ " << juce::String(audioState.sampleRate, 0) << " Hz";

    if (audioState.bufferSize > 0)
        summary << " / " << juce::String(audioState.bufferSize) << " smp";

    if (audioState.availableBufferSizesText.isNotEmpty())
        summary << " | Buffers: " << audioState.availableBufferSizesText;

    if (audioState.restoreOutcome.isNotEmpty())
    {
        summary << " | Restore: " << audioState.restoreOutcome;

        if (audioState.mismatchReasons.isNotEmpty())
            summary << " (" << audioState.mismatchReasons << ")";
    }

    return { .summary = summary };
}

HeaderPanel::AudioStatus buildHeaderPanelAudioStatus(const devpiano::core::AppState& appState)
{
    return buildHeaderPanelAudioStatus(appState.audio);
}
