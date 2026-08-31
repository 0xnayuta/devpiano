#pragma once

#include "Settings/SettingsModel.h"
#include "Settings/jive/SettingsLayoutModel.h"
#include <array>
#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

// ============================================================================
/// Settings Window Content Component
///
/// Uses JIVE declarative layout model (SettingsLayoutModel) and CSS Grid
/// for 16-channel follow key toggles.
// ============================================================================
class SettingsComponent : public juce::Component, private juce::ChangeListener, public juce::ValueTree::Listener {
public:
    explicit SettingsComponent(juce::AudioDeviceManager& dm, const juce::XmlElement* savedAudioDeviceState,
                               SettingsModel* displayModel = nullptr);
    ~SettingsComponent() override;

    void buildJiveUi();
    void rebuildColourModeCombo();
    void rebuildNoteDisplayCombo();
    void rebuildKeySignatureCombo();
    void refreshTexts();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    [[nodiscard]] bool isDirty() const noexcept {
        return dirty;
    }
    void setDirty(bool d) noexcept {
        dirty = d;
    }

    std::function<void()> onSaveRequested;
    std::function<void()> onDisplaySettingsChanged;
    std::function<void(const juce::String&)> onLanguageChanged;
    std::function<void()> onRefreshTexts;

private:
    void wireAudioControls();
    void wireMidiControls();
    void wireAppearanceAndLocaleControls();
    void syncEditingStateFromModel();

    void populateAudioDeviceTypes();
    void populateAudioOutputDevices();
    void populateAudioActiveChannels();
    void populateAudioSampleRates();
    void populateAudioBufferSizes();
    void updateAsioControlPanelVisibility();
    void refreshAllAudioControls();

    static constexpr std::array<int, 13> comboKeyMapping { 0, 0, 1, 2, 3, 4, 5, 6, -5, -4, -3, -2, -1 };
    [[nodiscard]] static int keySignatureToComboId(int ks);
    [[nodiscard]] int calculateSettingsContentHeight() const {
        return devpiano::ui::jive::kSettingsLayoutContentHeight;
    }
    void updateContentBounds();
    void updateFollowKeyTogglesEnablement();
    void updateDiagnostics();

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override;

    juce::AudioDeviceManager& deviceManager;
    SettingsModel* model = nullptr;

    juce::Viewport viewport;
    std::unique_ptr<::jive::Interpreter> interpreter;
    std::unique_ptr<::jive::GuiItem> jiveRootItem;

    juce::ComboBox* audioDeviceTypeCombo = nullptr;
    juce::ComboBox* audioOutputDeviceCombo = nullptr;
    juce::ComboBox* audioActiveChannelsCombo = nullptr;
    juce::Button* audioTestButton = nullptr;
    juce::ComboBox* audioSampleRateCombo = nullptr;
    juce::ComboBox* audioBufferSizeCombo = nullptr;
    juce::Button* asioControlPanelButton = nullptr;
    ::jive::GuiItem* asioControlPanelRowItem = nullptr;
    bool isUpdatingAudioControls = false;

    juce::ComboBox* keySignatureCombo = nullptr;
    juce::ToggleButton* midiTransposeToggle = nullptr;
    std::array<juce::ToggleButton*, 16> followKeyToggles {};

    juce::ComboBox* colourModeCombo = nullptr;
    juce::ComboBox* noteDisplayCombo = nullptr;
    juce::Slider* fadeSpeedSlider = nullptr;
    juce::ToggleButton* instrumentFilterToggle = nullptr;
    juce::ComboBox* languageCombo = nullptr;
    juce::TextEditor* diagnosticsEditor = nullptr;
    juce::Button* saveButton = nullptr;
    ::jive::GuiItem* followKeyAreaItem = nullptr;

    std::unique_ptr<juce::XmlElement> savedStateSnapshot;
    bool dirty = false;
    juce::ValueTree editingState { "Settings" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};
