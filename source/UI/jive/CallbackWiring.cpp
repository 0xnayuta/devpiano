#include "CallbackWiring.h"
#include "Diagnostics/Log.h"

#include "MainComponent.h"

#include "UI/KeyBindingEditDialog.h"

namespace devpiano::ui::jive {

namespace {

/// Helper: find a typed Component in the JIVE tree by ID.
template <typename T> T* find(::jive::GuiItem& root, const juce::Identifier& id) {
    if (auto* item = ::jive::findItemWithID(root, id))
        return dynamic_cast<T*>(item->getComponent().get());
    return nullptr;
}

/// Helper: find a Button by ID (TextButton or DrawableButton).
juce::Button* findButton(::jive::GuiItem& root, const juce::Identifier& id) {
    return dynamic_cast<juce::Button*>(::jive::findItemWithID(root, id)->getComponent().get());
}

/// Helper: find a Slider by ID.
juce::Slider* findSlider(::jive::GuiItem& root, const juce::Identifier& id) {
    return find<juce::Slider>(root, id);
}

} // namespace

void wireAllCallbacks(::jive::GuiItem& root, MainComponent& mc) {
    // ── Header ──
    if (auto* btn = findButton(root, "settings-btn"))
        btn->onClick = [&mc] { mc.showSettingsDialog(); };

    // ── PluginPanel ──
    if (auto* btn = findButton(root, "scan-btn"))
        btn->onClick = [&mc] { mc.pluginOperationController->scanPlugins(); };
    if (auto* btn = findButton(root, "load-btn"))
        btn->onClick = [&mc] { mc.pluginOperationController->loadSelectedPlugin(); };
    if (auto* btn = findButton(root, "unload-btn"))
        btn->onClick = [&mc] { mc.pluginOperationController->unloadCurrentPlugin(); };
    if (auto* btn = findButton(root, "editor-btn"))
        btn->onClick = [&mc] { mc.pluginOperationController->togglePluginEditor(); };

    // Plugin expand/collapse toggle
    if (auto* btn = findButton(root, "toggle-btn"))
        btn->onClick = [&mc] {
            auto expanded = mc.isPluginPanelExpanded();
            mc.setPluginPanelExpanded(!expanded);
            mc.resized();
            mc.appSettings.pluginPanelExpanded = !expanded;
            mc.settingsStore.scheduleSave(mc.appSettings);
        };

    // Plugin browse button
    if (auto* btn = findButton(root, "browse-btn"))
        btn->onClick = [&mc] {
            juce::FileChooser chooser("Select VST3 Directory", juce::File(mc.getPluginPanelPath()), "*", true);
            if (chooser.browseForDirectory())
                mc.setPluginPanelPath(chooser.getResult().getFullPathName());
        };

    // ── ControlsPanel: ADSR/volume knobs ──
    auto wireKnob = [&](const juce::Identifier& id) {
        if (auto* sl = findSlider(root, id))
            sl->onValueChange = [&mc] { mc.handlePerformanceUiChanged(); };
    };
    wireKnob("volume-knob");
    wireKnob("attack-knob");
    wireKnob("decay-knob");
    wireKnob("sustain-knob");
    wireKnob("release-knob");

    // Playback speed knob
    if (auto* sl = findSlider(root, "speed-knob"))
        sl->onValueChange = [&mc, sl] { mc.recordingSessionController->handlePlaybackSpeedChange(sl->getValue()); };

    // ── Preset selector ──
    if (auto* cb = find<juce::ComboBox>(root, "preset-combo"))
        cb->onChange = [&mc, cb] {
            auto idx = cb->getSelectedItemIndex();
            auto ids = mc.presetFlowSupport->getPresetIds();
            if (idx >= 0 && idx < ids.size())
                mc.presetFlowSupport->applyPresetById(ids[idx]);
        };

    // Preset action buttons
    if (auto* btn = findButton(root, "save-preset-btn"))
        btn->onClick = [&mc] { mc.presetFlowSupport->handleSaveAsNewPreset(); };
    if (auto* btn = findButton(root, "rename-preset-btn"))
        btn->onClick = [&mc] { mc.presetFlowSupport->handleRenamePreset(); };
    if (auto* btn = findButton(root, "delete-preset-btn"))
        btn->onClick = [&mc] { mc.presetFlowSupport->handleDeletePreset(); };

    // ── Transport buttons ──
    if (auto* btn = findButton(root, "record-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleRecordClicked(); };
    if (auto* btn = findButton(root, "play-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handlePlayClicked(); };
    if (auto* btn = findButton(root, "stop-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleStopClicked(); };
    if (auto* btn = findButton(root, "back-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleBackToStartClicked(); };

    // ── Export / File buttons ──
    if (auto* btn = findButton(root, "export-midi-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleExportMidiClicked(); };
    if (auto* btn = findButton(root, "export-wav-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleExportWavClicked(); };
    if (auto* btn = findButton(root, "import-midi-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleImportMidiClicked(); };
    if (auto* btn = findButton(root, "save-perf-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleSavePerformanceClicked(); };
    if (auto* btn = findButton(root, "open-perf-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleOpenPerformanceClicked(); };
    if (auto* btn = findButton(root, "song-info-btn"))
        btn->onClick = [&mc] { mc.recordingSessionController->handleSongInfoClicked(); };
    if (auto* btn = findButton(root, "recent-btn"))
        btn->onClick = [&mc] { mc.showRecentFilesMenu(); };

    // ── onFileOpened ──
    mc.recordingSessionController->onFileOpened = [&mc](const juce::File& file) {
        mc.recentFiles.addFile(file);
        mc.saveRecentFiles();
    };

    // ── CustomKeyboard ──
    // CustomKeyboard is wrapped in a Viewport; find the Viewport's viewed component
    if (auto* vpItem = ::jive::findItemWithID(root, "custom-keyboard")) {
        if (auto* vp = dynamic_cast<juce::Viewport*>(vpItem->getComponent().get())) {
            if (auto* ck = dynamic_cast<CustomKeyboard*>(vp->getViewedComponent())) {
                ck->onNoteOn = [&mc](int midiNote, int sourceChannel) {
                    if (mc.midiChannelMapper != nullptr)
                        mc.midiChannelMapper->sendNoteOn(sourceChannel, midiNote, 1.0f,
                                                         mc.audioEngine.getKeyboardState());
                    else
                        mc.audioEngine.getKeyboardState().noteOn(1, midiNote, 1.0f);
                    mc.suppressTextInputMethods();
                };
                ck->onNoteOff = [&mc](int midiNote, int sourceChannel) {
                    if (mc.midiChannelMapper != nullptr)
                        mc.midiChannelMapper->sendNoteOff(sourceChannel, midiNote, 1.0f,
                                                          mc.audioEngine.getKeyboardState());
                    else
                        mc.audioEngine.getKeyboardState().noteOff(1, midiNote, 1.0f);
                };
                ck->onBindingEditRequested = [&mc](int midiNote) {
                    const auto& layout = mc.keyboardMidiMapper.getLayout();
                    auto noteName = devpiano::ui::getNoteDisplayName(midiNote, devpiano::ui::NoteDisplayMode::noteName);

                    const devpiano::core::KeyBinding* existingBinding = nullptr;
                    for (const auto& binding : layout.bindings) {
                        if (binding.action.type == devpiano::core::KeyActionType::note
                            && binding.action.midiNote == midiNote) {
                            existingBinding = &binding;
                            break;
                        }
                    }

                    auto currentLabel = mc.appSettings.customKeyLabels[static_cast<std::size_t>(midiNote)];
                    auto currentColour = mc.appSettings.customKeyColours[static_cast<std::size_t>(midiNote)];

                    KeyBindingEditDialog::launch(
                        midiNote, noteName, existingBinding, currentLabel, currentColour,
                        [&mc, midiNote](KeyBindingEditResult result) {
                            if (result.labelChanged)
                                mc.appSettings.customKeyLabels[static_cast<std::size_t>(midiNote)] = result.customLabel;
                            if (result.colourChanged)
                                mc.appSettings.customKeyColours[static_cast<std::size_t>(midiNote)]
                                    = result.customColour;

                            if (result.binding.has_value()) {
                                auto updatedLayout = mc.keyboardMidiMapper.getLayout();

                                if (result.binding->keyCode < 0) {
                                    updatedLayout.bindings.erase(
                                        std::remove_if(updatedLayout.bindings.begin(), updatedLayout.bindings.end(),
                                                       [note = result.binding->action.midiNote](const auto& b) {
                                                           return b.action.type == devpiano::core::KeyActionType::note
                                                               && b.action.midiNote == note;
                                                       }),
                                        updatedLayout.bindings.end());
                                } else {
                                    for (auto& b : updatedLayout.bindings)
                                        if (b.keyCode == result.binding->keyCode) {
                                            b = *result.binding;
                                            break;
                                        }
                                }

                                mc.keyboardMidiMapper.setLayout(updatedLayout);
                                mc.setCustomKeyboardLayout(updatedLayout);
                            }

                            mc.syncUiFromSettings();
                            mc.saveSettingsSoon();

                            if (mc.presetFlowSupport != nullptr) {
                                auto currentId = mc.presetFlowSupport->getCurrentPresetId();
                                if (currentId.isNotEmpty()) {
                                    auto updatedPreset = mc.presetFlowSupport->captureCurrentState(currentId);
                                    auto presetFile = devpiano::layout::getPresetDirectory().getChildFile(
                                        devpiano::layout::sanitisePresetFileName(currentId) + ".devpiano.preset");
                                    if (!devpiano::layout::savePreset(updatedPreset, presetFile))
                                        DP_LOG_WARN("[Preset] auto-save failed after binding edit: " + currentId);
                                }
                            }
                        },
                        &mc);
                };
            }
        }
    }
}

} // namespace devpiano::ui::jive
