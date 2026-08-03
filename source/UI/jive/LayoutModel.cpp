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

/// Helper: create a <Text> node. Explicit height is required because
/// TextComponent reports no intrinsic size to JUCE's FlexBox.
inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    t.setProperty("height", 14, nullptr);
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

} // namespace devpiano::ui::jive
