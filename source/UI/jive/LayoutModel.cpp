#include "LayoutModel.h"
#include "UI/jive/DesignTokens.h"

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

/// Helper: create a <Text> node. Height is intentionally NOT set here:
/// callers must give text an explicit height, or leave it "auto" so
/// align-items: stretch fills the row (TextComponent reports no intrinsic
/// size to JUCE's FlexBox).
inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    // Semantic title for inspector/accessibility: defaults to the displayed
    // text; dynamic labels override it at the call site.
    t.setProperty("title", content, nullptr);
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
    // Semantic title for inspector/accessibility: the visible label.
    t.setProperty("title", label, nullptr);
    // Required for the "Button" style-sheet border rule to render: JIVE's
    // BackgroundCanvas only strokes when the node carries a border-width.
    t.setProperty("border-width", "1", nullptr);

    auto labelText = text(label, id.isNotEmpty() ? id + "-text" : juce::String {});
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
    t.setProperty("align-items", "stretch", nullptr);
    return t;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// HeaderPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeHeaderTree() {
    auto row = flexRow("header");
    row.setProperty("title", TRANS("Header"), nullptr);
    row.setProperty("height", 36, nullptr);
    row.setProperty("padding", "0 12 0 12", nullptr);

    auto title = text("devpiano", "title");
    title.setProperty("flex-grow", 1.0, nullptr);
    title.setProperty("height", 28, nullptr);
    title.setProperty("justification", "centred-left", nullptr);
    row.appendChild(title, nullptr);

    // Settings button — DrawableButton with gear icon, provided by the
    // "SettingsButton" component factory registered in MainComponent.
    auto settings = node("SettingsButton", "settings-btn");
    settings.setProperty("title", TRANS("Settings"), nullptr);
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
    row.setProperty("title", TRANS("Status Bar"), nullptr);
    row.setProperty("height", devpiano::jive::DesignTokens::get().statusBarHeight(), nullptr);
    row.setProperty("padding", "0 8 0 8", nullptr);
    // Top separator line — drawn by the StyleSheet border canvas.
    row.setProperty("border-width", "1 0 0 0", nullptr);

    // Left section: MIDI activity dot + plugin/preset label (1/3 flex, left-aligned)
    auto leftSection = flexRowStretch("status-left");
    leftSection.setProperty("flex-grow", 1.0, nullptr);
    leftSection.setProperty("flex-shrink", 1.0, nullptr);

    auto dot = node("StatusBarMidiDot", "midi-dot");
    dot.setProperty("title", TRANS("MIDI Activity"), nullptr);
    dot.setProperty("width", 7, nullptr);
    dot.setProperty("height", 7, nullptr);
    dot.setProperty("align-self", "centre", nullptr);
    dot.setProperty("margin", "0 6 0 2", nullptr);
    leftSection.appendChild(dot, nullptr);

    auto pluginLabel = text({}, "plugin-name-label");
    pluginLabel.setProperty("title", TRANS("Plugin Name"), nullptr);
    pluginLabel.setProperty("flex-grow", 1.0, nullptr);
    pluginLabel.setProperty("flex-shrink", 1.0, nullptr);
    pluginLabel.setProperty("justification", "centred-left", nullptr);
    leftSection.appendChild(pluginLabel, nullptr);

    row.appendChild(leftSection, nullptr);

    // Centre section: audio driver & performance monitoring (1/3 flex, mathematically centred at 50%)
    auto audioInfo = text({}, "audio-info-label");
    audioInfo.setProperty("title", TRANS("Audio Info"), nullptr);
    audioInfo.setProperty("flex-grow", 1.0, nullptr);
    audioInfo.setProperty("flex-shrink", 1.0, nullptr);
    audioInfo.setProperty("justification", "centred", nullptr);
    row.appendChild(audioInfo, nullptr);

    // Right section: key signature, transpose, layout (1/3 flex, right-aligned)
    auto timeLabel = text({}, "time-label");
    timeLabel.setProperty("title", TRANS("Time"), nullptr);
    timeLabel.setProperty("flex-grow", 1.0, nullptr);
    timeLabel.setProperty("flex-shrink", 1.0, nullptr);
    timeLabel.setProperty("justification", "centred-right", nullptr);
    row.appendChild(timeLabel, nullptr);

    return row;
}

// ═══════════════════════════════════════════════════════════════════════════
// PluginPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makePluginPanelTree() {
    auto panel = flexColumn("plugin-panel");
    panel.setProperty("title", TRANS("Plugin Panel"), nullptr);
    panel.setProperty("padding", "4 8 4 8", nullptr);
    panel.setProperty("border-width", "1", nullptr);

    // ── Always-visible toolbar row: status | selector | filter | buttons ──
    auto actionRow = flexRow("plugin-action-row");
    actionRow.setProperty("title", TRANS("Plugin Actions"), nullptr);
    actionRow.setProperty("height", 30, nullptr);
    // 2px bottom margin + 4px panel padding = same 6px row rhythm as the
    // preset-card rows (Export/Save) below.
    actionRow.setProperty("margin", "0 0 2 0", nullptr);

    auto status = text({}, "plugin-status-label");
    status.setProperty("title", TRANS("Plugin Status"), nullptr);
    status.setProperty("flex-grow", 1.0, nullptr);
    status.setProperty("height", 26, nullptr); // explicit: toolbar row centres, not stretches
    status.setProperty("word-wrap", "none", nullptr); // single line, clipped like the native label
    status.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(status, nullptr);

    auto selector = node("ComboBox", "plugin-selector");
    selector.setProperty("title", TRANS("Plugin Selector"), nullptr);
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
    filter.setProperty("title", TRANS("Plugin Filter"), nullptr);
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
    toggleBtn.setProperty("title", TRANS("Toggle Plugin Panel"), nullptr);
    toggleBtn.setProperty("width", 30, nullptr);
    toggleBtn.setProperty("height", 26, nullptr);
    actionRow.appendChild(toggleBtn, nullptr);

    panel.appendChild(actionRow, nullptr);

    // ── Expandable area (height 0 when collapsed; 112 when expanded) ──
    auto expandedArea = flexColumn("plugin-expanded-area");
    expandedArea.setProperty("title", TRANS("Plugin Expanded Area"), nullptr);
    expandedArea.setProperty("height", 0, nullptr);

    auto pathRow = flexRow("plugin-path-row");
    pathRow.setProperty("title", TRANS("Plugin Path Row"), nullptr);
    pathRow.setProperty("height", 30, nullptr);

    auto pathLabel = text(TRANS("VST3 Path"), "plugin-path-label");
    pathLabel.setProperty("width", 80, nullptr);
    pathLabel.setProperty("height", 26, nullptr);
    pathRow.appendChild(pathLabel, nullptr);

    auto pathEditor = node("PathEditor", "plugin-path-editor");
    pathEditor.setProperty("title", TRANS("VST3 Path Editor"), nullptr);
    pathEditor.setProperty("flex-grow", 1.0, nullptr);
    pathEditor.setProperty("height", 26, nullptr);
    pathEditor.setProperty("border-width", "1", nullptr);
    pathEditor.setProperty("focusable", true, nullptr);
    pathEditor.setProperty("cursor", "text", nullptr);
    pathRow.appendChild(pathEditor, nullptr);

    auto browseBtn = button("...", "browse-btn");
    browseBtn.setProperty("title", TRANS("Browse"), nullptr);
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
    listEditor.setProperty("title", TRANS("Plugin List"), nullptr);
    listEditor.setProperty("flex-grow", 1.0, nullptr);
    listEditor.setProperty("margin", "6 0 0 0", nullptr);
    listEditor.setProperty("border-width", "1", nullptr);
    listEditor.setProperty("focusable", true, nullptr);
    expandedArea.appendChild(listEditor, nullptr);

    panel.appendChild(expandedArea, nullptr);

    return panel;
}

// ═══════════════════════════════════════════════════════════════════════════
// ControlsPanel
// ═══════════════════════════════════════════════════════════════════════════

juce::ValueTree makeControlsPanelTree() {
    auto panel = flexRowStretch("controls-panel");
    panel.setProperty("title", TRANS("Controls"), nullptr);
    panel.setProperty("margin", "0 0 8 0", nullptr);

    const auto makeTextBtn = [](const juce::String& label, const juce::String& id, const juce::String& margin = "0") {
        auto btn = button(label, id);
        btn.setProperty("margin", margin, nullptr);
        btn.setProperty("flex-grow", 1.0, nullptr);
        btn.setProperty("height", 26, nullptr);
        return btn;
    };

    const auto makeIconBtn = [](const juce::String& type, const juce::String& id, const juce::String& title,
                                const juce::String& margin = "0") {
        auto btn = node(type, id);
        btn.setProperty("title", title, nullptr);
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
    presetCard.setProperty("title", TRANS("Preset Card"), nullptr);
    presetCard.setProperty("width", 270, nullptr);
    presetCard.setProperty("flex-shrink", 0.0, nullptr);
    presetCard.setProperty("padding", "10", nullptr);
    presetCard.setProperty("margin", "0 10 0 0", nullptr);
    presetCard.setProperty("border-width", "1", nullptr);

    auto presetHeader = text(TRANS("Performance Preset"), "preset-card-title");
    presetHeader.setProperty("width", "100%", nullptr);
    presetHeader.setProperty("height", 20, nullptr);
    presetHeader.setProperty("margin", "0 0 6 0", nullptr);
    presetHeader.setProperty("justification", "centred-left", nullptr);
    presetHeader.setProperty("word-wrap", "none", nullptr);
    presetCard.appendChild(presetHeader, nullptr);

    auto presetCombo = node("ComboBox", "preset-combo");
    presetCombo.setProperty("title", TRANS("Performance Preset"), nullptr);
    presetCombo.setProperty("height", 28, nullptr);
    // 固定与下方 preset-btn-row 三按钮合计等宽（card 270 - 2*10 padding）。
    // JIVE ComboBox 无显式 width 时回退到 50px（jive_ComboBox 的 auto 默认），
    // 会渲染成窄条并裁剪占位文本/选项。
    presetCombo.setProperty("width", 250, nullptr);
    presetCombo.setProperty("margin", "0 0 8 0", nullptr);
    presetCombo.setProperty("border-width", "1", nullptr);
    presetCard.appendChild(presetCombo, nullptr);

    auto presetBtnRow = flexRow("preset-btn-row");
    presetBtnRow.setProperty("title", TRANS("Preset Actions"), nullptr);
    presetBtnRow.setProperty("height", 26, nullptr);
    presetBtnRow.setProperty("margin", "0 0 12 0", nullptr);
    presetBtnRow.appendChild(makeTextBtn(TRANS("New"), "save-preset-btn", "0 6 0 0"), nullptr);
    presetBtnRow.appendChild(makeTextBtn(TRANS("Rename"), "rename-preset-btn", "0 6 0 0"), nullptr);
    presetBtnRow.appendChild(makeTextBtn(TRANS("Delete"), "delete-preset-btn", "0"), nullptr);
    presetCard.appendChild(presetBtnRow, nullptr);

    // Spacer between presets and file actions
    auto spacer = node("Component", "preset-spacer");
    spacer.setProperty("title", TRANS("Spacer"), nullptr);
    spacer.setProperty("flex-grow", 1.0, nullptr);
    presetCard.appendChild(spacer, nullptr);

    auto fileRow1 = flexRow("file-row-1");
    fileRow1.setProperty("title", TRANS("Export Row"), nullptr);
    fileRow1.setProperty("height", 26, nullptr);
    fileRow1.setProperty("margin", "0 0 6 0", nullptr);
    fileRow1.appendChild(makeTextBtn(TRANS("Export"), "export-midi-btn", "0 6 0 0"), nullptr);
    fileRow1.appendChild(makeTextBtn(TRANS("Import"), "import-midi-btn", "0"), nullptr);
    presetCard.appendChild(fileRow1, nullptr);

    auto fileRow2 = flexRow("file-row-2");
    fileRow2.setProperty("title", TRANS("File Row 2"), nullptr);
    fileRow2.setProperty("height", 26, nullptr);
    fileRow2.setProperty("margin", "0 0 6 0", nullptr);
    fileRow2.appendChild(makeTextBtn(TRANS("Save"), "save-perf-btn", "0 6 0 0"), nullptr);
    fileRow2.appendChild(makeTextBtn(TRANS("Open"), "open-perf-btn", "0"), nullptr);
    presetCard.appendChild(fileRow2, nullptr);

    auto fileRow3 = flexRow("file-row-3");
    fileRow3.setProperty("title", TRANS("File Row 3"), nullptr);
    fileRow3.setProperty("height", 26, nullptr);
    fileRow3.appendChild(makeTextBtn(TRANS("Export WAV"), "export-wav-btn", "0 6 0 0"), nullptr);
    fileRow3.appendChild(makeTextBtn(TRANS("Recent"), "recent-btn", "0 6 0 0"), nullptr);
    fileRow3.appendChild(makeTextBtn(TRANS("Info"), "song-info-btn", "0"), nullptr);
    presetCard.appendChild(fileRow3, nullptr);
    panel.appendChild(presetCard, nullptr);

    // ═════════════════════════════════════════════════════════════════════════
    // Card 2: Core Sound & ADSR Envelope (Center Card)
    // ═════════════════════════════════════════════════════════════════════════
    auto adsrCard = flexColumn("adsr-card");
    adsrCard.setProperty("title", TRANS("ADSR Card"), nullptr);
    adsrCard.setProperty("flex-grow", 2.0, nullptr);
    adsrCard.setProperty("padding", "10", nullptr);
    adsrCard.setProperty("margin", "0 10 0 0", nullptr);
    adsrCard.setProperty("border-width", "1", nullptr);

    const auto makeKnob = [](const juce::String& id, const juce::String& labelId, const juce::String& labelText) {
        auto wrapper = node("Component", id + "-wrap");
        wrapper.setProperty("title", labelText, nullptr);
        wrapper.setProperty("display", "flex", nullptr);
        wrapper.setProperty("flex-direction", "column", nullptr);
        wrapper.setProperty("align-items", "centre", nullptr);
        wrapper.setProperty("flex-grow", 1.0, nullptr);

        auto lbl = text(labelText, labelId);
        lbl.setProperty("height", 14, nullptr);
        lbl.setProperty("margin", "0 0 2 0", nullptr);
        wrapper.appendChild(lbl, nullptr);

        auto knob = node("DevKnob", id);
        knob.setProperty("title", labelText, nullptr);
        knob.setProperty("width", 48, nullptr);
        knob.setProperty("height", 52, nullptr);
        wrapper.appendChild(knob, nullptr);

        return wrapper;
    };

    // 第一行（上四：音量与钢琴音色参数：Volume, Brightness, Hammer, Resonance）
    auto pianoRow = flexRow("piano-row");
    pianoRow.setProperty("title", TRANS("Piano Row"), nullptr);
    pianoRow.setProperty("height", 72, nullptr);
    pianoRow.setProperty("justify-content", "space-around", nullptr);
    pianoRow.setProperty("margin", "0 0 8 0", nullptr);
    pianoRow.appendChild(makeKnob("volume-knob", "volume-label", TRANS("Volume")), nullptr);
    pianoRow.appendChild(makeKnob("brightness-knob", "brightness-label", TRANS("Brightness")), nullptr);
    pianoRow.appendChild(makeKnob("hardness-knob", "hardness-label", TRANS("Hammer")), nullptr);
    pianoRow.appendChild(makeKnob("resonance-knob", "resonance-label", TRANS("Resonance")), nullptr);
    adsrCard.appendChild(pianoRow, nullptr);

    // 第二行（下四：ADSR 包络参数：Attack, Decay, Sustain, Release）
    auto knobsRow = flexRow("knobs-row");
    knobsRow.setProperty("title", TRANS("Knobs Row"), nullptr);
    knobsRow.setProperty("height", 72, nullptr);
    knobsRow.setProperty("justify-content", "space-around", nullptr);
    knobsRow.setProperty("margin", "0 0 8 0", nullptr);
    knobsRow.appendChild(makeKnob("attack-knob", "attack-label", TRANS("Attack")), nullptr);
    knobsRow.appendChild(makeKnob("decay-knob", "decay-label", TRANS("Decay")), nullptr);
    knobsRow.appendChild(makeKnob("sustain-knob", "sustain-label", TRANS("Sustain")), nullptr);
    knobsRow.appendChild(makeKnob("release-knob", "release-label", TRANS("Release")), nullptr);
    adsrCard.appendChild(knobsRow, nullptr);
    auto adsrTitle = text(TRANS("ADSR Curve"), "adsr-curve-title");
    adsrTitle.setProperty("width", "100%", nullptr);
    adsrTitle.setProperty("height", 18, nullptr);
    adsrTitle.setProperty("margin", "0 0 6 0", nullptr);
    adsrTitle.setProperty("justification", "centred-left", nullptr);
    adsrTitle.setProperty("word-wrap", "none", nullptr);
    adsrCard.appendChild(adsrTitle, nullptr);

    auto curve = node("AdsrCurve", "adsr-curve");
    curve.setProperty("title", TRANS("ADSR Curve"), nullptr);
    curve.setProperty("flex-grow", 1.0, nullptr);
    curve.setProperty("min-height", 70, nullptr);
    adsrCard.appendChild(curve, nullptr);

    panel.appendChild(adsrCard, nullptr);

    // ═════════════════════════════════════════════════════════════════════════
    // Card 3: Transport Controls & Playback Speed (Right Card)
    // ═════════════════════════════════════════════════════════════════════════
    auto transportCard = flexColumn("transport-card");
    transportCard.setProperty("title", TRANS("Transport Card"), nullptr);
    transportCard.setProperty("width", 230, nullptr);
    transportCard.setProperty("flex-shrink", 0.0, nullptr);
    transportCard.setProperty("padding", "10", nullptr);
    transportCard.setProperty("border-width", "1", nullptr);

    auto transportHeader = text(TRANS("Transport Controls"), "transport-card-title");
    transportHeader.setProperty("width", "100%", nullptr);
    transportHeader.setProperty("height", 20, nullptr);
    transportHeader.setProperty("margin", "0 0 6 0", nullptr);
    transportHeader.setProperty("justification", "centred-left", nullptr);
    transportHeader.setProperty("word-wrap", "none", nullptr);
    transportCard.appendChild(transportHeader, nullptr);

    // 2x2 Large Transport Buttons (Record, Play, Stop, Back to Start).
    // The header's settings button already covers Settings — no gear here.
    auto transportGrid1 = flexRow("transport-grid-1");
    transportGrid1.setProperty("title", TRANS("Transport Grid 1"), nullptr);
    transportGrid1.setProperty("height", 42, nullptr);
    transportGrid1.setProperty("margin", "0 0 6 0", nullptr);
    transportGrid1.appendChild(makeIconBtn("RecordButton", "record-btn", TRANS("Record"), "0 6 0 0"), nullptr);
    transportGrid1.appendChild(makeIconBtn("PlayButton", "play-btn", TRANS("Play"), "0"), nullptr);
    transportCard.appendChild(transportGrid1, nullptr);

    auto transportGrid2 = flexRow("transport-grid-2");
    transportGrid2.setProperty("title", TRANS("Transport Grid 2"), nullptr);
    transportGrid2.setProperty("height", 42, nullptr);
    transportGrid2.setProperty("margin", "0 0 8 0", nullptr);
    transportGrid2.appendChild(makeIconBtn("StopButton", "stop-btn", TRANS("Stop"), "0 6 0 0"), nullptr);
    transportGrid2.appendChild(makeIconBtn("BackButton", "back-btn", TRANS("Back to Start"), "0"), nullptr);
    transportCard.appendChild(transportGrid2, nullptr);

    // Speed Slider Area — horizontal slider matching the reference design
    auto speedHeader = text(TRANS("Playback Speed"), "speed-label");
    speedHeader.setProperty("width", "100%", nullptr);
    speedHeader.setProperty("height", 18, nullptr);
    speedHeader.setProperty("margin", "0 0 4 0", nullptr);
    speedHeader.setProperty("justification", "centred-left", nullptr);
    speedHeader.setProperty("word-wrap", "none", nullptr);
    transportCard.appendChild(speedHeader, nullptr);

    auto speedSlider = node("SpeedSlider", "speed-knob");
    speedSlider.setProperty("title", TRANS("Playback Speed"), nullptr);
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
    area.setProperty("title", TRANS("Keyboard Area"), nullptr);
    area.setProperty("display", "flex", nullptr);
    area.setProperty("flex-direction", "column", nullptr);

    // KeyboardViewport (Viewport + CustomKeyboard) provided by the factory.
    auto ck = node("CustomKeyboard", "custom-keyboard");
    ck.setProperty("title", TRANS("Keyboard"), nullptr);
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
    root.setProperty("title", TRANS("Window"), nullptr);
    // 初始尺寸以 design_tokens.json 为唯一事实来源（与窗口默认尺寸一致）；
    // 后续由 MainComponent::resized() 按实际窗口尺寸覆盖。
    root.setProperty("width", devpiano::jive::DesignTokens::get().windowDefaultWidth(), nullptr);
    root.setProperty("height", devpiano::jive::DesignTokens::get().windowDefaultHeight(), nullptr);
    auto mainArea = flexColumn("main-area");
    mainArea.setProperty("title", TRANS("Main Area"), nullptr);
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
    contentRow.setProperty("title", TRANS("Content Row"), nullptr);
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

void refreshTitles(::jive::GuiItem& root) {
    // Static semantic titles are re-evaluated on every runtime language
    // switch. This table covers every node whose title is evaluated once
    // from TRANS() at build time and has no accessor text-refresh path:
    // containers, editors, labels, icon buttons (toggle/browse/transport),
    // combo boxes and knobs. Text buttons keep their titles refreshed by
    // the accessors alongside their visible text.
    struct TitleKey {
        const char* id;
        const char* key;
    };
    static constexpr TitleKey titles[] = {
        { "window", "Window" },
        { "main-area", "Main Area" },
        { "content-row", "Content Row" },
        { "header", "Header" },
        { "settings-btn", "Settings" },
        { "status-bar", "Status Bar" },
        { "midi-dot", "MIDI Activity" },
        { "plugin-name-label", "Plugin Name" },
        { "audio-info-label", "Audio Info" },
        { "time-label", "Time" },
        { "plugin-panel", "Plugin Panel" },
        { "plugin-action-row", "Plugin Actions" },
        { "toggle-btn", "Toggle Plugin Panel" },
        { "plugin-status-label", "Plugin Status" },
        { "plugin-selector", "Plugin Selector" },
        { "plugin-filter-combo", "Plugin Filter" },
        { "plugin-expanded-area", "Plugin Expanded Area" },
        { "plugin-path-row", "Plugin Path Row" },
        { "plugin-path-editor", "VST3 Path Editor" },
        { "browse-btn", "Browse" },
        { "plugin-list-editor", "Plugin List" },
        { "controls-panel", "Controls" },
        { "preset-card", "Preset Card" },
        { "preset-card-title", "Performance Preset" },
        { "preset-combo", "Performance Preset" },
        { "preset-btn-row", "Preset Actions" },
        { "file-row-1", "Export Row" },
        { "file-row-2", "File Row 2" },
        { "file-row-3", "File Row 3" },
        { "adsr-card", "ADSR Card" },
        { "adsr-curve-title", "ADSR Curve" },
        { "knobs-row", "Knobs Row" },
        { "piano-row", "Piano Row" },
        { "brightness-knob", "Brightness" },
        { "brightness-knob-wrap", "Brightness" },
        { "brightness-label", "Brightness" },
        { "hardness-knob", "Hammer" },
        { "hardness-knob-wrap", "Hammer" },
        { "hardness-label", "Hammer" },
        { "resonance-knob", "Resonance" },
        { "resonance-knob-wrap", "Resonance" },
        { "resonance-label", "Resonance" },
        { "volume-knob", "Volume" },
        { "volume-knob-wrap", "Volume" },
        { "attack-knob", "Attack" },
        { "attack-knob-wrap", "Attack" },
        { "decay-knob", "Decay" },
        { "decay-knob-wrap", "Decay" },
        { "sustain-knob", "Sustain" },
        { "sustain-knob-wrap", "Sustain" },
        { "release-knob", "Release" },
        { "release-knob-wrap", "Release" },
        { "adsr-curve", "ADSR Curve" },
        { "transport-card", "Transport Card" },
        { "transport-card-title", "Transport Controls" },
        { "transport-grid-1", "Transport Grid 1" },
        { "record-btn", "Record" },
        { "speed-label", "Playback Speed" },
        { "speed-knob", "Playback Speed" },
        { "stop-btn", "Stop" },
        { "back-btn", "Back to Start" },
        { "speed-knob", "Playback Speed" },
        { "keyboard-area", "Keyboard Area" },
        { "custom-keyboard", "Keyboard" },
    };

    for (const auto& t : titles) {
        if (auto* item = ::jive::findItemWithID(root, t.id)) {
            item->state.setProperty("title", TRANS(t.key), nullptr);
        }
    }
}

} // namespace devpiano::ui::jive
