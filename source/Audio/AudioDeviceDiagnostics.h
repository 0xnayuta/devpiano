#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace devpiano::audio
{
struct SavedAudioDeviceState
{
    bool hasSavedState = false;
    juce::String deviceType;
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 0.0;
    int bufferSize = 0;
};

struct LiveAudioDeviceState
{
    bool hasLiveDevice = false;
    juce::String backendName;
    juce::String deviceName;
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 0.0;
    int bufferSize = 0;
    int defaultBufferSize = 0;
    juce::Array<int> availableBufferSizes;
};

struct AudioDeviceDiagnostics
{
    SavedAudioDeviceState saved;
    LiveAudioDeviceState live;
    juce::String restoreOutcome;
    juce::String mismatchReasons;
    juce::String compactSummary;
    juce::String detailedSummary;
};

[[nodiscard]] inline juce::String formatBufferSizes(const juce::Array<int>& sizes)
{
    if (sizes.isEmpty())
        return "(none reported)";

    juce::StringArray parts;
    for (const auto size : sizes)
        parts.add(juce::String(size));

    return parts.joinIntoString(", ");
}

[[nodiscard]] inline SavedAudioDeviceState parseSavedAudioDeviceState(const juce::XmlElement* state)
{
    if (state == nullptr)
        return {};

    return { .hasSavedState = true,
             .deviceType = state->getStringAttribute("deviceType"),
             .inputDeviceName = state->getStringAttribute("audioInputDeviceName"),
             .outputDeviceName = state->getStringAttribute("audioOutputDeviceName"),
             .sampleRate = state->getDoubleAttribute("audioDeviceRate", 0.0),
             .bufferSize = state->getIntAttribute("audioDeviceBufferSize", 0) };
}

[[nodiscard]] inline LiveAudioDeviceState captureLiveAudioDeviceState(const juce::AudioDeviceManager& deviceManager)
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    LiveAudioDeviceState live;
    live.backendName = deviceManager.getCurrentAudioDeviceType();
    live.inputDeviceName = setup.inputDeviceName;
    live.outputDeviceName = setup.outputDeviceName;
    live.sampleRate = setup.sampleRate;
    live.bufferSize = setup.bufferSize;

    if (const auto* currentDevice = deviceManager.getCurrentAudioDevice())
    {
        auto* device = const_cast<juce::AudioIODevice*>(currentDevice);
        live.hasLiveDevice = true;
        live.backendName = device->getTypeName().isNotEmpty() ? device->getTypeName()
                                                              : live.backendName;
        live.deviceName = device->getName();
        live.sampleRate = device->getCurrentSampleRate();
        live.bufferSize = device->getCurrentBufferSizeSamples();
        live.defaultBufferSize = device->getDefaultBufferSize();
        live.availableBufferSizes = device->getAvailableBufferSizes();

        if (live.outputDeviceName.isEmpty())
            live.outputDeviceName = device->getName();
    }
    else
    {
        live.deviceName = live.outputDeviceName.isNotEmpty() ? live.outputDeviceName
                                                             : live.inputDeviceName;
    }

    return live;
}

[[nodiscard]] inline bool sampleRatesMatch(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 0.5;
}

[[nodiscard]] inline AudioDeviceDiagnostics buildAudioDeviceDiagnostics(const SavedAudioDeviceState& saved,
                                                                        const LiveAudioDeviceState& live)
{
    AudioDeviceDiagnostics diagnostics;
    diagnostics.saved = saved;
    diagnostics.live = live;

    juce::StringArray mismatchReasons;

    if (! saved.hasSavedState)
    {
        diagnostics.restoreOutcome = "none";
    }
    else if (! live.hasLiveDevice)
    {
        diagnostics.restoreOutcome = "fallback suspected";
        mismatchReasons.add("no live device");
    }
    else
    {
        if (saved.deviceType.isNotEmpty() && live.backendName.isNotEmpty()
            && ! saved.deviceType.equalsIgnoreCase(live.backendName))
            mismatchReasons.add("backend");

        const auto liveDeviceName = live.outputDeviceName.isNotEmpty() ? live.outputDeviceName
                                                                       : live.deviceName;
        if (saved.outputDeviceName.isNotEmpty() && liveDeviceName.isNotEmpty()
            && ! saved.outputDeviceName.equalsIgnoreCase(liveDeviceName))
            mismatchReasons.add("output device");

        if (saved.sampleRate > 0.0 && live.sampleRate > 0.0
            && ! sampleRatesMatch(saved.sampleRate, live.sampleRate))
            mismatchReasons.add("sample rate");

        if (saved.bufferSize > 0 && live.bufferSize > 0 && saved.bufferSize != live.bufferSize)
            mismatchReasons.add("buffer size");

        if (mismatchReasons.isEmpty())
            diagnostics.restoreOutcome = "exact";
        else if (mismatchReasons.contains("backend") || mismatchReasons.contains("output device"))
            diagnostics.restoreOutcome = "fallback suspected";
        else
            diagnostics.restoreOutcome = "adjusted";
    }

    diagnostics.mismatchReasons = mismatchReasons.joinIntoString(", ");

    auto compact = juce::String("Audio: ");
    compact << (live.backendName.isNotEmpty() ? live.backendName : "(no backend)");
    compact << " / " << (live.deviceName.isNotEmpty() ? live.deviceName : "(no device)");

    if (live.sampleRate > 0.0)
        compact << " @ " << juce::String(live.sampleRate, 0) << " Hz";

    if (live.bufferSize > 0)
        compact << " / " << juce::String(live.bufferSize) << " smp";

    compact << " | Buffers: " << formatBufferSizes(live.availableBufferSizes);
    compact << " | Restore: " << diagnostics.restoreOutcome;

    if (diagnostics.mismatchReasons.isNotEmpty())
        compact << " (" << diagnostics.mismatchReasons << ")";

    diagnostics.compactSummary = compact;

    juce::String detailed;
    detailed << "Saved state: " << (saved.hasSavedState ? "yes" : "no") << "\n";
    detailed << "Saved backend: " << (saved.deviceType.isNotEmpty() ? saved.deviceType : "(none)") << "\n";
    detailed << "Saved output: " << (saved.outputDeviceName.isNotEmpty() ? saved.outputDeviceName : "(none)") << "\n";
    detailed << "Saved input: " << (saved.inputDeviceName.isNotEmpty() ? saved.inputDeviceName : "(none)") << "\n";
    detailed << "Saved rate/buffer: "
             << (saved.sampleRate > 0.0 ? juce::String(saved.sampleRate, 0) + " Hz" : juce::String("(none)"))
             << " / "
             << (saved.bufferSize > 0 ? juce::String(saved.bufferSize) + " samples" : juce::String("(none)"))
             << "\n";
    detailed << "Live backend: " << (live.backendName.isNotEmpty() ? live.backendName : "(none)") << "\n";
    detailed << "Live device: " << (live.deviceName.isNotEmpty() ? live.deviceName : "(none)") << "\n";
    detailed << "Live output: " << (live.outputDeviceName.isNotEmpty() ? live.outputDeviceName : "(none)") << "\n";
    detailed << "Live input: " << (live.inputDeviceName.isNotEmpty() ? live.inputDeviceName : "(none)") << "\n";
    detailed << "Live rate/buffer: "
             << (live.sampleRate > 0.0 ? juce::String(live.sampleRate, 0) + " Hz" : juce::String("(none)"))
             << " / "
             << (live.bufferSize > 0 ? juce::String(live.bufferSize) + " samples" : juce::String("(none)"))
             << "\n";
    detailed << "Default buffer: " << (live.defaultBufferSize > 0 ? juce::String(live.defaultBufferSize) : juce::String("(none)")) << "\n";
    detailed << "Available buffer sizes: " << formatBufferSizes(live.availableBufferSizes) << "\n";
    detailed << "Restore outcome: " << diagnostics.restoreOutcome;

    if (diagnostics.mismatchReasons.isNotEmpty())
        detailed << "\nMismatch reasons: " << diagnostics.mismatchReasons;

    diagnostics.detailedSummary = detailed;
    return diagnostics;
}

[[nodiscard]] inline AudioDeviceDiagnostics buildAudioDeviceDiagnostics(const juce::XmlElement* savedState,
                                                                        const juce::AudioDeviceManager& deviceManager)
{
    return buildAudioDeviceDiagnostics(parseSavedAudioDeviceState(savedState),
                                       captureLiveAudioDeviceState(deviceManager));
}
} // namespace devpiano::audio
