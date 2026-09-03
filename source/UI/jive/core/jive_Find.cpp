//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Find.h"

#include "jive_VariantConvertion.h"

namespace jive {
juce::ValueTree find(const juce::ValueTree& root, std::function<bool(const juce::ValueTree&)> predicate) {
    if (predicate == nullptr) {
        return {};
    }

    if (predicate(root)) {
        return root;
    }

    for (auto child : root) {
        if (predicate(child)) {
            return child;
        }
    }

    for (auto child : root) {
        if (auto result = find(child, predicate); result != juce::ValueTree {}) {
            return result;
        }
    }

    return {};
}

juce::ValueTree findElementWithID(const juce::ValueTree& root, const juce::Identifier& id) {
    return find(root, [id](const auto& element) { return fromVar<juce::Identifier>(element["id"]) == id; });
}
} // namespace jive
