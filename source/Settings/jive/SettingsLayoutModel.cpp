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

    auto labelText = text(label, id.isNotEmpty() ? id + "-text" : juce::String {});
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
    t.setProperty("align-items", "stretch", nullptr);
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
} // namespace

// ============================================================================
// Section Builders
// ============================================================================

juce::ValueTree makeAudioDeviceSectionTree() {
    auto card = flexColumn("audio-device-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);
    auto title = text(TRANS("Audio Device"), "audio-device-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("audio-device-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Row 1: Audio Device Type (ComboBox)
    auto typeCombo = node("ComboBox", "audio-device-type-combo");
    typeCombo.setProperty("width", 300, nullptr);
    typeCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Audio Device Type:"), typeCombo, "audio-device-type-label"), nullptr);

    // Row 2: Output Device (ComboBox + Test Button)
    auto outputRow = flexRow("audio-output-row");
    outputRow.setProperty("height", 28, nullptr);
    outputRow.setProperty("margin", "0 0 6 0", nullptr);

    auto outputLbl = text(TRANS("Output Device:"), "audio-output-label");
    outputLbl.setProperty("flex-grow", 1.0, nullptr);
    outputLbl.setProperty("height", 22, nullptr);
    outputLbl.setProperty("font-size", 14, nullptr);
    outputLbl.setProperty("justification", "centred-left", nullptr);
    outputRow.appendChild(outputLbl, nullptr);

    auto outputControls = flexRow("audio-output-controls");
    outputControls.setProperty("width", 300, nullptr);

    auto outputCombo = node("ComboBox", "audio-output-device-combo");
    outputCombo.setProperty("width", 236, nullptr);
    outputCombo.setProperty("height", 24, nullptr);
    outputCombo.setProperty("margin", "0 8 0 0", nullptr);
    outputControls.appendChild(outputCombo, nullptr);

    auto testBtn = button(TRANS("Test"), "audio-test-button");
    testBtn.setProperty("width", 56, nullptr);
    testBtn.setProperty("height", 24, nullptr);
    outputControls.appendChild(testBtn, nullptr);
    outputRow.appendChild(outputControls, nullptr);
    content.appendChild(outputRow, nullptr);
    // Row 3: Active Output Channels (ComboBox)
    auto channelsCombo = node("ComboBox", "audio-active-channels-combo");
    channelsCombo.setProperty("width", 300, nullptr);
    channelsCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Active output channels:"), channelsCombo, "audio-active-channels-label"),
                        nullptr);

    // Row 4: Sample Rate (ComboBox)
    auto sampleRateCombo = node("ComboBox", "audio-sample-rate-combo");
    sampleRateCombo.setProperty("width", 300, nullptr);
    sampleRateCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Sample Rate:"), sampleRateCombo, "audio-sample-rate-label"), nullptr);

    // Row 5: Buffer Size (ComboBox)
    auto bufferSizeCombo = node("ComboBox", "audio-buffer-size-combo");
    bufferSizeCombo.setProperty("width", 300, nullptr);
    bufferSizeCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Audio Buffer Size:"), bufferSizeCombo, "audio-buffer-size-label"), nullptr);

    // Row 6: ASIO Control Panel (optional, collapsed when not in ASIO mode)
    auto asioRow = flexRow("asio-control-panel-row");
    asioRow.setProperty("height", 0, nullptr);
    asioRow.setProperty("max-height", 0, nullptr);
    asioRow.setProperty("margin", "0 0 0 0", nullptr);
    asioRow.setProperty("visibility", false, nullptr);
    auto asioLbl = text(TRANS("Device Control Panel:"), "asio-control-panel-label");
    asioLbl.setProperty("flex-grow", 1.0, nullptr);
    asioLbl.setProperty("height", 22, nullptr);
    asioLbl.setProperty("font-size", 14, nullptr);
    asioLbl.setProperty("justification", "centred-left", nullptr);
    asioRow.appendChild(asioLbl, nullptr);

    auto asioBtn = button(TRANS("Open Control Panel"), "asio-control-panel-button");
    asioBtn.setProperty("width", 300, nullptr);
    asioBtn.setProperty("height", 24, nullptr);
    asioRow.appendChild(asioBtn, nullptr);

    content.appendChild(asioRow, nullptr);

    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeKeySignatureSectionTree() {
    auto card = flexColumn("key-sig-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Key Signature"), "key-sig-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("key-sig-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Row 1: Key Signature combo
    auto ksCombo = node("ComboBox", "key-signature-combo");
    ksCombo.setProperty("width", 300, nullptr);
    ksCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Key Signature:"), ksCombo, "key-signature-label"), nullptr);

    // Row 2: MIDI Transpose toggle
    auto transposeToggle = node("Checkbox", "midi-transpose-toggle");
    transposeToggle.setProperty("text", TRANS("MIDI Transpose"), nullptr);
    transposeToggle.setProperty("toggleable", true, nullptr);
    transposeToggle.setProperty("toggle-on-click", true, nullptr);
    transposeToggle.setProperty("width", 300, nullptr);
    transposeToggle.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("MIDI Transpose:"), transposeToggle, "midi-transpose-label"), nullptr);
    // Row 3: Channel Follow Key (16-channel Grid layout)
    auto followKeyArea = flexColumn("channel-follow-key-area");
    followKeyArea.setProperty("margin", "6 0 0 0", nullptr);

    auto followKeyLbl = text(TRANS("Channel Follow Key:"), "channel-follow-key-label");
    followKeyLbl.setProperty("height", 20, nullptr);
    followKeyLbl.setProperty("margin", "0 0 4 0", nullptr);
    followKeyLbl.setProperty("font-size", 14, nullptr);
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
        cb.setProperty("toggleable", true, nullptr);
        cb.setProperty("toggle-on-click", true, nullptr);
        cb.setProperty("height", 24, nullptr);
        grid.appendChild(cb, nullptr);
    }
    followKeyArea.appendChild(grid, nullptr);
    content.appendChild(followKeyArea, nullptr);
    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeKeyboardDisplaySectionTree() {
    auto card = flexColumn("keyboard-display-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Keyboard Display"), "keyboard-display-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("keyboard-display-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Colour Mode
    auto colourCombo = node("ComboBox", "colour-mode-combo");
    colourCombo.setProperty("width", 300, nullptr);
    colourCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Colour Mode:"), colourCombo, "colour-mode-label"), nullptr);

    // Note Display
    auto noteCombo = node("ComboBox", "note-display-combo");
    noteCombo.setProperty("width", 300, nullptr);
    noteCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Note Display:"), noteCombo, "note-display-label"), nullptr);

    // Fade Speed
    auto fadeSlider = node("Slider", "fade-speed-slider");
    fadeSlider.setProperty("width", 300, nullptr);
    fadeSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Fade Speed:"), fadeSlider, "fade-speed-label"), nullptr);

    // Instrument Filter
    auto filterCb = node("Checkbox", "instrument-filter-toggle");
    filterCb.setProperty("text", TRANS("Show MIDI/VSTi Instrument Filter"), nullptr);
    filterCb.setProperty("toggleable", true, nullptr);
    filterCb.setProperty("toggle-on-click", true, nullptr);
    filterCb.setProperty("width", 300, nullptr);
    filterCb.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Show MIDI/VSTi Instrument Filter:"), filterCb, "instrument-filter-label"),
                        nullptr);

    // Language
    auto langCombo = node("ComboBox", "language-combo");
    langCombo.setProperty("width", 300, nullptr);
    langCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Language:"), langCombo, "language-label"), nullptr);
    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeDiagnosticsSectionTree() {
    auto card = flexColumn("diagnostics-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Diagnostics"), "diagnostics-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("diagnostics-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    auto editor = node("ListEditor", "diagnostics-editor");
    editor.setProperty("height", 96, nullptr);
    editor.setProperty("focusable", true, nullptr);
    content.appendChild(editor, nullptr);

    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeSaveActionSectionTree() {
    auto row = flexRow("save-action-row");
    row.setProperty("justify-content", "flex-end", nullptr);
    row.setProperty("height", 36, nullptr);
    row.setProperty("margin", "4 0 16 0", nullptr);

    auto saveBtn = button(TRANS("Save"), "save-button");
    saveBtn.setProperty("width", 110, nullptr);
    saveBtn.setProperty("height", 28, nullptr);
    row.appendChild(saveBtn, nullptr);

    return row;
}

juce::ValueTree makeSettingsLayoutTree() {
    auto root = flexColumn("settings-root");
    root.setProperty("width", 680, nullptr);
    root.setProperty("height", 960, nullptr);
    root.setProperty("padding", "10", nullptr);
    root.appendChild(makeAudioDeviceSectionTree(), nullptr);
    root.appendChild(makeKeySignatureSectionTree(), nullptr);
    root.appendChild(makeKeyboardDisplaySectionTree(), nullptr);
    root.appendChild(makeDiagnosticsSectionTree(), nullptr);
    root.appendChild(makeSaveActionSectionTree(), nullptr);

    return root;
}

} // namespace devpiano::ui::jive
