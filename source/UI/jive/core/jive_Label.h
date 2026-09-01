//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_GuiItemDecorator.h"

namespace jive {
class Label : public GuiItemDecorator {
public:
    explicit Label(std::unique_ptr<GuiItem> itemToDecorate);

    bool isContainer() const override;

    juce::Label& getLabel();
    const juce::Label& getLabel() const;

private:
    Property<juce::BorderSize<float>> border;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Label)
};
} // namespace jive
