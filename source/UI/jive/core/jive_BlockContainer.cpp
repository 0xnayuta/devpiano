//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_BlockContainer.h"

#include "jive_BlockItem.h"

#include "jive_Display.h"

namespace jive {
BlockContainer::BlockContainer(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem { std::move(itemToDecorate) } {
    jassert(state.hasProperty("display"));
    jassert(state["display"] == juce::VariantConverter<Display>::toVar(Display::block));
}

void BlockContainer::layOutChildren() {
    GuiItemDecorator::layOutChildren();

    for (auto child : getChildren()) {
        auto& blockItem = *dynamic_cast<GuiItemDecorator&>(*child).toType<BlockItem>();
        child->getComponent()->setBounds(blockItem.calculateBounds());
    }
}

juce::Rectangle<float> BlockContainer::calculateIdealSize(juce::Rectangle<float>) const {
    return { 0.0f, 0.0f };
}
} // namespace jive
