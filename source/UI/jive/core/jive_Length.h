//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_Property.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jive {
class Length : public Property<juce::String> {
public:
    using Property<juce::String>::Property;
    using Property<juce::String>::operator=;

    [[nodiscard]] float toPixels(const juce::Rectangle<float>& parentBounds) const;

    [[nodiscard]] bool isPixels() const;
    [[nodiscard]] bool isPercent() const;
    [[nodiscard]] bool isEm() const;
    [[nodiscard]] bool isRem() const;

    static constexpr auto pixelValueWhenAuto = 0.0f;

private:
    [[nodiscard]] double getRelativeParentLength(const juce::Rectangle<double>& parentBounds) const;
    [[nodiscard]] float getFontSize() const;
    [[nodiscard]] float getRootFontSize() const;
};
} // namespace jive
