#pragma once
#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"

class SettingsComponent : public juce::Component, private juce::ChangeListener
{
public:
    explicit SettingsComponent(juce::AudioDeviceManager& dm,
                               const juce::XmlElement* savedAudioDeviceState)
        : deviceManager(dm)
    {
        if (savedAudioDeviceState != nullptr)
            savedStateSnapshot = std::make_unique<juce::XmlElement>(*savedAudioDeviceState);

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

        diagnosticsEditor.setMultiLine(true);
        diagnosticsEditor.setReadOnly(true);
        diagnosticsEditor.setScrollbarsShown(true);
        diagnosticsEditor.setCaretVisible(false);
        diagnosticsEditor.setPopupMenuEnabled(true);
        diagnosticsEditor.setWantsKeyboardFocus(false);
        diagnosticsEditor.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(diagnosticsEditor);

        deviceManager.addChangeListener(this);
        updateDiagnostics();
        setSize(560, 520);
    }

    ~SettingsComponent() override
    {
        deviceManager.removeChangeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto bottomArea = area.removeFromBottom(120);
        auto buttonRow = bottomArea.removeFromBottom(36);
        diagnosticsEditor.setBounds(bottomArea);
        saveButton.setBounds(buttonRow.removeFromRight(120).reduced(4));
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
    juce::TextEditor diagnosticsEditor;
    std::unique_ptr<juce::XmlElement> savedStateSnapshot;

    bool dirty = false;

    void updateDiagnostics()
    {
        const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(savedStateSnapshot.get(), deviceManager);
        diagnosticsEditor.setText(diagnostics.detailedSummary, juce::dontSendNotification);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        dirty = true;
        updateDiagnostics();
    }
};
