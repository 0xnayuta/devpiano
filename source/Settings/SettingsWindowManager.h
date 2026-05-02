#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

class SettingsComponent;

namespace devpiano::settings
{

class SettingsWindowManager final
{
public:
    struct ShowOptions
    {
        juce::Component& parent;
        juce::AudioDeviceManager& deviceManager;
        const juce::XmlElement* savedAudioDeviceState = nullptr;
        std::function<void()> onSaveRequested;
        std::function<void()> onClosed;
    };

    SettingsWindowManager();
    ~SettingsWindowManager();

    void show(ShowOptions options);
    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool isOpen() const;
    void close();
    void closeAsync();
    void saveAndClose();

private:
    struct State;
    std::shared_ptr<State> state;

    [[nodiscard]] SettingsComponent* getSettingsContent() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindowManager)
};

} // namespace devpiano::settings
