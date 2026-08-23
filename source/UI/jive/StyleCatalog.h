#pragma once

#include <jive_core/jive_core.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace devpiano::ui::jive {

/// Global style rules loaded from style_sheets.json, applied to a JIVE
/// ValueTree BEFORE interpretation.
///
/// How styles are injected:
///   JIVE's StyleSheet reads the "style" property through
///   VariantConverter<jive::Object::Ptr>, which REJECTS plain
///   juce::DynamicObject vars (jassert + nullptr), and its parseJSON-string
///   path is broken (the Object is released before the var takes ownership).
///   So we build real ::jive::Object instances ourselves (makeJiveObject)
///   and keep them alive in ownedStyles; ValueTree properties hold the raw
///   pointers without ownership.
///
/// Rule keys: component type ("Button"), id ("#header"), and pseudo-state
/// keys ("hover", "active", "focus", "disabled", "checked") nested inside
/// either. A leading ':' on pseudo keys is tolerated (":hover").
class StyleCatalog {
public:
    StyleCatalog(const StyleCatalog&) = delete;
    StyleCatalog& operator=(const StyleCatalog&) = delete;

    /// Returns the global singleton.
    static StyleCatalog& get();

    /// Parse the style_sheets.json root object (JSON object of rules).
    void loadFromJSON(const juce::var& json);

    /// Recursively set a "style" property on every node in the tree,
    /// merging the applicable type / id rules (id wins) plus pseudo-state
    /// sub-rules. Any existing explicit "style" on a node is replaced by
    /// the merged rule (or removed when no rule matches).
    void applyToTree(juce::ValueTree& tree) const;
    /// Clear existing owned styles and re-apply newly built styles recursively.
    void refreshStyles(juce::ValueTree& tree);

    /// Release owned style objects (tests only): safe once no ValueTree
    /// references the styles anymore.
    void releaseOwnedStyles() {
        ownedStyles.clear();
    }

    /// Reset to the pristine empty state (TEST-013): clears rules and owned
    /// styles so a test file can establish an independent baseline instead of
    /// depending on which other test file touched the process-wide singleton.
    void reset() {
        rules = nullptr;
        ownedStyles.clear();
    }

private:
    StyleCatalog() = default;

    void applyToNode(juce::ValueTree& node) const;

    /// Copy a DynamicObject tree into a jive::Object tree (recursively) and
    /// keep it alive in ownedStyles. JIVE's StyleSheet only accepts real
    /// jive::Object values, and ValueTree properties do not own them.
    [[nodiscard]] ::jive::Object::ReferenceCountedPointer makeJiveObject(const juce::DynamicObject& src) const;

    /// Copy non-pseudo props from rule into target (overwriting).
    static void mergeBaseProps(juce::DynamicObject& target, const juce::DynamicObject& rule);
    /// Merge rule's pseudo-state sub-objects into target's pseudo objects.
    static void mergePseudoProps(juce::DynamicObject& target, const juce::DynamicObject& rule);
    static bool isPseudoKey(const juce::String& key);

    juce::DynamicObject::Ptr rules;

    /// Keeps every style jive::Object alive for the lifetime of the catalog
    /// (ValueTree properties hold raw DynamicObject pointers without ownership).
    mutable std::vector<::jive::Object::ReferenceCountedPointer> ownedStyles;
};

} // namespace devpiano::ui::jive
