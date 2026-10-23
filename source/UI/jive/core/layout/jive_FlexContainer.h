//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "UI/jive/core/layout/jive_ContainerItem.h"

namespace jive {
class FlexContainer : public ContainerItem {
public:
    explicit FlexContainer(std::unique_ptr<GuiItem> itemToDecorate);
    ~FlexContainer() override;

    void layOutChildren() override;

    operator juce::FlexBox();

protected:
    juce::Rectangle<float> calculateIdealSize(juce::Rectangle<float> constraints) const override;

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    juce::FlexBox buildFlexBox(juce::Rectangle<float> bounds, LayoutStrategy strategy);
    [[nodiscard]] juce::Point<float> calculateGaps() const;

    PassiveProperty<juce::FlexBox::Direction> flexDirection;
    PassiveProperty<juce::FlexBox::Wrap> flexWrap;
    PassiveProperty<juce::FlexBox::JustifyContent> flexJustifyContent;
    PassiveProperty<juce::FlexBox::AlignItems> flexAlignItems;
    PassiveProperty<juce::FlexBox::AlignContent> flexAlignContent;
    PassiveProperty<juce::Array<juce::Grid::Px>> gap;

    bool layoutRecursionLock = false;
    bool changesDuringLayout = false;

    JUCE_LEAK_DETECTOR(FlexContainer)
};
} // namespace jive
