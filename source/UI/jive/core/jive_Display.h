//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace jive {
enum class Display { flex, grid, block };
} // namespace jive

namespace juce {
template <> struct VariantConverter<jive::Display> {
    static jive::Display fromVar(const var& v);
    static var toVar(jive::Display display);

private:
    static const Array<var> options;
};
} // namespace juce
