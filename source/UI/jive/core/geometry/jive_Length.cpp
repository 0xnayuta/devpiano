//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/geometry/jive_Length.h"

namespace jive {
bool Length::operator==(const Length& other) const {
    return juce::approximatelyEqual(value, other.value) && unit == other.unit;
}

juce::String Length::toString() const {
    if (unit == Unit::percentage) {
        return juce::String { value } + "%";
    }

    return juce::String { value } + "px";
}

Length Length::fromString(const juce::String& lengthString) {
    const auto text = lengthString.trim();

    return Length {
        text.getFloatValue(),
        text.endsWith("%") ? Unit::percentage : Unit::absolute,
    };
}
} // namespace jive

namespace juce {
var VariantConverter<jive::Length>::toVar(const jive::Length& length) {
    return length.toString();
}

jive::Length VariantConverter<jive::Length>::fromVar(const var& value) {
    return jive::Length::fromString(value.toString());
}
} // namespace juce
