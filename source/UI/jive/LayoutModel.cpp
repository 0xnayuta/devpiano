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

} // namespace devpiano::ui::jive
