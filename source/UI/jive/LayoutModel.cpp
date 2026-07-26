#include "LayoutModel.h"

namespace devpiano::ui::jive {

namespace {

/// Helper: create a ValueTree node with a type name.
inline juce::ValueTree node(const juce::Identifier& type, const juce::String& id = {}) {
    auto t = juce::ValueTree(type);
    if (id.isNotEmpty()) {
        t.setProperty("id", id, nullptr);
    }
    return t;
}

/// Helper: create a <Text> node with text content.
inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    return t;
}

/// Helper: create a <Button> node.
inline juce::ValueTree button(const juce::String& label, const juce::String& id = {}) {
    auto t = node("Button", id);
    t.setProperty("text", label, nullptr);
    return t;
}

/// Helper: create a <Component> flex row.
inline juce::ValueTree flexRow(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "row", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    return t;
}

/// Helper: create a <Component> flex column.
inline juce::ValueTree flexColumn(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "column", nullptr);
    return t;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// HeaderPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeHeaderTree() {
    auto row = flexRow("header");
    row.setProperty("height", 36, nullptr);
    row.setProperty("padding", "0 12 0 12", nullptr);

    auto title = text("devpiano", "title");
    title.setProperty("flex-grow", 1.0, nullptr);
    row.appendChild(title, nullptr);

    // Settings button — DrawableButton with gear icon, handled by AppView::createComponent
    auto settings = node("Button", "settings-btn");
    settings.setProperty("tooltip", "Settings", nullptr);
    settings.setProperty("width", 36, nullptr);
    settings.setProperty("height", 36, nullptr);
    row.appendChild(settings, nullptr);

    return row;
}

// ═══════════════════════════════════════════════════════════════════════════
// StatusBar
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeStatusBarTree() {
    auto row = flexRow("status-bar");
    row.setProperty("height", 22, nullptr);
    row.setProperty("padding", "0 8 0 8", nullptr);

    // MIDI activity dot — native Component, 6x6 px
    auto dot = node("StatusBarMidiDot", "midi-dot");
    dot.setProperty("width", 6, nullptr);
    dot.setProperty("height", 6, nullptr);
    dot.setProperty("margin", "0 6 0 0", nullptr);
    row.appendChild(dot, nullptr);

    auto pluginLabel = text({}, "plugin-name-label");
    pluginLabel.setProperty("flex-grow", 1.0, nullptr);
    row.appendChild(pluginLabel, nullptr);

    auto audioInfo = text({}, "audio-info-label");
    audioInfo.setProperty("flex-grow", 1.0, nullptr);
    row.appendChild(audioInfo, nullptr);

    auto timeLabel = text({}, "time-label");
    timeLabel.setProperty("width", 120, nullptr);
    timeLabel.setProperty("justification", "centred-right", nullptr);
    row.appendChild(timeLabel, nullptr);

    return row;
}

// ═══════════════════════════════════════════════════════════════════════════
// PluginPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makePluginPanelTree() {
    auto panel = flexColumn("plugin-panel");
    panel.setProperty("padding", "4 8 4 8", nullptr);

    // ── Always-visible row: plugin selector + action buttons ──
    auto actionRow = flexRow("plugin-action-row");
    actionRow.setProperty("height", 32, nullptr);

    auto selector = node("ComboBox", "plugin-selector");
    selector.setProperty("flex-grow", 1.0, nullptr);
    selector.setProperty("margin", "0 4 0 0", nullptr);
    actionRow.appendChild(selector, nullptr);

    auto loadBtn = button("Load", "load-btn");
    loadBtn.setProperty("width", 60, nullptr);
    loadBtn.setProperty("margin", "2", nullptr);
    actionRow.appendChild(loadBtn, nullptr);

    auto unloadBtn = button("Unload", "unload-btn");
    unloadBtn.setProperty("width", 60, nullptr);
    unloadBtn.setProperty("margin", "2", nullptr);
    actionRow.appendChild(unloadBtn, nullptr);

    auto editorBtn = button("Open Editor", "editor-btn");
    editorBtn.setProperty("width", 90, nullptr);
    editorBtn.setProperty("margin", "2", nullptr);
    actionRow.appendChild(editorBtn, nullptr);

    auto toggleBtn = button("...", "toggle-btn");
    toggleBtn.setProperty("width", 28, nullptr);
    toggleBtn.setProperty("margin", "2", nullptr);
    actionRow.appendChild(toggleBtn, nullptr);

    panel.appendChild(actionRow, nullptr);

    // ── Expandable rows (visible only when expanded) ──
    auto expandedArea = flexColumn("plugin-expanded-area");
    expandedArea.setProperty("visibility", false, nullptr);

    // Scan + browse row
    auto scanRow = flexRow("plugin-scan-row");
    scanRow.setProperty("height", 28, nullptr);

    scanRow.appendChild(button("Scan VST3", "scan-btn"), nullptr);
    scanRow.appendChild(button("...", "browse-btn"), nullptr);

    auto pathEditor = node("Editor", "plugin-path-editor");
    pathEditor.setProperty("flex-grow", 1.0, nullptr);
    scanRow.appendChild(pathEditor, nullptr);

    expandedArea.appendChild(scanRow, nullptr);

    // Plugin list editor
    auto listEditor = node("Editor", "plugin-list-editor");
    listEditor.setProperty("flex-grow", 1.0, nullptr);
    listEditor.setProperty("min-height", 60, nullptr);
    expandedArea.appendChild(listEditor, nullptr);

    panel.appendChild(expandedArea, nullptr);

    return panel;
}

// ═══════════════════════════════════════════════════════════════════════════
// ControlsPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeControlsPanelTree() {
    auto panel = flexColumn("controls-panel");
    panel.setProperty("padding", "8", nullptr);

    // ── Row 1: ADSR knobs (6 knobs: volume + 5 ADSR) ──
    auto knobsRow = flexRow("knobs-row");
    knobsRow.setProperty("justify-content", "space-around", nullptr);
    knobsRow.setProperty("height", 72, nullptr);

    auto makeKnob = [](const juce::String& id, const juce::String& label) {
        auto wrapper = flexColumn();
        wrapper.setProperty("align-items", "centre", nullptr);

        auto lbl = text(label);
        lbl.setProperty("height", 14, nullptr);
        wrapper.appendChild(lbl, nullptr);

        auto knob = node("Knob", id);
        knob.setProperty("width", 36, nullptr);
        knob.setProperty("height", 36, nullptr);
        knob.setProperty("min", 0.0, nullptr);
        knob.setProperty("max", 1.0, nullptr);
        knob.setProperty("interval", 0.01, nullptr);
        wrapper.appendChild(knob, nullptr);

        return wrapper;
    };

    knobsRow.appendChild(makeKnob("volume-knob", "Vol"), nullptr);
    knobsRow.appendChild(makeKnob("attack-knob", "A"), nullptr);
    knobsRow.appendChild(makeKnob("decay-knob", "D"), nullptr);
    knobsRow.appendChild(makeKnob("sustain-knob", "S"), nullptr);
    knobsRow.appendChild(makeKnob("release-knob", "R"), nullptr);
    knobsRow.appendChild(makeKnob("speed-knob", "Spd"), nullptr);
    panel.appendChild(knobsRow, nullptr);

    // ── Row 2: ADSR curve (native Component) ──
    auto adsrCurve = node("AdsrCurve", "adsr-curve");
    adsrCurve.setProperty("height", 60, nullptr);
    panel.appendChild(adsrCurve, nullptr);

    // ── Row 3: Preset selector ──
    auto presetRow = flexRow("preset-row");
    presetRow.setProperty("height", 28, nullptr);

    auto presetLabel = text("Preset:", "preset-label");
    presetLabel.setProperty("width", 60, nullptr);
    presetRow.appendChild(presetLabel, nullptr);

    auto presetCombo = node("ComboBox", "preset-combo");
    presetCombo.setProperty("flex-grow", 1.0, nullptr);
    presetRow.appendChild(presetCombo, nullptr);

    presetRow.appendChild(button("Save As New", "save-preset-btn"), nullptr);
    presetRow.appendChild(button("Rename", "rename-preset-btn"), nullptr);
    presetRow.appendChild(button("Delete", "delete-preset-btn"), nullptr);
    panel.appendChild(presetRow, nullptr);

    // ── Row 4: Transport + Export + Performance buttons ──
    auto transportRow = flexRow("transport-row");
    transportRow.setProperty("height", 28, nullptr);

    // Transport icons — DrawableButtons, handled by AppView::createComponent
    auto makeTransportBtn = [](const juce::String& id, const juce::String& tooltip) {
        auto btn = node("Button", id);
        btn.setProperty("tooltip", tooltip, nullptr);
        btn.setProperty("width", 28, nullptr);
        btn.setProperty("height", 28, nullptr);
        return btn;
    };

    transportRow.appendChild(makeTransportBtn("record-btn", "Record"), nullptr);
    transportRow.appendChild(makeTransportBtn("play-btn", "Play"), nullptr);
    transportRow.appendChild(makeTransportBtn("stop-btn", "Stop"), nullptr);
    transportRow.appendChild(makeTransportBtn("back-btn", "Back to Start"), nullptr);
    panel.appendChild(transportRow, nullptr);

    // ── Row 5: Export + File buttons ──
    auto fileRow = flexRow("file-row");
    fileRow.setProperty("height", 28, nullptr);

    fileRow.appendChild(button("Export MIDI", "export-midi-btn"), nullptr);
    fileRow.appendChild(button("Export WAV", "export-wav-btn"), nullptr);
    fileRow.appendChild(button("Import MIDI", "import-midi-btn"), nullptr);
    fileRow.appendChild(button("Save", "save-perf-btn"), nullptr);
    fileRow.appendChild(button("Open", "open-perf-btn"), nullptr);
    fileRow.appendChild(button("Song Info", "song-info-btn"), nullptr);
    fileRow.appendChild(button("Recent", "recent-btn"), nullptr);
    panel.appendChild(fileRow, nullptr);

    return panel;
}

// ═══════════════════════════════════════════════════════════════════════════
// KeyboardArea
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeKeyboardAreaTree() {
    auto area = node("Component", "keyboard-area");
    area.setProperty("display", "flex", nullptr);
    area.setProperty("flex-grow", 1.0, nullptr);

    auto ck = node("CustomKeyboard", "custom-keyboard");
    ck.setProperty("flex-grow", 1.0, nullptr);
    area.appendChild(ck, nullptr);

    return area;
}

// ═══════════════════════════════════════════════════════════════════════════
// Root layout
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeRootLayout() {
    auto root = flexColumn("root");
    root.setProperty("width", "100%", nullptr);
    root.setProperty("height", "100%", nullptr);

    // ── Header ──
    root.appendChild(makeHeaderTree(), nullptr);

    // ── Plugin panel (sidebar) ──
    root.appendChild(makePluginPanelTree(), nullptr);

    // ── Content area: ControlsPanel + KeyboardPanel side-by-side ──
    auto contentRow = flexRow("content-row");
    contentRow.setProperty("flex-grow", 1.0, nullptr);

    // Left sidebar: Controls
    auto sidebar = makeControlsPanelTree();
    sidebar.setProperty("id", "sidebar", nullptr);
    sidebar.setProperty("width", 280, nullptr);
    sidebar.setProperty("min-width", 240, nullptr);
    contentRow.appendChild(sidebar, nullptr);

    // Right: Keyboard
    contentRow.appendChild(makeKeyboardAreaTree(), nullptr);

    root.appendChild(contentRow, nullptr);

    // ── Status bar ──
    root.appendChild(makeStatusBarTree(), nullptr);

    return root;
}

} // namespace devpiano::ui::jive
