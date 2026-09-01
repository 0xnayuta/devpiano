//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_ContainerItem.h"

namespace jive {
class FlexItem : public ContainerItem::Child, private BoxModel::Listener {
public:
    explicit FlexItem(std::unique_ptr<GuiItem> itemToDecorate);
    ~FlexItem();

    [[nodiscard]] juce::FlexItem toJuceFlexItem(juce::Rectangle<float> parentContentBounds, LayoutStrategy strategy);

private:
    void boxModelChanged(BoxModel&) final;

    Property<int> order;
    Property<float> flexGrow;
    Property<float> flexShrink;
    Property<float> flexBasis;
    Property<juce::FlexItem::AlignSelf> alignSelf;

    const BoxModel& box { boxModel(*this) };
    const std::unique_ptr<juce::Component> layoutDummy;
    std::unordered_map<ItemCacheKey, juce::FlexItem> cachedItems;
};
} // namespace jive
