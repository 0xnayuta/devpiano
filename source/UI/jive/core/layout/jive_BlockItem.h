//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "UI/jive/core/layout/jive_ContainerItem.h"

namespace jive {
class BlockItem : public ContainerItem::Child {
public:
    explicit BlockItem(std::unique_ptr<GuiItem> itemToDecorate);
    ~BlockItem() override;

    juce::Rectangle<int> calculateBounds() const;

private:
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    int calculateX() const;
    int calculateY() const;

    PassiveProperty<Length> x;
    PassiveProperty<Length> y;
    PassiveProperty<Length> centreX;
    PassiveProperty<Length> centreY;
    PassiveProperty<Length> width;
    PassiveProperty<Length> height;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockItem)
};
} // namespace jive
