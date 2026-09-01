//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_ContainerItem.h"

namespace jive {
class BlockContainer : public ContainerItem {
public:
    explicit BlockContainer(std::unique_ptr<GuiItem> itemToDecorate);

    void layOutChildren() override;

protected:
    juce::Rectangle<float> calculateIdealSize(juce::Rectangle<float> constraints) const override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockContainer)
};
} // namespace jive
