#include "Settings/jive/SettingsLayoutModel.h"

#include "Locale/LocaleManager.h"
#include "UI/jive/DesignTokens.h"

namespace devpiano::ui::jive {

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

inline juce::ValueTree flexColumn(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "column", nullptr);
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
    lbl.setProperty("justification", "centred-left", nullptr);
    row.appendChild(lbl, nullptr);

    row.appendChild(controlNode, nullptr);
    return row;
}

} // namespace

// ============================================================================
// Section Builders
// ============================================================================

juce::ValueTree makeAudioDeviceSectionTree() {
    auto section = flexColumn("audio-device-section");
    section.setProperty("margin", "0 0 12 0", nullptr);

    auto selector = node("AudioDeviceSelector", "audio-device-selector");
    selector.setProperty("height", 234, nullptr);
    section.appendChild(selector, nullptr);

    return section;
}

juce::ValueTree makeKeySignatureSectionTree() {
    auto card = flexColumn("key-sig-card");
    card.setProperty("margin", "0 0 12 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Key Signature"), "key-sig-title");
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 14, nullptr);
    title.setProperty("height", 20, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Row 1: Key Signature combo
    auto ksCombo = node("ComboBox", "key-signature-combo");
    ksCombo.setProperty("width", 200, nullptr);
    ksCombo.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Key Signature:"), ksCombo, "key-signature-label"), nullptr);

    // Row 2: MIDI Transpose toggle
    auto transposeToggle = node("Checkbox", "midi-transpose-toggle");
    transposeToggle.setProperty("text", TRANS("MIDI Transpose"), nullptr);
    transposeToggle.setProperty("width", 200, nullptr);
    transposeToggle.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("MIDI Transpose:"), transposeToggle, "midi-transpose-label"), nullptr);

    // Row 3: Channel Follow Key (Grid layout for 16 channels)
    auto followKeyArea = flexColumn("channel-follow-key-area");
    followKeyArea.setProperty("margin", "4 0 0 0", nullptr);

    auto followKeyLbl = text(TRANS("Channel Follow Key:"), "channel-follow-key-label");
    followKeyLbl.setProperty("height", 20, nullptr);
    followKeyLbl.setProperty("margin", "0 0 4 0", nullptr);
    followKeyArea.appendChild(followKeyLbl, nullptr);

    auto grid = node("Component", "follow-key-grid");
    grid.setProperty("display", "grid", nullptr);
    grid.setProperty("grid-template-columns", "1fr 1fr 1fr 1fr 1fr 1fr 1fr 1fr", nullptr);
    grid.setProperty("gap", "4", nullptr);
    grid.setProperty("height", 52, nullptr);

    for (int ch = 0; ch < 16; ++ch) {
        auto cb = node("Checkbox", "follow-key-" + juce::String(ch));
        cb.setProperty("text", "Ch" + juce::String(ch + 1), nullptr);
        cb.setProperty("title", TRANS("Follow Key"), nullptr);
        cb.setProperty("height", 24, nullptr);
        grid.appendChild(cb, nullptr);
    }
    followKeyArea.appendChild(grid, nullptr);
    card.appendChild(followKeyArea, nullptr);

    return card;
}

juce::ValueTree makeKeyboardDisplaySectionTree() {
    auto card = flexColumn("keyboard-display-card");
    card.setProperty("margin", "0 0 12 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Keyboard Display"), "keyboard-display-title");
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 14, nullptr);
    title.setProperty("height", 20, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Colour Mode
    auto colourCombo = node("ComboBox", "colour-mode-combo");
    colourCombo.setProperty("width", 200, nullptr);
    colourCombo.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Colour Mode:"), colourCombo, "colour-mode-label"), nullptr);

    // Note Display
    auto noteCombo = node("ComboBox", "note-display-combo");
    noteCombo.setProperty("width", 200, nullptr);
    noteCombo.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Note Display:"), noteCombo, "note-display-label"), nullptr);

    // Fade Speed
    auto fadeSlider = node("Slider", "fade-speed-slider");
    fadeSlider.setProperty("width", 200, nullptr);
    fadeSlider.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Fade Speed:"), fadeSlider, "fade-speed-label"), nullptr);

    // Resizable Window
    auto resizableCb = node("Checkbox", "resizable-toggle");
    resizableCb.setProperty("text", TRANS("Resizable Window"), nullptr);
    resizableCb.setProperty("width", 200, nullptr);
    resizableCb.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Resizable Window:"), resizableCb, "resizable-label"), nullptr);

    // Instrument Filter
    auto filterCb = node("Checkbox", "instrument-filter-toggle");
    filterCb.setProperty("text", TRANS("Show MIDI/VSTi Instrument Filter"), nullptr);
    filterCb.setProperty("width", 200, nullptr);
    filterCb.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Show MIDI/VSTi Instrument Filter:"), filterCb, "instrument-filter-label"),
                     nullptr);

    // Language
    auto langCombo = node("ComboBox", "language-combo");
    langCombo.setProperty("width", 200, nullptr);
    langCombo.setProperty("height", 24, nullptr);
    card.appendChild(settingRow(TRANS("Language:"), langCombo, "language-label"), nullptr);

    return card;
}

juce::ValueTree makeDiagnosticsSectionTree() {
    auto section = flexColumn("diagnostics-card");
    section.setProperty("margin", "0 0 10 0", nullptr);

    auto editor = node("ListEditor", "diagnostics-editor");
    editor.setProperty("height", 96, nullptr);
    section.appendChild(editor, nullptr);

    return section;
}

juce::ValueTree makeSaveActionSectionTree() {
    auto row = flexRow("save-action-row");
    row.setProperty("justify-content", "flex-end", nullptr);
    row.setProperty("height", 30, nullptr);

    auto saveBtn = button(TRANS("Save"), "save-button");
    saveBtn.setProperty("width", 110, nullptr);
    saveBtn.setProperty("height", 28, nullptr);
    row.appendChild(saveBtn, nullptr);

    return row;
}

juce::ValueTree makeSettingsLayoutTree() {
    auto root = flexColumn("settings-root");
    root.setProperty("width", 560, nullptr);
    root.setProperty("height", 800, nullptr);
    root.setProperty("padding", "10", nullptr);

    root.appendChild(makeAudioDeviceSectionTree(), nullptr);
    root.appendChild(makeKeySignatureSectionTree(), nullptr);
    root.appendChild(makeKeyboardDisplaySectionTree(), nullptr);
    root.appendChild(makeDiagnosticsSectionTree(), nullptr);
    root.appendChild(makeSaveActionSectionTree(), nullptr);

    return root;
}

::jive::GuiItem* findGuiItemById(::jive::GuiItem& root, const juce::String& id) {
    if (root.state.getProperty("id").toString() == id) {
        return &root;
    }
    for (auto* child : root.getChildren()) {
        if (auto* found = findGuiItemById(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace devpiano::ui::jive
