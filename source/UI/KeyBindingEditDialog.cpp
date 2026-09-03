#include "UI/KeyBindingEditDialog.h"

#include <JuceHeader.h>
#include <array>
#include <cctype>
#include <memory>

#include "UI/ColourSwatchButton.h"
#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveBuilderHelpers.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

namespace {
using namespace devpiano::ui::jive;

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

struct KeyCaptureSession {
    bool active = false;
    int keyCode = 0;
    juce::String displayText;
};

// 按键捕获监听：Bind Key 流程中捕获下一个有效物理按键
class BindKeyCaptureListener final : public juce::KeyListener {
public:
    explicit BindKeyCaptureListener(std::shared_ptr<KeyCaptureSession> stateToTrack)
        : session(std::move(stateToTrack)) {
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override {
        if (!session->active) {
            return false;
        }

        // ESC 取消捕获并保持对话框打开（消费事件阻止关闭）
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            session->active = false;
            if (onCancelled != nullptr) {
                onCancelled();
            }
            return true;
        }

        // 纯修饰键（Shift/Ctrl/Alt/Win）没有有效 keyCode，忽略
        if (key.getKeyCode() == 0) {
            return false;
        }

        const auto rawCode = key.getKeyCode();
        const bool isAlphaNum = std::isalnum(static_cast<unsigned char>(rawCode)) != 0;
        session->keyCode = isAlphaNum ? devpiano::core::normaliseAlphaNumericKeyCode(rawCode) : rawCode;
        session->displayText = juce::KeyPress(session->keyCode).getTextDescription();
        session->active = false;
        if (onCaptured != nullptr) {
            onCaptured();
        }
        return true;
    }

    std::function<void()> onCaptured;
    std::function<void()> onCancelled;

private:
    std::shared_ptr<KeyCaptureSession> session;
};

inline devpiano::ui::ColourSwatchButton* findSwatchById(const devpiano::ui::ViewHost& host, size_t index) {
    return host.find<devpiano::ui::ColourSwatchButton>("colour-btn-" + juce::String(index));
}

void setupBindingInfoAndInputs(const devpiano::ui::ViewHost& host, const KeyBindingDialogParams& params) {
    const bool hasExisting = params.existingBinding.has_value();
    const auto keyLabel = hasExisting ? params.existingBinding->displayText : juce::String();

    const auto msg = hasExisting ? (TRANS("Bound to keyboard key:") + "  " + keyLabel)
                                 : TRANS("No keyboard key is currently mapped to this note.");
    host.setText("binding-info-text", msg);

    if (auto* combo = host.find<juce::ComboBox>("channel-combo")) {
        for (int ch = 1; ch <= 16; ++ch) {
            combo->addItem(juce::String(ch), ch);
        }
        if (hasExisting) {
            combo->setSelectedId(juce::jlimit(1, 16, params.existingBinding->action.midiChannel),
                                 juce::dontSendNotification);
        }
    }

    if (auto* slider = host.find<juce::Slider>("note-slider")) {
        slider->setRange(0.0, 127.0, 1.0);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
        slider->setNumDecimalPlacesToDisplay(0);
        if (hasExisting) {
            slider->setValue(juce::jlimit(0, 127, params.existingBinding->action.midiNote), juce::dontSendNotification);
        }
    }

    if (auto* slider = host.find<juce::Slider>("velocity-slider")) {
        slider->setRange(0.0, 127.0, 1.0);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
        slider->setNumDecimalPlacesToDisplay(0);
        if (hasExisting) {
            slider->setValue(
                juce::jlimit(0.0, 127.0, static_cast<double>(params.existingBinding->action.velocity * 127.0f)),
                juce::dontSendNotification);
        }
    }

    if (auto* labelEd = host.find<juce::TextEditor>("custom-label-editor")) {
        labelEd->setText(params.currentCustomLabel, false);
        labelEd->setInputRestrictions(32, {});
    }
}

void setupColourPaletteControls(const devpiano::ui::ViewHost& host, const std::shared_ptr<juce::Colour>& selectedColour,
                                const std::shared_ptr<std::array<juce::Colour, 8>>& palette) {
    const auto* hostPtr = &host;
    const auto refreshSwatches = [hostPtr, selectedColour, palette] {
        for (size_t k = 0; k < palette->size(); ++k) {
            if (auto* sw = findSwatchById(*hostPtr, k)) {
                sw->setSwatchColour((*palette)[k]);
                sw->setSelected(!selectedColour->isTransparent() && *selectedColour == (*palette)[k]);
            }
        }
    };

    for (size_t i = 0; i < palette->size(); ++i) {
        if (auto* sw = findSwatchById(host, i)) {
            sw->setSwatchColour((*palette)[i]);
            sw->setSelected(!selectedColour->isTransparent() && *selectedColour == (*palette)[i]);
            sw->onClick = [selectedColour, palette, i, refreshSwatches] {
                *selectedColour = (*palette)[i];
                refreshSwatches();
            };
            sw->onColourChosen = [selectedColour, palette, i, refreshSwatches](juce::Colour chosen) {
                (*palette)[i] = chosen;
                *selectedColour = chosen;
                refreshSwatches();
            };
        }
    }

    if (auto* clearBtn = host.find<juce::Button>("clear-colour-btn")) {
        clearBtn->onClick = [selectedColour, refreshSwatches] {
            *selectedColour = juce::Colour(0x00000000);
            refreshSwatches();
        };
    }
}

void setupBindKeyFlow(const devpiano::ui::ViewHost& host, const std::shared_ptr<KeyCaptureSession>& captureSession,
                      const std::shared_ptr<BindKeyCaptureListener>& captureListener) {
    const auto* hostPtr = &host;
    const auto updateBindBtnLabel
        = [hostPtr](const juce::String& label) { hostPtr->setButtonLabel("dialog-bind-btn", label); };

    if (auto* bindBtn = host.find<juce::Button>("dialog-bind-btn")) {
        bindBtn->onClick = [captureSession, hostPtr, updateBindBtnLabel] {
            captureSession->active = true;
            updateBindBtnLabel(TRANS("Press a key..."));
            if (auto* btn = hostPtr->find<juce::Button>("dialog-bind-btn")) {
                btn->grabKeyboardFocus();
            }
            hostPtr->setText("binding-info-text", TRANS("Press a key..."));
        };
    }

    captureListener->onCaptured = [captureSession, hostPtr, updateBindBtnLabel] {
        updateBindBtnLabel(TRANS("Bind Key..."));
        hostPtr->setText("binding-info-text", TRANS("Bound to keyboard key:") + "  " + captureSession->displayText);
    };

    captureListener->onCancelled = [hostPtr, updateBindBtnLabel] {
        updateBindBtnLabel(TRANS("Bind Key..."));
        hostPtr->setText("binding-info-text", TRANS("No keyboard key is currently mapped to this note."));
    };

    if (auto* rootComp = host.getRootComponent()) {
        rootComp->addKeyListener(captureListener.get());
    }
}

void setupUnbindButton(const devpiano::ui::ViewHost& host, const KeyBindingDialogParams& params,
                       const std::shared_ptr<juce::Colour>& selectedColour,
                       const std::function<void(KeyBindingEditResult)>& safeOnComplete) {
    const auto* hostPtr = &host;
    if (auto* unbindBtn = host.find<juce::Button>("dialog-unbind-btn")) {
        unbindBtn->onClick = [params, hostPtr, selectedColour, safeOnComplete, unbindBtn] {
            KeyBindingEditResult result;
            if (auto* ed = hostPtr->find<juce::TextEditor>("custom-label-editor")) {
                result.customLabel = ed->getText();
            }
            result.customColour = *selectedColour;
            result.labelChanged = true;
            result.colourChanged = true;

            if (params.existingBinding.has_value()) {
                auto removed = *params.existingBinding;
                removed.keyCode = -1;
                result.binding = removed;
            }

            safeOnComplete(result);
            if (auto* dw = unbindBtn->findParentComponentOfClass<juce::DialogWindow>()) {
                dw->exitModalState(0);
            }
        };
    }
}
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
    // JIVE CommonGuiItem 缺省 focusable=false 会禁用键盘焦点，导致无法点击输入
    labelEd.setProperty("focusable", true, nullptr);
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
        auto cBtn = node("ColourSwatch", "colour-btn-" + juce::String(i));
        cBtn.setProperty("width", 18, nullptr);
        cBtn.setProperty("height", 20, nullptr);
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
        auto bindBtn = button(TRANS("Bind Key..."), "dialog-bind-btn");
        bindBtn.setProperty("width", 120, nullptr);
        bindBtn.setProperty("height", 28, nullptr);
        bindBtn.setProperty("margin", "0 8 0 0", nullptr);
        btnRow.appendChild(bindBtn, nullptr);

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

void KeyBindingEditDialog::launch(const KeyBindingDialogParams& params) {
    const bool hasExisting = params.existingBinding.has_value();
    const int dlgWidth = 460;
    const int dlgHeight = hasExisting ? 300 : 210;

    auto layoutTree = makeKeyBindingEditLayout(hasExisting, dlgWidth, dlgHeight);
    auto title = TRANS("Key Binding Editor") + " - " + params.noteName + " (#" + juce::String(params.midiNote) + ")";

    auto completionInvoked = std::make_shared<std::atomic<bool>>(false);
    auto safeOnComplete = [onComplete = params.onComplete, completionInvoked](KeyBindingEditResult res) {
        if (!completionInvoked->exchange(true)) {
            if (onComplete) {
                onComplete(std::move(res));
            }
        }
    };

    auto selectedColour = std::make_shared<juce::Colour>(params.currentCustomColour);
    auto palette = std::make_shared<std::array<juce::Colour, 8>>(paletteColours);
    auto captureSession = std::make_shared<KeyCaptureSession>();
    auto captureListener = std::make_shared<BindKeyCaptureListener>(captureSession);

    devpiano::ui::jive::JiveModalDialog::LaunchOptions options;
    options.title = title;
    options.layoutTree = layoutTree;
    options.componentToCentreAround = params.parent;
    options.defaultWidth = dlgWidth;
    options.defaultHeight = dlgHeight;
    options.configureFactory = [](::jive::ComponentFactory& factory) {
        factory.set("ColourSwatch", [] { return std::make_unique<devpiano::ui::ColourSwatchButton>(); });
    };

    options.onInitHost = [=](const devpiano::ui::ViewHost& host) {
        setupBindingInfoAndInputs(host, params);
        setupColourPaletteControls(host, selectedColour, palette);
        setupBindKeyFlow(host, captureSession, captureListener);
        setupUnbindButton(host, params, selectedColour, safeOnComplete);
    };

    options.onConfirmHost = [=](const devpiano::ui::ViewHost& host) -> bool {
        KeyBindingEditResult result;
        if (auto* ed = host.find<juce::TextEditor>("custom-label-editor")) {
            result.customLabel = ed->getText();
        }
        result.customColour = *selectedColour;
        result.labelChanged = (result.customLabel != params.currentCustomLabel);
        result.colourChanged = (result.customColour != params.currentCustomColour);

        if (hasExisting) {
            auto updated = *params.existingBinding;
            if (auto* combo = host.find<juce::ComboBox>("channel-combo")) {
                updated.action.midiChannel = combo->getSelectedId();
            }
            if (auto* slider = host.find<juce::Slider>("note-slider")) {
                updated.action.midiNote = static_cast<int>(slider->getValue());
            }
            if (auto* slider = host.find<juce::Slider>("velocity-slider")) {
                updated.action.velocity = static_cast<float>(slider->getValue() / 127.0);
            }
            result.binding = updated;
        } else if (captureSession->keyCode != 0) {
            devpiano::core::KeyBinding created;
            created.keyCode = captureSession->keyCode;
            created.displayText = captureSession->displayText;
            created.action.type = devpiano::core::KeyActionType::note;
            created.action.trigger = devpiano::core::KeyTrigger::keyDown;
            created.action.setMidiNoteNumber(devpiano::core::MidiNoteNumber::fromClamped(params.midiNote));
            created.action.setMidiChannel(devpiano::core::MidiChannel::fromClamped(1));
            created.action.setVelocity(devpiano::core::Velocity::fromClamped(1.0f));
            result.binding = created;
        }

        safeOnComplete(result);
        return true;
    };

    options.onCancel = [=] { safeOnComplete(KeyBindingEditResult {}); };
    devpiano::ui::jive::JiveModalDialog::launchCustom(options);
}
