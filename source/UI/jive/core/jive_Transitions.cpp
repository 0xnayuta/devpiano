//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Transitions.h"

#include "jive_TimeParser.h"

namespace jive {
int Transitions::size() const {
    return static_cast<int>(std::size(transitions));
}

Transition* Transitions::operator[](const juce::String& propertyName) {
    if (const auto keyValuePair = transitions.find(propertyName); keyValuePair != std::end(transitions)) {
        return &keyValuePair->second;
    }

    return nullptr;
}

Transitions::ReferenceCountedPointer Transitions::fromString(const juce::String& s) {
    const auto tokens = juce::StringArray::fromTokens(s, ",", "");
    auto object = std::make_unique<Transitions>();

    for (const auto& token : tokens) {
        const auto transitionString = token.trim();

        if (const auto transition = Transition::fromString(transitionString); transition.has_value()) {
            const auto propertyName = transitionString.upToFirstOccurrenceOf(" ", false, true);
            object->transitions[propertyName] = *transition;
        }
    }

    return object.release();
}
} // namespace jive
