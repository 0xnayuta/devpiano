//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Overflow.h"

namespace juce {
const std::unordered_map<jive::Overflow, String> VariantConverter<jive::Overflow>::stringsMap = {
    { jive::Overflow::hidden, "hidden" },
    { jive::Overflow::scroll, "scroll" },
};
} // namespace juce
