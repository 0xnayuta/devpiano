#pragma once

#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Locale/LocaleManager.h"
#include "Settings/SettingsModel.h"
#include "Settings/jive/SettingsLayoutModel.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/StyleCatalog.h"

// ============================================================================
// Helper functions for safe JIVE Component Tree traversal and cleanup
// ============================================================================

namespace {

inline juce::Component* findComponentById(::jive::GuiItem& root, const juce::String& id) {
    if (root.state.getProperty("id").toString() == id) {
        return root.getComponent().get();
    }
    for (auto* child : root.getChildren()) {
        if (auto* found = findComponentById(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

inline void clearJiveStyleSheets(juce::Component* comp) {
    if (comp == nullptr) {
        return;
    }
    for (int i = 0; i < comp->getNumChildComponents(); ++i) {
        clearJiveStyleSheets(comp->getChildComponent(i));
    }
    if (comp->getProperties().contains("style-sheet")) {
        comp->getProperties().remove("style-sheet");
    }
}

inline void collectJiveComponents(::jive::GuiItem& item, std::vector<std::shared_ptr<juce::Component>>& components) {
    if (auto component = item.getComponent()) {
        components.push_back(std::move(component));
    }
    for (auto* child : item.getChildren()) {
        collectJiveComponents(*child, components);
    }
}

inline void safeCleanupJiveTree(std::unique_ptr<::jive::GuiItem>& rootItem) {
    if (rootItem != nullptr) {
        std::vector<std::shared_ptr<juce::Component>> jiveComponents;
        collectJiveComponents(*rootItem, jiveComponents);
        clearJiveStyleSheets(rootItem->getComponent().get());
        rootItem.reset();
    }
}

} // namespace

// ============================================================================
/// Settings Window Content Component
///
/// Refactored in Phase 15-C to use JIVE's declarative layout model
/// (SettingsLayoutModel) and CSS Grid for 16-channel follow key toggles,
/// eliminating 300+ lines of manual setBounds/removeFromTop calculations.
// ============================================================================
class SettingsComponent : public juce::Component, private juce::ChangeListener, public juce::ValueTree::Listener {
public:
    explicit SettingsComponent(juce::AudioDeviceManager& dm, const juce::XmlElement* savedAudioDeviceState,
                               SettingsModel* displayModel = nullptr)
        : deviceManager(dm)
        , model(displayModel) {
        if (savedAudioDeviceState != nullptr) {
            savedStateSnapshot = std::make_unique<juce::XmlElement>(*savedAudioDeviceState);
        }

        buildJiveUi();

        viewport.setViewedComponent(jiveRootItem != nullptr ? jiveRootItem->getComponent().get() : nullptr, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        setSize(560, 800);
        deviceManager.addChangeListener(this);

        updateDiagnostics();
    }

    ~SettingsComponent() override {
        deviceManager.removeChangeListener(this);
        safeCleanupJiveTree(jiveRootItem);
        interpreter.reset();
    }

    void buildJiveUi() {
        safeCleanupJiveTree(jiveRootItem);
        interpreter = std::make_unique<::jive::Interpreter>();
        auto& factory = interpreter->getComponentFactory();

        factory.set("AudioDeviceSelector", [this] {
            return std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 0, 2, false, false, true,
                                                                        false);
        });

        factory.set("ListEditor", [] {
            auto ed = std::make_unique<juce::TextEditor>();
            ed->setMultiLine(true);
            ed->setReadOnly(true);
            ed->setScrollbarsShown(true);
            ed->setCaretVisible(false);
            ed->setPopupMenuEnabled(true);
            ed->setWantsKeyboardFocus(false);
            ed->setMouseClickGrabsKeyboardFocus(false);
            return ed;
        });

        auto layoutTree = devpiano::ui::jive::makeSettingsLayoutTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(layoutTree);

        jiveRootItem = interpreter->interpret(layoutTree);
        jassert(jiveRootItem != nullptr);

        if (jiveRootItem != nullptr) {
            // Find component references
            keySignatureCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "key-signature-combo"));
            midiTransposeToggle
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*jiveRootItem, "midi-transpose-toggle"));
            colourModeCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "colour-mode-combo"));
            noteDisplayCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "note-display-combo"));
            fadeSpeedSlider = dynamic_cast<juce::Slider*>(findComponentById(*jiveRootItem, "fade-speed-slider"));
            resizableToggle = dynamic_cast<juce::ToggleButton*>(findComponentById(*jiveRootItem, "resizable-toggle"));
            instrumentFilterToggle
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*jiveRootItem, "instrument-filter-toggle"));
            languageCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "language-combo"));
            diagnosticsEditor = dynamic_cast<juce::TextEditor*>(findComponentById(*jiveRootItem, "diagnostics-editor"));
            saveButton = dynamic_cast<juce::Button*>(findComponentById(*jiveRootItem, "save-button"));
            followKeyAreaItem = devpiano::ui::jive::findGuiItemById(*jiveRootItem, "channel-follow-key-area");

            for (int ch = 0; ch < 16; ++ch) {
                followKeyToggles[static_cast<size_t>(ch)] = dynamic_cast<juce::ToggleButton*>(
                    findComponentById(*jiveRootItem, "follow-key-" + juce::String(ch)));
            }

            // Populate & wire Key Signature
            if (keySignatureCombo != nullptr) {
                rebuildKeySignatureCombo();
                if (model != nullptr) {
                    keySignatureCombo->setSelectedId(keySignatureToComboId(model->keySignature),
                                                     juce::dontSendNotification);
                }
                keySignatureCombo->onChange = [this] {
                    auto ks = comboKeyMapping[static_cast<size_t>(keySignatureCombo->getSelectedId())];
                    editingState.setProperty("keySignature", ks, nullptr);
                };
            }

            // Wire MIDI Transpose toggle
            if (midiTransposeToggle != nullptr) {
                if (model != nullptr) {
                    midiTransposeToggle->setToggleState(model->midiTranspose, juce::dontSendNotification);
                }
                midiTransposeToggle->onStateChange = [this] {
                    editingState.setProperty("midiTranspose", midiTransposeToggle->getToggleState(), nullptr);
                };
            }

            // Wire 16 Channel Follow Key toggles
            for (int ch = 0; ch < 16; ++ch) {
                if (auto* tb = followKeyToggles[static_cast<size_t>(ch)]) {
                    if (model != nullptr) {
                        tb->setToggleState(model->channelMatrix.channels[static_cast<size_t>(ch)].followKey,
                                           juce::dontSendNotification);
                    }
                    tb->onStateChange = [this, ch] {
                        editingState.setProperty("followKey_" + juce::String(ch),
                                                 followKeyToggles[static_cast<size_t>(ch)]->getToggleState(), nullptr);
                    };
                }
            }
            updateFollowKeyTogglesVisibility();

            // Populate & wire Colour Mode
            if (colourModeCombo != nullptr) {
                rebuildColourModeCombo();
                if (model != nullptr) {
                    colourModeCombo->setSelectedId(1 + static_cast<int>(model->keyboardDisplay.colourMode),
                                                   juce::dontSendNotification);
                }
                colourModeCombo->onChange
                    = [this] { editingState.setProperty("colourMode", colourModeCombo->getSelectedId(), nullptr); };
            }

            // Populate & wire Note Display
            if (noteDisplayCombo != nullptr) {
                rebuildNoteDisplayCombo();
                if (model != nullptr) {
                    noteDisplayCombo->setSelectedId(1 + static_cast<int>(model->keyboardDisplay.noteDisplay),
                                                    juce::dontSendNotification);
                }
                noteDisplayCombo->onChange
                    = [this] { editingState.setProperty("noteDisplay", noteDisplayCombo->getSelectedId(), nullptr); };
            }

            // Configure & wire Fade Speed Slider
            if (fadeSpeedSlider != nullptr) {
                fadeSpeedSlider->setRange(0.50, 1.00, 0.01);
                fadeSpeedSlider->setSliderStyle(juce::Slider::LinearHorizontal);
                fadeSpeedSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
                if (model != nullptr) {
                    fadeSpeedSlider->setValue(model->keyboardDisplay.fadeSpeed, juce::dontSendNotification);
                }
                fadeSpeedSlider->onValueChange
                    = [this] { editingState.setProperty("fadeSpeed", fadeSpeedSlider->getValue(), nullptr); };
            }

            // Wire Resizable Window Toggle
            if (resizableToggle != nullptr) {
                if (model != nullptr) {
                    resizableToggle->setToggleState(model->keyboardDisplay.resizableWindow, juce::dontSendNotification);
                }
                resizableToggle->onStateChange = [this] {
                    editingState.setProperty("resizableWindow", resizableToggle->getToggleState(), nullptr);
                };
            }

            // Wire Instrument Filter Toggle
            if (instrumentFilterToggle != nullptr) {
                if (model != nullptr) {
                    instrumentFilterToggle->setToggleState(model->keyboardDisplay.showInstrumentFilter,
                                                           juce::dontSendNotification);
                }
                instrumentFilterToggle->onStateChange = [this] {
                    editingState.setProperty("showInstrumentFilter", instrumentFilterToggle->getToggleState(), nullptr);
                };
            }

            // Populate & wire Language Combo
            if (languageCombo != nullptr) {
                languageCombo->clear(juce::dontSendNotification);
                languageCombo->addItem("English", 1);
                languageCombo->addItem(devpiano::locale::languageDisplayName(devpiano::locale::Language::zhCN), 2);
                if (model != nullptr) {
                    languageCombo->setSelectedId(model->languageCode == "zh-CN" ? 2 : 1, juce::dontSendNotification);
                }
                languageCombo->onChange = [this] {
                    editingState.setProperty(
                        "languageCode",
                        languageCombo->getSelectedId() == 2 ? juce::String("zh-CN") : juce::String("en"), nullptr);
                };
            }

            // Wire Save Button
            if (saveButton != nullptr) {
                saveButton->onClick = [this] {
                    dirty = false;
                    if (onSaveRequested) {
                        onSaveRequested();
                    }
                };
            }

            // Load model into editingState
            if (model != nullptr) {
                editingState.removeListener(this);
                if (colourModeCombo) {
                    editingState.setProperty("colourMode", colourModeCombo->getSelectedId(), nullptr);
                }
                if (noteDisplayCombo) {
                    editingState.setProperty("noteDisplay", noteDisplayCombo->getSelectedId(), nullptr);
                }
                if (fadeSpeedSlider) {
                    editingState.setProperty("fadeSpeed", static_cast<double>(model->keyboardDisplay.fadeSpeed),
                                             nullptr);
                }
                if (resizableToggle) {
                    editingState.setProperty("resizableWindow", model->keyboardDisplay.resizableWindow, nullptr);
                }
                if (instrumentFilterToggle) {
                    editingState.setProperty("showInstrumentFilter", model->keyboardDisplay.showInstrumentFilter,
                                             nullptr);
                }
                editingState.setProperty("languageCode", model->languageCode, nullptr);
                editingState.setProperty("keySignature", model->keySignature, nullptr);
                editingState.setProperty("midiTranspose", model->midiTranspose, nullptr);
                for (int ch = 0; ch < 16; ++ch) {
                    editingState.setProperty("followKey_" + juce::String(ch),
                                             model->channelMatrix.channels[static_cast<size_t>(ch)].followKey, nullptr);
                }
                editingState.addListener(this);
            }
        }
    }

    void rebuildColourModeCombo() {
        if (colourModeCombo == nullptr) {
            return;
        }
        colourModeCombo->clear(juce::dontSendNotification);
        colourModeCombo->addItem(TRANS("Classic"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::classic));
        colourModeCombo->addItem(TRANS("Channel"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::channel));
        colourModeCombo->addItem(TRANS("Velocity"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::velocity));
    }

    void rebuildNoteDisplayCombo() {
        if (noteDisplayCombo == nullptr) {
            return;
        }
        noteDisplayCombo->clear(juce::dontSendNotification);
        noteDisplayCombo->addItem(TRANS("Do Re Mi"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi));
        noteDisplayCombo->addItem(TRANS("Fixed Do"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::fixedDo));
        noteDisplayCombo->addItem(TRANS("Note Name"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::noteName));
    }

    void rebuildKeySignatureCombo() {
        if (keySignatureCombo == nullptr) {
            return;
        }
        keySignatureCombo->clear(juce::dontSendNotification);
        keySignatureCombo->addItem("C", 1);
        keySignatureCombo->addItem("C# / Db", 2);
        keySignatureCombo->addItem("D", 3);
        keySignatureCombo->addItem("D# / Eb", 4);
        keySignatureCombo->addItem("E", 5);
        keySignatureCombo->addItem("F", 6);
        keySignatureCombo->addItem("F# / Gb", 7);
        keySignatureCombo->addItem("G", 8);
        keySignatureCombo->addItem("G# / Ab", 9);
        keySignatureCombo->addItem("A", 10);
        keySignatureCombo->addItem("A# / Bb", 11);
        keySignatureCombo->addItem("B", 12);
    }

    void refreshTexts() {
        buildJiveUi();
        if (viewport.getViewedComponent() != (jiveRootItem != nullptr ? jiveRootItem->getComponent().get() : nullptr)) {
            viewport.setViewedComponent(jiveRootItem != nullptr ? jiveRootItem->getComponent().get() : nullptr, false);
        }
        resized();
        updateDiagnostics();
        if (onRefreshTexts) {
            onRefreshTexts();
        }
    }

    void resized() override {
        viewport.setBounds(getLocalBounds());
        if (jiveRootItem != nullptr) {
            const auto availableWidth = viewport.getMaximumVisibleWidth();
            const auto contentWidth = juce::jmax(520, availableWidth);
            if (auto rootComp = jiveRootItem->getComponent()) {
                rootComp->setBounds(0, 0, contentWidth, rootComp->getHeight());
            }
        }
    }

    bool isDirty() const noexcept {
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
    juce::AudioDeviceManager& deviceManager;
    SettingsModel* model;

    juce::Viewport viewport;
    std::unique_ptr<::jive::Interpreter> interpreter;
    std::unique_ptr<::jive::GuiItem> jiveRootItem;

    juce::ComboBox* keySignatureCombo = nullptr;
    juce::ToggleButton* midiTransposeToggle = nullptr;
    std::array<juce::ToggleButton*, 16> followKeyToggles {};

    juce::ComboBox* colourModeCombo = nullptr;
    juce::ComboBox* noteDisplayCombo = nullptr;
    juce::Slider* fadeSpeedSlider = nullptr;
    juce::ToggleButton* resizableToggle = nullptr;
    juce::ToggleButton* instrumentFilterToggle = nullptr;
    juce::ComboBox* languageCombo = nullptr;
    juce::TextEditor* diagnosticsEditor = nullptr;
    juce::Button* saveButton = nullptr;
    ::jive::GuiItem* followKeyAreaItem = nullptr;

    // Combo ID (1-12) → semitone offset from C
    static constexpr std::array<int, 13> comboKeyMapping { 0, 0, 1, 2, 3, 4, 5, 6, -5, -4, -3, -2, -1 };

    [[nodiscard]] static int keySignatureToComboId(int ks) {
        for (int id = 1; id <= 12; ++id) {
            if (comboKeyMapping[static_cast<size_t>(id)] == ks) {
                return id;
            }
        }
        return 1; // default C
    }

    std::unique_ptr<juce::XmlElement> savedStateSnapshot;
    bool dirty = false;
    juce::ValueTree editingState { "Settings" };

    void updateFollowKeyTogglesVisibility() {
        if (followKeyAreaItem != nullptr) {
            const auto visible = midiTransposeToggle != nullptr && midiTransposeToggle->getToggleState();
            followKeyAreaItem->state.setProperty("display", visible ? "flex" : "none", nullptr);
        }
        if (jiveRootItem != nullptr) {
            const auto availableWidth = viewport.getMaximumVisibleWidth();
            const auto contentWidth = juce::jmax(520, availableWidth);
            if (auto rootComp = jiveRootItem->getComponent()) {
                rootComp->setBounds(0, 0, contentWidth, rootComp->getHeight());
            }
        }
    }

    void updateDiagnostics() {
        if (diagnosticsEditor != nullptr) {
            const auto diagnostics
                = devpiano::audio::buildAudioDeviceDiagnostics(savedStateSnapshot.get(), deviceManager);
            diagnosticsEditor->setText(diagnostics.detailedSummary, juce::dontSendNotification);
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        dirty = true;
        updateDiagnostics();
    }

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override {
        if (tree != editingState || !model) {
            return;
        }

        if (prop == juce::Identifier("colourMode")) {
            model->keyboardDisplay.colourMode = static_cast<devpiano::ui::KeyColourMode>((int)editingState[prop] - 1);
        } else if (prop == juce::Identifier("noteDisplay")) {
            model->keyboardDisplay.noteDisplay
                = static_cast<devpiano::ui::NoteDisplayMode>((int)editingState[prop] - 1);
        } else if (prop == juce::Identifier("fadeSpeed")) {
            model->keyboardDisplay.fadeSpeed = static_cast<float>((double)editingState[prop]);
        } else if (prop == juce::Identifier("resizableWindow")) {
            model->keyboardDisplay.resizableWindow = (bool)editingState[prop];
        } else if (prop == juce::Identifier("showInstrumentFilter")) {
            model->keyboardDisplay.showInstrumentFilter = (bool)editingState[prop];
        } else if (prop == juce::Identifier("keySignature")) {
            model->keySignature = (int)editingState[prop];
        } else if (prop == juce::Identifier("midiTranspose")) {
            model->midiTranspose = (bool)editingState[prop];
            updateFollowKeyTogglesVisibility();
        } else if (prop.toString().startsWith("followKey_")) {
            auto chIdx = prop.toString().substring(10).getIntValue();
            if (chIdx >= 0 && chIdx < 16) {
                model->channelMatrix.channels[static_cast<size_t>(chIdx)].followKey = (bool)editingState[prop];
            }
        } else if (prop == juce::Identifier("languageCode")) {
            model->languageCode = editingState[prop].toString();
            if (onLanguageChanged) {
                onLanguageChanged(model->languageCode);
            }
            refreshTexts();
            return;
        }

        setDirty(true);
        if (onDisplaySettingsChanged) {
            onDisplaySettingsChanged();
        }
    }
};
