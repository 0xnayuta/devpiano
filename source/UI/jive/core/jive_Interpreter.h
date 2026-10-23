//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#ifndef JIVE_ENABLE_GRID
#define JIVE_ENABLE_GRID 1
#endif

#include "jive_ComponentFactory.h"
#include "jive_GuiItemDecorator.h"

namespace juce {
class AudioProcessor;
}

namespace jive {
class Interpreter {
public:
    Interpreter() = default;

    const ComponentFactory& getComponentFactory() const;
    ComponentFactory& getComponentFactory();
    void setComponentFactory(const ComponentFactory& newFactory);

    [[nodiscard]] std::unique_ptr<GuiItem> interpret(const juce::ValueTree& tree,
                                                     juce::AudioProcessor* pluginProcessor = nullptr) const;

private:
    std::unique_ptr<GuiItem> interpret(const juce::ValueTree& tree, GuiItem* parent,
                                       juce::AudioProcessor* pluginProcessor) const;

    std::unique_ptr<GuiItem> createUndecoratedItem(const juce::ValueTree& tree, GuiItem* parent) const;
    void setChildItems(GuiItem& item) const;

    std::unique_ptr<juce::Component> createComponent(const juce::ValueTree& tree) const;

    ComponentFactory componentFactory;

    JUCE_LEAK_DETECTOR(Interpreter)
};
} // namespace jive
