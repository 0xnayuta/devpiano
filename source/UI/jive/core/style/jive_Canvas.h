//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jive {
class Canvas : public juce::Component {
public:
    Canvas() = default;
    explicit Canvas(std::function<void(juce::Graphics&)> paintFunction);

    void paint(juce::Graphics& g) final;

    std::function<void(juce::Graphics&)> onPaint;
};
} // namespace jive
