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
    ~GridItem() override;

    [[nodiscard]] juce::GridItem toJuceGridItem(juce::Rectangle<float> parentContentBounds, LayoutStrategy strategy);

private:
    void boxModelChanged(BoxModel&) final;
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    PassiveProperty<int> order;
    PassiveProperty<juce::GridItem::JustifySelf> justifySelf;
    PassiveProperty<juce::GridItem::AlignSelf> alignSelf;
    PassiveProperty<juce::GridItem::StartAndEndProperty> gridColumn;
    PassiveProperty<juce::GridItem::StartAndEndProperty> gridRow;
    PassiveProperty<juce::String> gridArea;

    const BoxModel& box { boxModel(*this) };
    std::unordered_map<ItemCacheKey, juce::GridItem> cachedItems;
};
} // namespace jive
