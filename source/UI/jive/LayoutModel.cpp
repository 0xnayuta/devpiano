#include "LayoutModel.h"
#include "UI/jive/DesignTokens.h"

namespace devpiano::ui::jive {

namespace {

/// Helper: create a ValueTree node with a type name.
inline juce::ValueTree node(const juce::Identifier& type, const juce::String& id = {}) {
    auto t = juce::ValueTree(type);
    if (id.isNotEmpty())
        t.setProperty("id", id, nullptr);
    return t;
}

/// Helper: create a <Text> node. Height is intentionally NOT set here:
/// callers must give text an explicit height, or leave it "auto" so
/// align-items: stretch fills the row (TextComponent reports no intrinsic
/// size to JUCE's FlexBox).
inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    return t;
}

/// Helper: create a <Button> node with a text label.
///
/// JIVE's Button widget maps the node's "text" property to
/// juce::Button::setTitle (accessibility title), NOT setButtonText — the
/// visible label must be a Text child, centred by the button's flex layout.
inline juce::ValueTree button(const juce::String& label, const juce::String& id = {}) {
    auto t = node("Button", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("justify-content", "centre", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    // Required for the "Button" style-sheet border rule to render: JIVE's
    // BackgroundCanvas only strokes when the node carries a border-width.
    t.setProperty("border-width", "1", nullptr);

    auto labelText = text(label);
    labelText.setProperty("justification", "centred", nullptr);
    labelText.setProperty("word-wrap", "none", nullptr); // single-line labels
    t.appendChild(labelText, nullptr);

    return t;
}

/// Helper: create a <Component> flex row with centre alignment.
inline juce::ValueTree flexRow(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "row", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    return t;
}

/// Helper: create a <Component> flex row with stretch alignment — children
/// without an explicit cross-axis size fill the row height.
inline juce::ValueTree flexRowStretch(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "row", nullptr);
    t.setProperty("align-items", "stretch", nullptr);
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
    title.setProperty("height", 18, nullptr);
    title.setProperty("justification", "centred-left", nullptr);
    row.appendChild(title, nullptr);

    // Settings button — DrawableButton with gear icon, provided by the
    // "SettingsButton" component factory registered in MainComponent.
    auto settings = node("SettingsButton", "settings-btn");
    settings.setProperty("tooltip", TRANS("Settings"), nullptr);
    settings.setProperty("width", 36, nullptr);
    settings.setProperty("height", 36, nullptr);
    row.appendChild(settings, nullptr);

    return row;
}

// ═══════════════════════════════════════════════════════════════════════════
// StatusBar
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeStatusBarTree() {
    auto row = flexRowStretch("status-bar");
    row.setProperty("height", devpiano::jive::DesignTokens::get().statusBarHeight(), nullptr);
    row.setProperty("padding", "0 8 0 8", nullptr);
    // Top separator line — drawn by the StyleSheet border canvas.
    row.setProperty("border-width", "1 0 0 0", nullptr);

    // MIDI activity dot — custom component (StatusBarMidiDot) 6x6 px.
    auto dot = node("StatusBarMidiDot", "midi-dot");
    dot.setProperty("width", 6, nullptr);
    dot.setProperty("height", 6, nullptr);
    dot.setProperty("margin", "0 6 0 0", nullptr);
    row.appendChild(dot, nullptr);

    // Labels stretch to the row height and centre their text vertically.
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
    panel.setProperty("border-width", "1", nullptr);

    // ── Always-visible toolbar row: status | selector | filter | buttons ──
    auto actionRow = flexRow("plugin-action-row");
    actionRow.setProperty("height", 30, nullptr);
    // 2px bottom margin + 4px panel padding = same 6px row rhythm as the
    // preset-card rows (Export/Save) below.
    actionRow.setProperty("margin", "0 0 2 0", nullptr);

    auto status = text({}, "plugin-status-label");
    status.setProperty("flex-grow", 1.0, nullptr);
    status.setProperty("height", 26, nullptr); // explicit: toolbar row centres, not stretches
    status.setProperty("word-wrap", "none", nullptr); // single line, clipped like the native label
    status.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(status, nullptr);

    auto selector = node("ComboBox", "plugin-selector");
    selector.setProperty("width", 180, nullptr);
    selector.setProperty("height", 26, nullptr);
    selector.setProperty("margin", "0 6 0 0", nullptr);
    selector.setProperty("border-width", "1", nullptr);
    actionRow.appendChild(selector, nullptr);

    // Filter combo: populated programmatically by MainComponent
    // (initialiseUi / refreshPluginPanelTexts), NOT via declarative Option
    // children. JIVE's Option "selected" write-back (Option::selected calls
    // setSelectedId(0) when deselected) clears the combo on the second user
    // selection when items are also managed with clear()/addItem(), so this
    // must stay a bare ComboBox like plugin-selector.
    auto filter = node("ComboBox", "plugin-filter-combo");
    filter.setProperty("width", 100, nullptr);
    filter.setProperty("height", 26, nullptr);
    filter.setProperty("margin", "0 6 0 0", nullptr);
    filter.setProperty("border-width", "1", nullptr);
    actionRow.appendChild(filter, nullptr);

    auto loadBtn = button(TRANS("Load"), "load-btn");
    loadBtn.setProperty("width", 72, nullptr);
    loadBtn.setProperty("height", 26, nullptr);
    loadBtn.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(loadBtn, nullptr);

    auto unloadBtn = button(TRANS("Unload"), "unload-btn");
    unloadBtn.setProperty("width", 72, nullptr);
    unloadBtn.setProperty("height", 26, nullptr);
    unloadBtn.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(unloadBtn, nullptr);

    auto editorBtn = button(TRANS("Open Editor"), "editor-btn");
    editorBtn.setProperty("width", 124, nullptr);
    editorBtn.setProperty("height", 26, nullptr);
    editorBtn.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(editorBtn, nullptr);

    auto toggleBtn = button(juce::String::charToString(0x22EF), "toggle-btn");
    toggleBtn.setProperty("width", 30, nullptr);
    toggleBtn.setProperty("height", 26, nullptr);
    actionRow.appendChild(toggleBtn, nullptr);

    panel.appendChild(actionRow, nullptr);

    // ── Expandable area (height 0 when collapsed; 112 when expanded) ──
    auto expandedArea = flexColumn("plugin-expanded-area");
    expandedArea.setProperty("height", 0, nullptr);

    auto pathRow = flexRow("plugin-path-row");
    pathRow.setProperty("height", 30, nullptr);

    auto pathLabel = text(TRANS("VST3 Path"), "plugin-path-label");
    pathLabel.setProperty("width", 80, nullptr);
    pathLabel.setProperty("height", 26, nullptr);
    pathRow.appendChild(pathLabel, nullptr);

    auto pathEditor = node("PathEditor", "plugin-path-editor");
    pathEditor.setProperty("flex-grow", 1.0, nullptr);
    pathEditor.setProperty("height", 26, nullptr);
    pathEditor.setProperty("border-width", "1", nullptr);
    pathRow.appendChild(pathEditor, nullptr);

    auto browseBtn = button("...", "browse-btn");
    browseBtn.setProperty("width", 40, nullptr);
    browseBtn.setProperty("height", 26, nullptr);
    browseBtn.setProperty("margin", "0 0 0 6", nullptr);
    pathRow.appendChild(browseBtn, nullptr);

    auto scanBtn = button(TRANS("Scan VST3"), "scan-btn");
    scanBtn.setProperty("width", 96, nullptr);
    scanBtn.setProperty("height", 26, nullptr);
    scanBtn.setProperty("margin", "0 0 0 6", nullptr);
    pathRow.appendChild(scanBtn, nullptr);

    expandedArea.appendChild(pathRow, nullptr);

    auto listEditor = node("ListEditor", "plugin-list-editor");
    listEditor.setProperty("flex-grow", 1.0, nullptr);
    listEditor.setProperty("margin", "6 0 0 0", nullptr);
    listEditor.setProperty("border-width", "1", nullptr);
    expandedArea.appendChild(listEditor, nullptr);

    panel.appendChild(expandedArea, nullptr);

    return panel;
}

// ═══════════════════════════════════════════════════════════════════════════
// ControlsPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeControlsPanelTree() {
    auto panel = flexRowStretch("controls-panel");
    panel.setProperty("margin", "0 0 8 0", nullptr);

    const auto makeTextBtn = [](const juce::String& label, const juce::String& id, const juce::String& margin = "0") {
        auto btn = button(label, id);
        btn.setProperty("margin", margin, nullptr);
        btn.setProperty("flex-grow", 1.0, nullptr);
        btn.setProperty("height", 26, nullptr);
        return btn;
    };

    const auto makeIconBtn = [](const juce::String& type, const juce::String& id, const juce::String& margin = "0") {
        auto btn = node(type, id);
        btn.setProperty("margin", margin, nullptr);
        btn.setProperty("flex-grow", 1.0, nullptr);
        btn.setProperty("height", 38, nullptr);
        btn.setProperty("border-width", "1", nullptr);
        return btn;
    };

    // ═════════════════════════════════════════════════════════════════════════
    // Card 1: Presets & Performance Files (Left Card)
    // ═════════════════════════════════════════════════════════════════════════
    auto presetCard = flexColumn("preset-card");
    presetCard.setProperty("width", 270, nullptr);
    presetCard.setProperty("flex-shrink", 0.0, nullptr);
    presetCard.setProperty("padding", "10", nullptr);
    presetCard.setProperty("margin", "0 10 0 0", nullptr);
    presetCard.setProperty("border-width", "1", nullptr);

    auto presetHeader = text(TRANS("Performance Preset"), "preset-card-title");
    presetHeader.setProperty("height", 16, nullptr);
    presetHeader.setProperty("margin", "0 0 6 0", nullptr);
    presetCard.appendChild(presetHeader, nullptr);

    auto presetCombo = node("ComboBox", "preset-combo");
    presetCombo.setProperty("height", 28, nullptr);
    presetCombo.setProperty("margin", "0 0 8 0", nullptr);
    presetCombo.setProperty("border-width", "1", nullptr);
    presetCard.appendChild(presetCombo, nullptr);

    auto presetBtnRow = flexRow("preset-btn-row");
    presetBtnRow.setProperty("height", 26, nullptr);
    presetBtnRow.setProperty("margin", "0 0 12 0", nullptr);
    presetBtnRow.appendChild(makeTextBtn(TRANS("New"), "save-preset-btn", "0 6 0 0"), nullptr);
    presetBtnRow.appendChild(makeTextBtn(TRANS("Rename"), "rename-preset-btn", "0 6 0 0"), nullptr);
    presetBtnRow.appendChild(makeTextBtn(TRANS("Delete"), "delete-preset-btn", "0"), nullptr);
    presetCard.appendChild(presetBtnRow, nullptr);

    // Spacer between presets and file actions
    auto spacer = node("Component");
    spacer.setProperty("flex-grow", 1.0, nullptr);
    presetCard.appendChild(spacer, nullptr);

    auto fileRow1 = flexRow("file-row-1");
    fileRow1.setProperty("height", 26, nullptr);
    fileRow1.setProperty("margin", "0 0 6 0", nullptr);
    fileRow1.appendChild(makeTextBtn(TRANS("Export"), "export-midi-btn", "0 6 0 0"), nullptr);
    fileRow1.appendChild(makeTextBtn(TRANS("Import"), "import-midi-btn", "0"), nullptr);
    presetCard.appendChild(fileRow1, nullptr);

    auto fileRow2 = flexRow("file-row-2");
    fileRow2.setProperty("height", 26, nullptr);
    fileRow2.setProperty("margin", "0 0 6 0", nullptr);
    fileRow2.appendChild(makeTextBtn(TRANS("Save"), "save-perf-btn", "0 6 0 0"), nullptr);
    fileRow2.appendChild(makeTextBtn(TRANS("Open"), "open-perf-btn", "0"), nullptr);
    presetCard.appendChild(fileRow2, nullptr);

    auto fileRow3 = flexRow("file-row-3");
    fileRow3.setProperty("height", 24, nullptr);
    fileRow3.appendChild(makeTextBtn(TRANS("Export WAV"), "export-wav-btn", "0 6 0 0"), nullptr);
    fileRow3.appendChild(makeTextBtn(TRANS("Recent"), "recent-btn", "0 6 0 0"), nullptr);
    fileRow3.appendChild(makeTextBtn(TRANS("Info"), "song-info-btn", "0"), nullptr);
    presetCard.appendChild(fileRow3, nullptr);

    panel.appendChild(presetCard, nullptr);

    // ═════════════════════════════════════════════════════════════════════════
    // Card 2: Core Sound & ADSR Envelope (Center Card)
    // ═════════════════════════════════════════════════════════════════════════
    auto adsrCard = flexColumn("adsr-card");
    adsrCard.setProperty("flex-grow", 2.0, nullptr);
    adsrCard.setProperty("padding", "10", nullptr);
    adsrCard.setProperty("margin", "0 10 0 0", nullptr);
    adsrCard.setProperty("border-width", "1", nullptr);

    // 5 Rotary Knobs (Volume, Attack, Decay, Sustain, Release)
    auto knobsRow = flexRow("knobs-row");
    knobsRow.setProperty("height", 72, nullptr);
    knobsRow.setProperty("justify-content", "space-around", nullptr);
    knobsRow.setProperty("margin", "0 0 8 0", nullptr);

    const auto makeKnob = [](const juce::String& id, const juce::String& labelId, const juce::String& labelText) {
        auto wrapper = node("Component");
        wrapper.setProperty("display", "flex", nullptr);
        wrapper.setProperty("flex-direction", "column", nullptr);
        wrapper.setProperty("align-items", "centre", nullptr);
        wrapper.setProperty("flex-grow", 1.0, nullptr);

        auto lbl = text(labelText, labelId);
        lbl.setProperty("height", 14, nullptr);
        lbl.setProperty("margin", "0 0 2 0", nullptr);
        wrapper.appendChild(lbl, nullptr);

        auto knob = node("DevKnob", id);
        knob.setProperty("width", 48, nullptr);
        knob.setProperty("height", 52, nullptr);
        wrapper.appendChild(knob, nullptr);

        return wrapper;
    };

    knobsRow.appendChild(makeKnob("volume-knob", "volume-label", TRANS("Volume")), nullptr);
    knobsRow.appendChild(makeKnob("attack-knob", "attack-label", TRANS("Attack")), nullptr);
    knobsRow.appendChild(makeKnob("decay-knob", "decay-label", TRANS("Decay")), nullptr);
    knobsRow.appendChild(makeKnob("sustain-knob", "sustain-label", TRANS("Sustain")), nullptr);
    knobsRow.appendChild(makeKnob("release-knob", "release-label", TRANS("Release")), nullptr);
    adsrCard.appendChild(knobsRow, nullptr);

    auto adsrTitle = text(TRANS("ADSR Curve"), "adsr-curve-title");
    adsrTitle.setProperty("height", 14, nullptr);
    adsrTitle.setProperty("margin", "0 0 6 0", nullptr);
    adsrCard.appendChild(adsrTitle, nullptr);

    auto curve = node("AdsrCurve", "adsr-curve");
    curve.setProperty("flex-grow", 1.0, nullptr);
    curve.setProperty("min-height", 70, nullptr);
    adsrCard.appendChild(curve, nullptr);

    panel.appendChild(adsrCard, nullptr);

    // ═════════════════════════════════════════════════════════════════════════
    // Card 3: Transport Controls & Playback Speed (Right Card)
    // ═════════════════════════════════════════════════════════════════════════
    auto transportCard = flexColumn("transport-card");
    transportCard.setProperty("width", 230, nullptr);
    transportCard.setProperty("flex-shrink", 0.0, nullptr);
    transportCard.setProperty("padding", "10", nullptr);
    transportCard.setProperty("border-width", "1", nullptr);

    auto transportHeader = text(TRANS("Transport Controls"), "transport-card-title");
    transportHeader.setProperty("height", 16, nullptr);
    transportHeader.setProperty("margin", "0 0 6 0", nullptr);
    transportCard.appendChild(transportHeader, nullptr);

    // 2x2 Large Transport Buttons (Record, Play, Stop, Back to Start).
    // The header's settings button already covers Settings — no gear here.
    auto transportGrid1 = flexRow("transport-grid-1");
    transportGrid1.setProperty("height", 42, nullptr);
    transportGrid1.setProperty("margin", "0 0 6 0", nullptr);
    transportGrid1.appendChild(makeIconBtn("RecordButton", "record-btn", "0 6 0 0"), nullptr);
    transportGrid1.appendChild(makeIconBtn("PlayButton", "play-btn", "0"), nullptr);
    transportCard.appendChild(transportGrid1, nullptr);

    auto transportGrid2 = flexRow("transport-grid-2");
    transportGrid2.setProperty("height", 42, nullptr);
    transportGrid2.setProperty("margin", "0 0 8 0", nullptr);
    transportGrid2.appendChild(makeIconBtn("StopButton", "stop-btn", "0 6 0 0"), nullptr);
    transportGrid2.appendChild(makeIconBtn("BackButton", "back-btn", "0"), nullptr);
    transportCard.appendChild(transportGrid2, nullptr);

    // Speed Slider Area — horizontal slider matching the reference design
    auto speedHeader = text(TRANS("Playback Speed"), "speed-label");
    speedHeader.setProperty("height", 14, nullptr);
    speedHeader.setProperty("margin", "0 0 4 0", nullptr);
    transportCard.appendChild(speedHeader, nullptr);

    auto speedSlider = node("SpeedSlider", "speed-knob");
    speedSlider.setProperty("height", 36, nullptr);
    transportCard.appendChild(speedSlider, nullptr);

    panel.appendChild(transportCard, nullptr);

    return panel;
}

// ═══════════════════════════════════════════════════════════════════════════
// KeyboardArea
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeKeyboardAreaTree() {
    auto area = node("Component", "keyboard-area");
    area.setProperty("display", "flex", nullptr);
    area.setProperty("flex-direction", "column", nullptr);

    // KeyboardViewport (Viewport + CustomKeyboard) provided by the factory.
    auto ck = node("CustomKeyboard", "custom-keyboard");
    ck.setProperty("flex-grow", 1.0, nullptr);
    area.appendChild(ck, nullptr);

    return area;
}

// ═══════════════════════════════════════════════════════════════════════════
// Root layout
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeRootLayout() {
    // Id "window" matches the "#window" rule in style_sheets.json (background,
    // foreground, font-size) which acts as the window-level default that every
    // child inherits through JIVE's StyleSheet ancestor chain.
    auto root = flexColumn("window");
    root.setProperty("width", 1120, nullptr);
    root.setProperty("height", 760, nullptr);
    auto mainArea = flexColumn("main-area");
    mainArea.setProperty("flex-grow", 1.0, nullptr);
    mainArea.setProperty("padding", "16", nullptr);

    auto header = makeHeaderTree();
    header.setProperty("margin", "0 0 10 0", nullptr);
    mainArea.appendChild(header, nullptr);

    auto plugin = makePluginPanelTree();
    plugin.setProperty("height", 42, nullptr); // collapsed; setPluginPanelExpanded updates
    plugin.setProperty("margin", "0 0 12 0", nullptr);
    mainArea.appendChild(plugin, nullptr);

    auto contentRow = flexColumn("content-row");
    contentRow.setProperty("flex-grow", 1.0, nullptr);

    auto controls = makeControlsPanelTree();
    controls.setProperty("flex-grow", 1.0, nullptr);
    controls.setProperty("flex-shrink", 1.0, nullptr);
    controls.setProperty("min-height", 140, nullptr);
    controls.setProperty("margin", "0 0 8 0", nullptr);
    contentRow.appendChild(controls, nullptr);

    auto keyboard = makeKeyboardAreaTree();
    keyboard.setProperty("flex-grow", 1.0, nullptr);
    keyboard.setProperty("flex-shrink", 0.0, nullptr);
    keyboard.setProperty("min-height", 90, nullptr);
    keyboard.setProperty("max-height", 170, nullptr);
    keyboard.setProperty("height", 170, nullptr);
    contentRow.appendChild(keyboard, nullptr);
    mainArea.appendChild(contentRow, nullptr);
    root.appendChild(mainArea, nullptr);

    auto status = makeStatusBarTree();
    root.appendChild(status, nullptr);

    return root;
}

} // namespace devpiano::ui::jive
