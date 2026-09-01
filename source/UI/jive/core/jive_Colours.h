//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace jive {
[[nodiscard]] juce::Colour parseColour(const juce::String& colourString);
} // namespace jive
