//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

namespace jive {
struct ReferenceCountedValueTreeWrapper : juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<ReferenceCountedValueTreeWrapper>;

    explicit ReferenceCountedValueTreeWrapper(const juce::ValueTree& tree)
        : state { tree } {
    }

    juce::ValueTree state;
};
} // namespace jive
