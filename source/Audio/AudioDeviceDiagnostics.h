#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <memory>

namespace devpiano::audio {

struct SavedAudioDeviceState {
    bool hasSavedDeviceStateXml = false;
    juce::String deviceType;
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 0.0;
    int bufferSize = 0;
};

struct LiveAudioDeviceState {
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

struct AudioDeviceDiagnostics {
    SavedAudioDeviceState saved;
    LiveAudioDeviceState live;
    juce::String restoreOutcome;
    juce::String mismatchReasons;
    juce::String compactSummary;
    juce::String detailedSummary;
};

[[nodiscard]] juce::String formatBufferSizes(const juce::Array<int>& sizes);

[[nodiscard]] SavedAudioDeviceState parseSavedAudioDeviceState(const juce::XmlElement* state);

// 构造与 JUCE AudioDeviceManager::updateXml() 同款格式的 DEVICESETUP XML。
// JUCE v8/v9 中 initialise/initialiseDefault 路径（treatAsChosenDevice=false）不维护
// lastExplicitSettings，createStateXml() 恒为 null；此函数供默认设备启动路径
// 手工构造可持久化/可恢复的设备状态（恢复端 initialiseFromXML 消费同款属性）。
[[nodiscard]] std::unique_ptr<juce::XmlElement>
createDeviceStateXml(juce::AudioIODevice& device, const juce::AudioDeviceManager::AudioDeviceSetup& setup);

[[nodiscard]] LiveAudioDeviceState captureLiveAudioDeviceState(const juce::AudioDeviceManager& deviceManager);

[[nodiscard]] bool sampleRatesMatch(double lhs, double rhs);

[[nodiscard]] AudioDeviceDiagnostics buildAudioDeviceDiagnostics(const SavedAudioDeviceState& saved,
                                                                 const LiveAudioDeviceState& live);

[[nodiscard]] AudioDeviceDiagnostics buildAudioDeviceDiagnostics(const juce::XmlElement* savedState,
                                                                 const juce::AudioDeviceManager& deviceManager);

} // namespace devpiano::audio
