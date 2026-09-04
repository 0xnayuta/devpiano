#include "Audio/AudioDeviceDiagnostics.h"

#include <cmath>

namespace devpiano::audio {
namespace {

void collectMismatchReasons(const SavedAudioDeviceState& saved, const LiveAudioDeviceState& live,
                            juce::StringArray& mismatchReasons, juce::String& restoreOutcome) {
    if (!saved.hasSavedDeviceStateXml) {
        restoreOutcome = "none";
        return;
    }

    if (!live.hasLiveDevice) {
        restoreOutcome = "fallback suspected";
        mismatchReasons.add("no live device");
        return;
    }

    if (saved.deviceType.isNotEmpty() && live.backendName.isNotEmpty()
        && !saved.deviceType.equalsIgnoreCase(live.backendName)) {
        mismatchReasons.add("backend");
    }

    const auto liveDeviceName = live.outputDeviceName.isNotEmpty() ? live.outputDeviceName : live.deviceName;
    if (saved.outputDeviceName.isNotEmpty() && liveDeviceName.isNotEmpty()
        && !saved.outputDeviceName.equalsIgnoreCase(liveDeviceName)) {
        mismatchReasons.add("output device");
    }

    if (saved.sampleRate > 0.0 && live.sampleRate > 0.0 && !sampleRatesMatch(saved.sampleRate, live.sampleRate)) {
        mismatchReasons.add("sample rate");
    }

    if (saved.bufferSize > 0 && live.bufferSize > 0 && saved.bufferSize != live.bufferSize) {
        mismatchReasons.add("buffer size");
    }

    if (mismatchReasons.isEmpty()) {
        restoreOutcome = "exact";
    } else if (mismatchReasons.contains("backend") || mismatchReasons.contains("output device")) {
        restoreOutcome = "fallback suspected";
    } else {
        restoreOutcome = "adjusted";
    }
}

juce::String formatCompactSummary(const LiveAudioDeviceState& live, const juce::String& restoreOutcome,
                                  const juce::String& mismatchReasons) {
    auto compact = juce::String("Audio: ");
    compact << (live.backendName.isNotEmpty() ? live.backendName : "(no backend)");
    compact << " / " << (live.deviceName.isNotEmpty() ? live.deviceName : "(no device)");

    if (live.sampleRate > 0.0) {
        compact << " @ " << juce::String(live.sampleRate, 0) << " Hz";
    }

    if (live.bufferSize > 0) {
        compact << " / " << juce::String(live.bufferSize) << " smp";
    }

    compact << " | Buffers: " << formatBufferSizes(live.availableBufferSizes);
    compact << " | Restore: " << restoreOutcome;

    if (mismatchReasons.isNotEmpty()) {
        compact << " (" << mismatchReasons << ")";
    }

    return compact;
}

juce::String formatDetailedSummary(const SavedAudioDeviceState& saved, const LiveAudioDeviceState& live,
                                   const juce::String& restoreOutcome, const juce::String& mismatchReasons) {
    juce::String detailed;
    detailed << "Saved state: " << (saved.hasSavedDeviceStateXml ? "yes" : "no") << "\n";
    detailed << "Saved backend: " << (saved.deviceType.isNotEmpty() ? saved.deviceType : "(none)") << "\n";
    detailed << "Saved output: " << (saved.outputDeviceName.isNotEmpty() ? saved.outputDeviceName : "(none)") << "\n";
    detailed << "Saved input: " << (saved.inputDeviceName.isNotEmpty() ? saved.inputDeviceName : "(none)") << "\n";
    detailed << "Saved rate/buffer: "
             << (saved.sampleRate > 0.0 ? juce::String(saved.sampleRate, 0) + " Hz" : juce::String("(none)")) << " / "
             << (saved.bufferSize > 0 ? juce::String(saved.bufferSize) + " samples" : juce::String("(none)")) << "\n";
    detailed << "Live backend: " << (live.backendName.isNotEmpty() ? live.backendName : "(none)") << "\n";
    detailed << "Live device: " << (live.deviceName.isNotEmpty() ? live.deviceName : "(none)") << "\n";
    detailed << "Live output: " << (live.outputDeviceName.isNotEmpty() ? live.outputDeviceName : "(none)") << "\n";
    detailed << "Live input: " << (live.inputDeviceName.isNotEmpty() ? live.inputDeviceName : "(none)") << "\n";
    detailed << "Live rate/buffer: "
             << (live.sampleRate > 0.0 ? juce::String(live.sampleRate, 0) + " Hz" : juce::String("(none)")) << " / "
             << (live.bufferSize > 0 ? juce::String(live.bufferSize) + " samples" : juce::String("(none)")) << "\n";
    detailed << "Default buffer: "
             << (live.defaultBufferSize > 0 ? juce::String(live.defaultBufferSize) : juce::String("(none)")) << "\n";
    detailed << "Available buffer sizes: " << formatBufferSizes(live.availableBufferSizes) << "\n";
    detailed << "Restore outcome: " << restoreOutcome;

    if (mismatchReasons.isNotEmpty()) {
        detailed << "\nMismatch reasons: " << mismatchReasons;
    }

    return detailed;
}

} // namespace

juce::String formatBufferSizes(const juce::Array<int>& sizes) {
    if (sizes.isEmpty()) {
        return "(none reported)";
    }

    juce::StringArray parts;
    for (const auto size : sizes) {
        parts.add(juce::String(size));
    }

    return parts.joinIntoString(", ");
}

SavedAudioDeviceState parseSavedAudioDeviceState(const juce::XmlElement* state) {
    if (state == nullptr) {
        return {};
    }

    return { .hasSavedDeviceStateXml = true,
             .deviceType = state->getStringAttribute("deviceType"),
             .inputDeviceName = state->getStringAttribute("audioInputDeviceName"),
             .outputDeviceName = state->getStringAttribute("audioOutputDeviceName"),
             .sampleRate = state->getDoubleAttribute("audioDeviceRate", 0.0),
             .bufferSize = state->getIntAttribute("audioDeviceBufferSize", 0) };
}

std::unique_ptr<juce::XmlElement> createDeviceStateXml(juce::AudioIODevice& device,
                                                       const juce::AudioDeviceManager::AudioDeviceSetup& setup) {
    auto xml = std::make_unique<juce::XmlElement>("DEVICESETUP");

    xml->setAttribute("deviceType", device.getTypeName());
    xml->setAttribute("audioOutputDeviceName", setup.outputDeviceName);
    xml->setAttribute("audioInputDeviceName", setup.inputDeviceName);
    xml->setAttribute("audioDeviceRate", device.getCurrentSampleRate());
    xml->setAttribute("audioDeviceBufferSize", device.getCurrentBufferSizeSamples());

    if (!setup.useDefaultInputChannels) {
        xml->setAttribute("audioDeviceInChans", setup.inputChannels.toString(2));
    }
    if (!setup.useDefaultOutputChannels) {
        xml->setAttribute("audioDeviceOutChans", setup.outputChannels.toString(2));
    }

    return xml;
}

LiveAudioDeviceState captureLiveAudioDeviceState(const juce::AudioDeviceManager& deviceManager) {
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    LiveAudioDeviceState live;
    live.backendName = deviceManager.getCurrentAudioDeviceType();
    live.inputDeviceName = setup.inputDeviceName;
    live.outputDeviceName = setup.outputDeviceName;
    live.sampleRate = setup.sampleRate;
    live.bufferSize = setup.bufferSize;

    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        live.hasLiveDevice = true;
        live.backendName = device->getTypeName().isNotEmpty() ? device->getTypeName() : live.backendName;
        live.deviceName = device->getName();
        live.sampleRate = device->getCurrentSampleRate();
        live.bufferSize = device->getCurrentBufferSizeSamples();
        live.defaultBufferSize = device->getDefaultBufferSize();
        live.availableBufferSizes = device->getAvailableBufferSizes();

        if (live.outputDeviceName.isEmpty()) {
            live.outputDeviceName = device->getName();
        }
    } else {
        live.deviceName = live.outputDeviceName.isNotEmpty() ? live.outputDeviceName : live.inputDeviceName;
    }

    return live;
}

bool sampleRatesMatch(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 0.5;
}

AudioDeviceDiagnostics buildAudioDeviceDiagnostics(const SavedAudioDeviceState& saved,
                                                   const LiveAudioDeviceState& live) {
    AudioDeviceDiagnostics diagnostics;
    diagnostics.saved = saved;
    diagnostics.live = live;

    juce::StringArray mismatchReasons;
    collectMismatchReasons(saved, live, mismatchReasons, diagnostics.restoreOutcome);

    diagnostics.mismatchReasons = mismatchReasons.joinIntoString(", ");
    diagnostics.compactSummary = formatCompactSummary(live, diagnostics.restoreOutcome, diagnostics.mismatchReasons);
    diagnostics.detailedSummary
        = formatDetailedSummary(saved, live, diagnostics.restoreOutcome, diagnostics.mismatchReasons);

    return diagnostics;
}

AudioDeviceDiagnostics buildAudioDeviceDiagnostics(const juce::XmlElement* savedState,
                                                   const juce::AudioDeviceManager& deviceManager) {
    return buildAudioDeviceDiagnostics(parseSavedAudioDeviceState(savedState),
                                       captureLiveAudioDeviceState(deviceManager));
}

} // namespace devpiano::audio
