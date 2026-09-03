#include "StyleCatalog.h"

#include "UI/jive/DesignTokens.h"

namespace devpiano::ui::jive {

namespace {

const juce::StringArray kPseudoKeys { "hover", "active", "focus", "disabled", "checked" };

[[nodiscard]] juce::String normalizePseudoKey(const juce::String& key) {
    auto k = key.startsWith(":") ? key.substring(1) : key;
    if (k == "pressed") {
        k = "active";
    }
    return k;
}

// Style values starting with '@' reference DesignTokens (DOC-007) so
// style_sheets.json and design_tokens.json share one source of truth for
// colours / typography.  Resolution goes through DesignTokens getters, so
// it is safe whether or not design_tokens.json was loaded first.
void resolveTokenValues(juce::DynamicObject& obj) {
    for (const auto& prop : obj.getProperties()) {
        if (auto* child = prop.value.getDynamicObject()) {
            resolveTokenValues(*child);
        } else if (prop.value.isString()) {
            const auto text = prop.value.toString();
            if (text.startsWith("@") && text.length() > 1) {
                const auto resolved = devpiano::jive::DesignTokens::get().resolveToken(text.substring(1));
                if (resolved.isNotEmpty()) {
                    obj.setProperty(prop.name, resolved);
                }
            }
        }
    }
}

} // namespace

StyleCatalog& StyleCatalog::get() {
    static StyleCatalog instance;
    return instance;
}

void StyleCatalog::loadFromJSON(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        resolveTokenValues(*obj);
        rules = obj;
        cachedStyles.clear();
    }
}

bool StyleCatalog::isPseudoKey(const juce::String& key) {
    return kPseudoKeys.contains(normalizePseudoKey(key));
}

void StyleCatalog::mergeBaseProps(juce::DynamicObject& target, const juce::DynamicObject& rule) {
    const auto& props = rule.getProperties();
    for (const auto& prop : props) {
        const auto key = prop.name.toString();
        if (!isPseudoKey(key)) {
            target.setProperty(prop.name, prop.value);
        }
    }
}

void StyleCatalog::mergePseudoProps(juce::DynamicObject& target, const juce::DynamicObject& rule) {
    const auto& props = rule.getProperties();
    for (const auto& prop : props) {
        const auto key = prop.name.toString();
        if (!isPseudoKey(key)) {
            continue;
        }

        auto* pseudoRule = prop.value.getDynamicObject();
        if (pseudoRule == nullptr) {
            continue;
        }

        auto pseudoKey = normalizePseudoKey(key);
        auto* pseudoTarget = target.getProperty(pseudoKey).getDynamicObject();
        if (pseudoTarget == nullptr) {
            pseudoTarget = new juce::DynamicObject();
            target.setProperty(pseudoKey, juce::var(pseudoTarget));
        }
        for (const auto& pit : pseudoRule->getProperties()) {
            pseudoTarget->setProperty(pit.name, pit.value);
        }
    }
}

::jive::Object::ReferenceCountedPointer StyleCatalog::makeJiveObject(const juce::DynamicObject& src) const {
    // ValueTree properties do not own DynamicObjects, and JIVE's
    // parseJSON-String conversion path is broken (the Object is released
    // before the var takes ownership) — so we build jive::Object instances
    // ourselves and keep them alive in ownedStyles.
    auto result = ::jive::Object::ReferenceCountedPointer(new ::jive::Object());
    for (const auto& it : src.getProperties()) {
        if (auto* child = it.value.getDynamicObject()) {
            result->setProperty(it.name, juce::var(makeJiveObject(*child).get()));
        } else {
            result->setProperty(it.name, it.value);
        }
    }
    ownedStyles.push_back(result);
    return result;
}

void StyleCatalog::applyToNode(juce::ValueTree& node) const {
    if (rules == nullptr) {
        return;
    }

    const auto nodeId = node["id"].toString();
    const auto nodeType = node.getType().toString();
    const auto styleKey = nodeType + "#" + nodeId;

    // Reuse already generated jive::Object to prevent unbounded memory growth (RES-002)
    if (auto it = cachedStyles.find(styleKey); it != cachedStyles.end()) {
        if (it->second != nullptr) {
            node.setProperty("style", juce::var(it->second.get()), nullptr);
        } else if (node.hasProperty("style")) {
            node.removeProperty("style", nullptr);
        }
        return;
    }

    // Build a merged style object: base props + pseudo-state sub-objects.
    juce::DynamicObject merged;

    const auto applyRule = [&merged](const juce::DynamicObject& rule) {
        mergeBaseProps(merged, rule);
        mergePseudoProps(merged, rule);
    };

    // Type rule first (lowest priority), then id rule (overrides).
    if (auto* typeRule = rules->getProperty(nodeType).getDynamicObject()) {
        applyRule(*typeRule);
    }
    if (nodeId.isNotEmpty()) {
        if (auto* idRule = rules->getProperty("#" + nodeId).getDynamicObject()) {
            applyRule(*idRule);
        }
    }

    if (merged.getProperties().size() == 0) {
        cachedStyles[styleKey] = nullptr;
        if (node.hasProperty("style")) {
            node.removeProperty("style", nullptr);
        }
        return;
    }

    auto jiveObj = makeJiveObject(merged);
    cachedStyles[styleKey] = jiveObj;
    node.setProperty("style", juce::var(jiveObj.get()), nullptr);
}

void StyleCatalog::refreshStyles(juce::ValueTree& tree) {
    ownedStyles.clear();
    cachedStyles.clear();
    applyToTree(tree);
}
void StyleCatalog::applyToTree(juce::ValueTree& tree) const {
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED;
    applyToNode(tree);
    for (auto child : tree) {
        applyToTree(child);
    }
}

} // namespace devpiano::ui::jive
