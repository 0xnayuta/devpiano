//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_IgnoredComponent.h"

namespace jive {
IgnoredComponent::IgnoredComponent() {
    setInterceptsMouseClicks(false, true);
}

std::unique_ptr<juce::AccessibilityHandler> IgnoredComponent::createAccessibilityHandler() {
    return createIgnoredAccessibilityHandler(*this);
}
} // namespace jive
