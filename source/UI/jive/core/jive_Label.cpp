//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Label.h"

namespace jive {
static juce::BorderSize<int> toNearestInt(juce::BorderSize<float> border) {
    return juce::BorderSize<int> {
        juce::roundToInt(border.getTop()),
        juce::roundToInt(border.getLeft()),
        juce::roundToInt(border.getBottom()),
        juce::roundToInt(border.getRight()),
    };
}

Label::Label(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , border { state, "border-width" } {
    border.onValueChange = [this]() { getLabel().setBorderSize(toNearestInt(border)); };
    getLabel().setBorderSize(toNearestInt(border));
}

bool Label::isContainer() const {
    return false;
}

juce::Label& Label::getLabel() {
    auto* label = dynamic_cast<juce::Label*>(getComponent().get());
    jassert(label != nullptr);

    return *label;
}

const juce::Label& Label::getLabel() const {
    const auto* label = dynamic_cast<const juce::Label*>(getComponent().get());
    jassert(label != nullptr);

    return *label;
}
} // namespace jive
