#include "LayoutModel.h"

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

/// Helper: create a <Button> node with text content.
inline juce::ValueTree button(const juce::String& label, const juce::String& id = {}) {
    auto t = node("Button", id);
    t.setProperty("text", label, nullptr);
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
    auto row = flexRowStretch("status-bar");
    row.setProperty("height", 22, nullptr);
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

    // ── Always-visible toolbar row: status | selector | filter | buttons ──
    auto actionRow = flexRow("plugin-action-row");
    actionRow.setProperty("height", 28, nullptr);

    auto status = text({}, "plugin-status-label");
    status.setProperty("flex-grow", 1.0, nullptr);
    status.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(status, nullptr);

    auto selector = node("ComboBox", "plugin-selector");
    selector.setProperty("width", 180, nullptr);
    selector.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(selector, nullptr);

    auto filter = node("ComboBox", "plugin-filter-combo");
    filter.setProperty("width", 100, nullptr);
    filter.setProperty("margin", "0 6 0 0", nullptr);
    const auto addOption = [&filter](const juce::String& text, int index) {
        auto opt = juce::ValueTree("Option");
        opt.setProperty("text", text, nullptr);
        filter.addChild(opt, index, nullptr);
    };
    addOption("All", 0);
    addOption("Instruments Only", 1);
    addOption("Effects Only", 2);
    actionRow.appendChild(filter, nullptr);

    auto loadBtn = button("Load", "load-btn");
    loadBtn.setProperty("width", 60, nullptr);
    loadBtn.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(loadBtn, nullptr);

    auto unloadBtn = button("Unload", "unload-btn");
    unloadBtn.setProperty("width", 80, nullptr);
    unloadBtn.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(unloadBtn, nullptr);

    auto editorBtn = button("Open Editor", "editor-btn");
    editorBtn.setProperty("width", 100, nullptr);
    editorBtn.setProperty("margin", "0 6 0 0", nullptr);
    actionRow.appendChild(editorBtn, nullptr);

    auto toggleBtn = button("\u22EF", "toggle-btn");
    toggleBtn.setProperty("width", 30, nullptr);
    actionRow.appendChild(toggleBtn, nullptr);

    panel.appendChild(actionRow, nullptr);

    // ── Expandable area (height 0 when collapsed; 112 when expanded) ──
    auto expandedArea = flexColumn("plugin-expanded-area");
    expandedArea.setProperty("height", 0, nullptr);

    auto pathRow = flexRow("plugin-path-row");
    pathRow.setProperty("height", 28, nullptr);

    auto pathLabel = text("VST3 Path", "plugin-path-label");
    pathLabel.setProperty("width", 80, nullptr);
    pathRow.appendChild(pathLabel, nullptr);

    auto pathEditor = node("PathEditor", "plugin-path-editor");
    pathEditor.setProperty("flex-grow", 1.0, nullptr);
    pathRow.appendChild(pathEditor, nullptr);

    auto browseBtn = button("...", "browse-btn");
    browseBtn.setProperty("width", 40, nullptr);
    browseBtn.setProperty("margin", "0 0 0 6", nullptr);
    pathRow.appendChild(browseBtn, nullptr);

    auto scanBtn = button("Scan VST3", "scan-btn");
    scanBtn.setProperty("width", 80, nullptr);
    scanBtn.setProperty("margin", "0 0 0 6", nullptr);
    pathRow.appendChild(scanBtn, nullptr);

    expandedArea.appendChild(pathRow, nullptr);

    auto listEditor = node("ListEditor", "plugin-list-editor");
    listEditor.setProperty("flex-grow", 1.0, nullptr);
    listEditor.setProperty("margin", "8 0 0 0", nullptr);
    expandedArea.appendChild(listEditor, nullptr);

    panel.appendChild(expandedArea, nullptr);

    return panel;
}

} // namespace devpiano::ui::jive
