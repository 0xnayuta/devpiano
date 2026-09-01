//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_ComponentFactory.h"
#include "jive_GuiItemDecorator.h"

namespace juce {
class AudioProcessor;
}

namespace jive {
class Interpreter : private juce::ValueTree::Listener {
public:
    Interpreter() = default;

    const ComponentFactory& getComponentFactory() const;
    ComponentFactory& getComponentFactory();
    void setComponentFactory(const ComponentFactory& newFactory);

    void setAlias(juce::Identifier aliasType, const juce::ValueTree& treeToReplaceWith);

    template <typename Decorator> void addDecorator(const juce::Identifier& itemType);

    [[nodiscard]] std::unique_ptr<GuiItem> interpret(const juce::ValueTree& tree,
                                                     juce::AudioProcessor* pluginProcessor = nullptr) const;

    void listenTo(GuiItem& item);

private:
    void valueTreeChildAdded(juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded) final;

    std::unique_ptr<GuiItem> interpret(const juce::ValueTree& tree, GuiItem* const parent,
                                       juce::AudioProcessor* pluginProcessor) const;

    void expandAlias(juce::ValueTree& tree) const;

    std::unique_ptr<GuiItem> createUndecoratedItem(const juce::ValueTree& tree, GuiItem* const parent) const;
    void insertChild(GuiItem& item, int index, const juce::ValueTree& childState) const;
    void setChildItems(GuiItem& item) const;

    std::unique_ptr<juce::Component> createComponent(const juce::ValueTree& tree, const GuiItem* parent) const;
    void setupItemsRecursive(GuiItem& item) const;

    ComponentFactory componentFactory;
    std::vector<std::pair<juce::Identifier, std::function<std::unique_ptr<GuiItemDecorator>(std::unique_ptr<GuiItem>)>>>
        customDecorators;
    std::unordered_map<juce::Identifier, juce::ValueTree> aliases;

    juce::WeakReference<GuiItem> observedItem = nullptr;

    JUCE_LEAK_DETECTOR(Interpreter)
};
} // namespace jive
