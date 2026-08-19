#include "UI/KeyBindingEditDialog.h"

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "UI/DevPianoLookAndFeel.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/StyleCatalog.h"

namespace {

inline juce::ValueTree node(const juce::Identifier& type, const juce::String& id = {}) {
    auto t = juce::ValueTree(type);
    if (id.isNotEmpty()) {
        t.setProperty("id", id, nullptr);
    }
    return t;
}

inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    t.setProperty("title", content, nullptr);
    return t;
}

inline juce::ValueTree button(const juce::String& label, const juce::String& id = {}) {
    auto t = node("Button", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("justify-content", "centre", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    t.setProperty("title", label, nullptr);
    t.setProperty("border-width", "1", nullptr);

    auto labelText = text(label);
    labelText.setProperty("justification", "centred", nullptr);
    labelText.setProperty("word-wrap", "none", nullptr);
    t.appendChild(labelText, nullptr);

    return t;
}

inline juce::ValueTree flexRow(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "row", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    return t;
}

inline juce::ValueTree settingRow(const juce::String& labelStr, const juce::ValueTree& controlNode,
                                  const juce::String& labelId = {}) {
    auto row = flexRow();
    row.setProperty("height", 28, nullptr);
    row.setProperty("margin", "0 0 6 0", nullptr);

    auto lbl = text(labelStr, labelId);
    lbl.setProperty("flex-grow", 1.0, nullptr);
    lbl.setProperty("height", 22, nullptr);
    lbl.setProperty("font-size", 14, nullptr);
    lbl.setProperty("justification", "centred-left", nullptr);
    row.appendChild(lbl, nullptr);

    row.appendChild(controlNode, nullptr);
    return row;
}

inline juce::Component* findComponentById(::jive::GuiItem& root, const juce::String& id) {
    if (auto* item = devpiano::ui::jive::JiveModalDialog::findGuiItemById(root, id)) {
        return item->getComponent().get();
    }
    return nullptr;
}

const std::array<juce::Colour, 8> paletteColours {
    juce::Colour(0xFF00C8FF), // Cyan
    juce::Colour(0xFF2ECC71), // Green
    juce::Colour(0xFFF1C40F), // Yellow
    juce::Colour(0xFFE67E22), // Orange
    juce::Colour(0xFFE74C3C), // Red
    juce::Colour(0xFF9B59B6), // Purple
    juce::Colour(0xFFFF69B4), // Pink
    juce::Colour(0xFFFFFFFF), // White
};

class PaletteButtonLookAndFeel final : public DevPianoLookAndFeel {
public:
    PaletteButtonLookAndFeel(std::shared_ptr<juce::Colour> selected, const std::array<juce::Colour, 8>& colours)
        : selectedColour(std::move(selected))
        , palette(colours) {
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& /*bg*/, bool highlighted,
                              bool down) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto btnId = button.getComponentID();

        if (btnId.startsWith("colour-btn-")) {
            const int idx = btnId.substring(11).getIntValue();
            if (idx >= 0 && idx < static_cast<int>(palette.size())) {
                const auto btnColour = palette[static_cast<size_t>(idx)];
                constexpr float corner = 4.0f;

                // 纯色圆角色块
                g.setColour(btnColour);
                g.fillRoundedRectangle(bounds, corner);

                // 深色描边增强边界感
                g.setColour(juce::Colours::black.withAlpha(0.35f));
                g.drawRoundedRectangle(bounds, corner, 1.0f);

                // 悬停高亮描边
                if (highlighted) {
                    g.setColour(juce::Colours::white.withAlpha(0.45f));
                    g.drawRoundedRectangle(bounds, corner, 1.5f);
                }

                // 按下状态轻微压暗
                if (down) {
                    g.setColour(juce::Colours::black.withAlpha(0.25f));
                    g.fillRoundedRectangle(bounds, corner);
                }

                // 当前选中色：白色描边环 + 高对比中心圆点
                if (selectedColour != nullptr && !selectedColour->isTransparent() && *selectedColour == btnColour) {
                    g.setColour(juce::Colours::white);
                    g.drawRoundedRectangle(bounds.reduced(1.0f), corner - 0.5f, 2.0f);

                    const auto pipColour
                        = btnColour.getBrightness() > 0.65f ? juce::Colours::black : juce::Colours::white;
                    g.setColour(pipColour);
                    g.fillEllipse(bounds.getCentreX() - 2.5f, bounds.getCentreY() - 2.5f, 5.0f, 5.0f);
                }
                return;
            }
        }

        // 非色块按钮（如 Clear）沿用全局主题
        DevPianoLookAndFeel::drawButtonBackground(g, button, findColour(juce::TextButton::buttonColourId), highlighted,
                                                  down);
    }

private:
    std::shared_ptr<juce::Colour> selectedColour;
    const std::array<juce::Colour, 8>& palette;
};

} // namespace

juce::ValueTree KeyBindingEditDialog::makeKeyBindingEditLayout(bool hasExistingBinding, int width, int height) {
    auto root = node("Component", "dialog-root");
    root.setProperty("display", "flex", nullptr);
    root.setProperty("flex-direction", "column", nullptr);
    root.setProperty("width", width, nullptr);
    root.setProperty("height", height, nullptr);
    root.setProperty("padding", "16", nullptr);

    // Title / Info row
    auto infoText = text("", "binding-info-text");
    infoText.setProperty("font-size", 14, nullptr);
    infoText.setProperty("height", 24, nullptr);
    infoText.setProperty("margin", "0 0 10 0", nullptr);
    root.appendChild(infoText, nullptr);

    if (hasExistingBinding) {
        // Row 1: MIDI Channel (ComboBox: 1 - 16)
        auto chCombo = node("ComboBox", "channel-combo");
        chCombo.setProperty("width", 220, nullptr);
        chCombo.setProperty("height", 24, nullptr);
        root.appendChild(settingRow(TRANS("MIDI Channel:"), chCombo, "channel-label"), nullptr);

        // Row 2: MIDI Note (Slider / Number: 0 - 127)
        auto noteSlider = node("Slider", "note-slider");
        noteSlider.setProperty("width", 220, nullptr);
        noteSlider.setProperty("height", 24, nullptr);
        root.appendChild(settingRow(TRANS("MIDI Note:"), noteSlider, "note-label"), nullptr);

        // Row 3: Velocity (Slider: 0 - 127)
        auto velSlider = node("Slider", "velocity-slider");
        velSlider.setProperty("width", 220, nullptr);
        velSlider.setProperty("height", 24, nullptr);
        root.appendChild(settingRow(TRANS("Velocity:"), velSlider, "velocity-label"), nullptr);
    }

    // Row: Custom Label (PathEditor)
    auto labelEd = node("PathEditor", "custom-label-editor");
    labelEd.setProperty("width", 220, nullptr);
    labelEd.setProperty("height", 24, nullptr);
    root.appendChild(settingRow(TRANS("Label:"), labelEd, "custom-label-text"), nullptr);

    // Row: Custom Colour Selection (Palette buttons + Clear button)
    auto colourRow = flexRow("custom-colour-row");
    colourRow.setProperty("height", 28, nullptr);
    colourRow.setProperty("margin", "0 0 10 0", nullptr);

    auto colourLbl = text(TRANS("Colour:"), "custom-colour-label");
    colourLbl.setProperty("flex-grow", 1.0, nullptr);
    colourLbl.setProperty("height", 22, nullptr);
    colourLbl.setProperty("font-size", 14, nullptr);
    colourLbl.setProperty("justification", "centred-left", nullptr);
    colourRow.appendChild(colourLbl, nullptr);

    auto colourPalette = flexRow("custom-colour-palette");
    colourPalette.setProperty("width", 220, nullptr);
    colourPalette.setProperty("height", 24, nullptr);
    colourPalette.setProperty("align-items", "centre", nullptr);
    colourPalette.setProperty("gap", "4", nullptr);

    for (size_t i = 0; i < paletteColours.size(); ++i) {
        auto cBtn = node("Button", "colour-btn-" + juce::String(i));
        cBtn.setProperty("width", 18, nullptr);
        cBtn.setProperty("min-width", 18, nullptr);
        cBtn.setProperty("max-width", 18, nullptr);
        cBtn.setProperty("height", 20, nullptr);
        cBtn.setProperty("min-height", 20, nullptr);
        cBtn.setProperty("max-height", 20, nullptr);
        cBtn.setProperty("border-radius", "4", nullptr);
        colourPalette.appendChild(cBtn, nullptr);
    }

    auto clearColourBtn = button(TRANS("Clear"), "clear-colour-btn");
    clearColourBtn.setProperty("width", 44, nullptr);
    clearColourBtn.setProperty("min-width", 44, nullptr);
    clearColourBtn.setProperty("max-width", 44, nullptr);
    clearColourBtn.setProperty("height", 22, nullptr);
    clearColourBtn.setProperty("min-height", 22, nullptr);
    clearColourBtn.setProperty("max-height", 22, nullptr);
    clearColourBtn.setProperty("margin", "0 0 0 4", nullptr);
    colourPalette.appendChild(clearColourBtn, nullptr);

    colourRow.appendChild(colourPalette, nullptr);
    root.appendChild(colourRow, nullptr);

    // Bottom Action Button Row
    auto btnRow = node("Component", "dialog-buttons");
    btnRow.setProperty("display", "flex", nullptr);
    btnRow.setProperty("flex-direction", "row", nullptr);
    btnRow.setProperty("align-items", "centre", nullptr);
    btnRow.setProperty("height", 30, nullptr);
    btnRow.setProperty("margin", "10 0 0 0", nullptr);

    if (hasExistingBinding) {
        auto unbindBtn = button(TRANS("Unbind"), "dialog-unbind-btn");
        unbindBtn.setProperty("width", 88, nullptr);
        unbindBtn.setProperty("height", 28, nullptr);
        btnRow.appendChild(unbindBtn, nullptr);

        auto spacer = node("Component", "btn-spacer");
        spacer.setProperty("flex-grow", 1.0, nullptr);
        btnRow.appendChild(spacer, nullptr);

        auto okBtn = button(TRANS("OK"), "dialog-ok-btn");
        okBtn.setProperty("width", 80, nullptr);
        okBtn.setProperty("height", 28, nullptr);
        okBtn.setProperty("margin", "0 8 0 0", nullptr);
        btnRow.appendChild(okBtn, nullptr);

        auto cancelBtn = button(TRANS("Cancel"), "dialog-cancel-btn");
        cancelBtn.setProperty("width", 80, nullptr);
        cancelBtn.setProperty("height", 28, nullptr);
        btnRow.appendChild(cancelBtn, nullptr);
    } else {
        auto spacer = node("Component", "btn-spacer");
        spacer.setProperty("flex-grow", 1.0, nullptr);
        btnRow.appendChild(spacer, nullptr);

        auto okBtn = button(TRANS("OK"), "dialog-ok-btn");
        okBtn.setProperty("width", 80, nullptr);
        okBtn.setProperty("height", 28, nullptr);
        okBtn.setProperty("margin", "0 8 0 0", nullptr);
        btnRow.appendChild(okBtn, nullptr);

        auto cancelBtn = button(TRANS("Cancel"), "dialog-cancel-btn");
        cancelBtn.setProperty("width", 80, nullptr);
        cancelBtn.setProperty("height", 28, nullptr);
        btnRow.appendChild(cancelBtn, nullptr);
    }

    root.appendChild(btnRow, nullptr);
    return root;
}

void KeyBindingEditDialog::launch(int midiNote, const juce::String& noteName,
                                  const devpiano::core::KeyBinding* existingBinding,
                                  const juce::String& currentCustomLabel, const juce::Colour& currentCustomColour,
                                  std::function<void(KeyBindingEditResult)> onComplete, juce::Component* parent) {
    const bool hasExisting = (existingBinding != nullptr);
    const auto keyLabel = hasExisting ? existingBinding->displayText : juce::String();
    const int dlgWidth = 460;
    const int dlgHeight = hasExisting ? 300 : 210;

    auto layoutTree = makeKeyBindingEditLayout(hasExisting, dlgWidth, dlgHeight);
    auto title = TRANS("Key Binding Editor") + " — " + noteName + " (#" + juce::String(midiNote) + ")";

    auto selectedColour = std::make_shared<juce::Colour>(currentCustomColour);
    auto paletteLaf = std::make_shared<PaletteButtonLookAndFeel>(selectedColour, paletteColours);

    devpiano::ui::jive::JiveModalDialog::LaunchOptions options;
    options.title = title;
    options.layoutTree = layoutTree;
    options.componentToCentreAround = parent;
    options.defaultWidth = dlgWidth;
    options.defaultHeight = dlgHeight;

    options.onInit = [=](::jive::GuiItem& root) {
        // Set info text
        if (auto* infoGui = devpiano::ui::jive::JiveModalDialog::findGuiItemById(root, "binding-info-text")) {
            const auto msg = hasExisting ? (TRANS("Bound to keyboard key:") + "  " + keyLabel)
                                         : TRANS("No keyboard key is currently mapped to this note.");
            infoGui->state.setProperty("text", msg, nullptr);
            infoGui->state.setProperty("title", msg, nullptr);
        }

        // Configure channel combo
        if (auto* chComp = findComponentById(root, "channel-combo")) {
            if (auto* combo = dynamic_cast<juce::ComboBox*>(chComp)) {
                for (int ch = 1; ch <= 16; ++ch) {
                    combo->addItem(juce::String(ch), ch);
                }
                if (hasExisting) {
                    combo->setSelectedId(juce::jlimit(1, 16, existingBinding->action.midiChannel),
                                         juce::dontSendNotification);
                }
            }
        }

        // Configure note slider
        if (auto* noteComp = findComponentById(root, "note-slider")) {
            if (auto* slider = dynamic_cast<juce::Slider*>(noteComp)) {
                slider->setRange(0.0, 127.0, 1.0);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
                slider->setNumDecimalPlacesToDisplay(0);
                if (hasExisting) {
                    slider->setValue(juce::jlimit(0, 127, existingBinding->action.midiNote),
                                     juce::dontSendNotification);
                }
            }
        }

        // Configure velocity slider
        if (auto* velComp = findComponentById(root, "velocity-slider")) {
            if (auto* slider = dynamic_cast<juce::Slider*>(velComp)) {
                slider->setRange(0.0, 127.0, 1.0);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
                slider->setNumDecimalPlacesToDisplay(0);
                if (hasExisting) {
                    slider->setValue(
                        juce::jlimit(0.0, 127.0, static_cast<double>(existingBinding->action.velocity * 127.0f)),
                        juce::dontSendNotification);
                }
            }
        }

        // Configure custom label editor
        if (auto* labelEd = devpiano::ui::jive::JiveModalDialog::findTextEditorById(root, "custom-label-editor")) {
            labelEd->setText(currentCustomLabel, false);
            labelEd->setInputRestrictions(32, {});
        }

        // Wire palette color buttons: 绑定自定义外观 + 点击即时刷新选中环
        ::jive::GuiItem* rootPtr = &root;
        for (size_t i = 0; i < paletteColours.size(); ++i) {
            if (auto* cBtn
                = devpiano::ui::jive::JiveModalDialog::findButtonById(root, "colour-btn-" + juce::String(i))) {
                cBtn->setComponentID("colour-btn-" + juce::String(i));
                cBtn->setLookAndFeel(paletteLaf.get());
                cBtn->onClick = [selectedColour, c = paletteColours[i], rootPtr] {
                    *selectedColour = c;
                    for (size_t k = 0; k < paletteColours.size(); ++k) {
                        if (auto* b = devpiano::ui::jive::JiveModalDialog::findButtonById(
                                *rootPtr, "colour-btn-" + juce::String(k))) {
                            b->repaint();
                        }
                    }
                    if (auto* clr = devpiano::ui::jive::JiveModalDialog::findButtonById(*rootPtr, "clear-colour-btn")) {
                        clr->repaint();
                    }
                };
            }
        }

        // Wire clear color button
        if (auto* clearBtn = devpiano::ui::jive::JiveModalDialog::findButtonById(root, "clear-colour-btn")) {
            clearBtn->setComponentID("clear-colour-btn");
            clearBtn->setLookAndFeel(paletteLaf.get());
            clearBtn->onClick = [selectedColour, rootPtr] {
                *selectedColour = juce::Colour(0x00000000);
                for (size_t k = 0; k < paletteColours.size(); ++k) {
                    if (auto* b = devpiano::ui::jive::JiveModalDialog::findButtonById(
                            *rootPtr, "colour-btn-" + juce::String(k))) {
                        b->repaint();
                    }
                }
                if (auto* cb = devpiano::ui::jive::JiveModalDialog::findButtonById(*rootPtr, "clear-colour-btn")) {
                    cb->repaint();
                }
            };
        }

        // Wire unbind button
        if (auto* unbindBtn = devpiano::ui::jive::JiveModalDialog::findButtonById(root, "dialog-unbind-btn")) {
            unbindBtn->onClick = [=, &root] {
                KeyBindingEditResult result;
                if (auto* ed = devpiano::ui::jive::JiveModalDialog::findTextEditorById(root, "custom-label-editor")) {
                    result.customLabel = ed->getText();
                }
                result.customColour = *selectedColour;
                result.labelChanged = true;
                result.colourChanged = true;

                if (hasExisting) {
                    auto removed = *existingBinding;
                    removed.keyCode = -1;
                    result.binding = removed;
                }

                if (onComplete) {
                    onComplete(result);
                }

                if (auto* dw = unbindBtn->findParentComponentOfClass<juce::DialogWindow>()) {
                    dw->exitModalState(0);
                }
            };
        }
    };

    options.onConfirm = [=](::jive::GuiItem& root) -> bool {
        KeyBindingEditResult result;
        if (auto* ed = devpiano::ui::jive::JiveModalDialog::findTextEditorById(root, "custom-label-editor")) {
            result.customLabel = ed->getText();
        }
        result.customColour = *selectedColour;
        result.labelChanged = (result.customLabel != currentCustomLabel);
        result.colourChanged = (result.customColour != currentCustomColour);

        if (hasExisting) {
            auto updated = *existingBinding;
            if (auto* chComp = findComponentById(root, "channel-combo")) {
                if (auto* combo = dynamic_cast<juce::ComboBox*>(chComp)) {
                    updated.action.midiChannel = combo->getSelectedId();
                }
            }
            if (auto* noteComp = findComponentById(root, "note-slider")) {
                if (auto* slider = dynamic_cast<juce::Slider*>(noteComp)) {
                    updated.action.midiNote = static_cast<int>(slider->getValue());
                }
            }
            if (auto* velComp = findComponentById(root, "velocity-slider")) {
                if (auto* slider = dynamic_cast<juce::Slider*>(velComp)) {
                    updated.action.velocity = static_cast<float>(slider->getValue() / 127.0);
                }
            }
            result.binding = updated;
        }

        if (onComplete) {
            onComplete(result);
        }
        return true;
    };

    options.onCancel = [=] {
        if (onComplete) {
            onComplete(KeyBindingEditResult {});
        }
    };

    devpiano::ui::jive::JiveModalDialog::launchCustom(options);
}
