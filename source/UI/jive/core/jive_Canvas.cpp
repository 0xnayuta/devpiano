//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Canvas.h"

namespace jive {
Canvas::Canvas(std::function<void(juce::Graphics&)> paintFunction)
    : onPaint { paintFunction } {
}

void Canvas::paint(juce::Graphics& g) {
    if (onPaint != nullptr) {
        onPaint(g);
    }
}
} // namespace jive
