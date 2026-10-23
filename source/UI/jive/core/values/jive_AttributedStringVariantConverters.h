//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace juce {
template <> class VariantConverter<AttributedString::ReadingDirection> {
public:
    static AttributedString::ReadingDirection fromVar(const var& v);
    static var toVar(const AttributedString::ReadingDirection& direction);

private:
    static const Array<var> options;
};

template <> class VariantConverter<AttributedString::WordWrap> {
public:
    static AttributedString::WordWrap fromVar(const var& v);
    static var toVar(const AttributedString::WordWrap& wordWrap);

private:
    static const Array<var> options;
};
} // namespace juce
