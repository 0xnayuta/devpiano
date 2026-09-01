//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_ContainerItem.h"

namespace jive {
class BlockItem : public ContainerItem::Child {
public:
    explicit BlockItem(std::unique_ptr<GuiItem> itemToDecorate);

    juce::Rectangle<int> calculateBounds() const;

private:
    int calculateX() const;
    int calculateY() const;

    Length x;
    Length y;
    Length centreX;
    Length centreY;
    Length width;
    Length height;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockItem)
};
} // namespace jive
