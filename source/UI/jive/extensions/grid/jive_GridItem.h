//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_ContainerItem.h"

namespace jive {
class GridItem : public ContainerItem::Child, private BoxModel::Listener {
public:
    explicit GridItem(std::unique_ptr<GuiItem> itemToDecorate);
    ~GridItem();

    [[nodiscard]] juce::GridItem toJuceGridItem(juce::Rectangle<float> parentContentBounds, LayoutStrategy strategy);

private:
    void boxModelChanged(BoxModel&) final;

    Property<int> order;
    Property<juce::GridItem::JustifySelf> justifySelf;
    Property<juce::GridItem::AlignSelf> alignSelf;
    Property<juce::GridItem::StartAndEndProperty> gridColumn;
    Property<juce::GridItem::StartAndEndProperty> gridRow;
    Property<juce::String> gridArea;

    const BoxModel& box { boxModel(*this) };
    std::unordered_map<ItemCacheKey, juce::GridItem> cachedItems;
};
} // namespace jive
