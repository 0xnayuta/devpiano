//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Orientation.h"

namespace juce {
const Array<var> VariantConverter<jive::Orientation>::options = {
    "horizontal",
    "vertical",
};

jive::Orientation VariantConverter<jive::Orientation>::fromVar(const var& v) {
    jassert(options.contains(v));
    return static_cast<jive::Orientation>(options.indexOf(v));
}

var VariantConverter<jive::Orientation>::toVar(const jive::Orientation& orientation) {
    const auto index = static_cast<int>(orientation);

    jassert(options.size() >= index);
    return options[index];
}
} // namespace juce
