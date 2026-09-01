//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jive {
class IgnoredComponent : public juce::Component {
public:
    IgnoredComponent();
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
};
} // namespace jive
