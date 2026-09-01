//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace jive {
[[nodiscard]] inline auto cubicBezier(juce::Point<float> startControlPoint, juce::Point<float> endControlPoint) {
    juce::Path path;

    path.startNewSubPath(0.0f, 0.0f);
    path.cubicTo(startControlPoint, endControlPoint, juce::Point { 1.0f, 1.0f });

    return path;
}
} // namespace jive
