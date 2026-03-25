#pragma once
#include <JuceHeader.h>

class SettingsComponent : public juce::Component, private juce::ChangeListener
{
public:
    explicit SettingsComponent(juce::AudioDeviceManager& dm)
        : deviceManager(dm)
    {
        selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager,
            0, 2,
            0, 2,
            false,
            false,
            true,
            false);

        addAndMakeVisible(selector.get());

        saveButton.setButtonText("Save");
        addAndMakeVisible(saveButton);
        saveButton.onClick = [this]
        {
            dirty = false;
            if (onSaveRequested)
            {
                auto callback = onSaveRequested;
                juce::MessageManager::callAsync([callback] { callback(); });
            }
        };

        deviceManager.addChangeListener(this);
        setSize(560, 420);
    }

    ~SettingsComponent() override
    {
        deviceManager.removeChangeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto bottom = area.removeFromBottom(36);
        saveButton.setBounds(bottom.removeFromRight(120).reduced(4));
        selector->setBounds(area);
    }

    // dirty tracking
    bool isDirty() const noexcept { return dirty; }
    void setDirty(bool d) noexcept { dirty = d; }

    std::function<void()> onSaveRequested;

private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    juce::TextButton saveButton;

    bool dirty = false;

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        dirty = true;
    }
};
