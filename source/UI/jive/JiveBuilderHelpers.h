#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace devpiano::ui::jive {

/// Helper: create a ValueTree node with a specified component/layout type name and optional id.
[[nodiscard]] inline juce::ValueTree node(const juce::Identifier& type, const juce::String& id = {}) {
    auto t = juce::ValueTree(type);
    if (id.isNotEmpty()) {
        t.setProperty("id", id, nullptr);
    }
    return t;
}

/// Helper: create a <Text> node.
/// Height is intentionally not forced so flex stretch or explicit parent rules apply.
[[nodiscard]] inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    t.setProperty("title", content, nullptr);
    return t;
}

/// Helper: create a <Button> node with centred text label child.
/// JIVE's Button widget maps title for accessibility; visible label is a centred Text child.
[[nodiscard]] inline juce::ValueTree button(const juce::String& label, const juce::String& id = {}) {
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

/// Helper: create a <Component> flex row with centre alignment.
[[nodiscard]] inline juce::ValueTree flexRow(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "row", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    return t;
}

/// Helper: create a <Component> flex row with stretch alignment.
[[nodiscard]] inline juce::ValueTree flexRowStretch(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "row", nullptr);
    t.setProperty("align-items", "stretch", nullptr);
    return t;
}

/// Helper: create a <Component> flex column with stretch alignment.
[[nodiscard]] inline juce::ValueTree flexColumn(const juce::String& id = {}) {
    auto t = node("Component", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("flex-direction", "column", nullptr);
    t.setProperty("align-items", "stretch", nullptr);
    return t;
}

/// Helper: create a 2-column settings row (Label + Control).
[[nodiscard]] inline juce::ValueTree settingRow(const juce::String& labelStr, const juce::ValueTree& controlNode,
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

} // namespace devpiano::ui::jive
