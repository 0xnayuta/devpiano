//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Interpreter.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include "jive_BlockContainer.h"
#include "jive_BlockItem.h"
#include "jive_FlexContainer.h"
#include "jive_FlexItem.h"
#include "jive_Text.h"
#if JIVE_ENABLE_GRID
#include "jive_GridContainer.h"
#endif
#if JIVE_ENABLE_GRID
#include "jive_GridItem.h"
#endif
#include "jive_Button.h"
#include "jive_ComboBox.h"
#include "jive_CommonGuiItem.h"
#include "jive_Display.h"
#include "jive_Label.h"
#include "jive_ProgressBar.h"
#include "jive_Slider.h"

namespace jive {
const ComponentFactory& Interpreter::getComponentFactory() const {
    return componentFactory;
}

ComponentFactory& Interpreter::getComponentFactory() {
    return componentFactory;
}

void Interpreter::setComponentFactory(const ComponentFactory& newFactory) {
    componentFactory = newFactory;
}

void Interpreter::setAlias(juce::Identifier aliasType, const juce::ValueTree& treeToReplaceWith) {
    aliases.emplace(aliasType, treeToReplaceWith.createCopy());
}

template <typename Decorator> void Interpreter::addDecorator(const juce::Identifier& itemType) {
    customDecorators.emplace_back(
        itemType, [](std::unique_ptr<GuiItem> item) { return std::make_unique<Decorator>(std::move(item)); });
}

std::unique_ptr<GuiItem> Interpreter::interpret(const juce::ValueTree& tree,
                                                juce::AudioProcessor* pluginProcessor) const {
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED;
    return interpret(tree, nullptr, pluginProcessor);
}

void Interpreter::listenTo(GuiItem& item) {
    if (observedItem != nullptr) {
        observedItem->state.removeListener(this);
    }

    observedItem = &item;
    observedItem->state.addListener(this);
}

[[nodiscard]] static GuiItem* findItem(GuiItem& root, const juce::ValueTree& state) {
    if (root.state == state) {
        return &root;
    }

    for (auto* const child : root.getChildren()) {
        if (auto item = findItem(*child, state)) {
            return item;
        }
    }

    return nullptr;
}

void Interpreter::valueTreeChildAdded(juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded) {
    if (observedItem != nullptr) {
        if (auto* parentItem = findItem(*observedItem, parentTree)) {
            const auto index = parentTree.indexOf(childWhichHasBeenAdded);
            insertChild(*parentItem, index, childWhichHasBeenAdded);
        }
    }
}

static std::unique_ptr<GuiItem> decorateWithDisplayBehaviour(std::unique_ptr<GuiItem> item) {
    Property<Display> display { item->state, "display" };

    switch (display.get()) {
    case Display::flex:
        return std::make_unique<FlexContainer>(std::move(item));
    case Display::grid:
#if JIVE_ENABLE_GRID
        return std::make_unique<GridContainer>(std::move(item));
#else
        jassertfalse;
        return nullptr;
#endif
    case Display::block:
        return std::make_unique<BlockContainer>(std::move(item));
    }

    // Unhandled display type!
    jassertfalse;
    return nullptr;
}

static std::unique_ptr<GuiItem> decorateWithHereditaryBehaviour(std::unique_ptr<GuiItem> item) {
    if (item->getParent() == nullptr) {
        return item;
    }

    Property<Display> display { item->state.getParent(), "display" };

    switch (display.get()) {
    case Display::flex:
        return std::make_unique<FlexItem>(std::move(item));
    case Display::grid:
#if JIVE_ENABLE_GRID
        return std::make_unique<GridItem>(std::move(item));
#else
        jassertfalse;
        return nullptr;
#endif
    case Display::block:
        return std::make_unique<BlockItem>(std::move(item));
    }

    // Unhandled display type!
    jassertfalse;
    return nullptr;
}

static std::unique_ptr<GuiItem> decorateWithWidgetBehaviour(std::unique_ptr<GuiItem> item) {
    const auto name = item->state.getType().toString();

    if (name == "Button" || name == "Checkbox") {
        return std::make_unique<Button>(std::move(item));
    }
    if (name == "ComboBox") {
        return std::make_unique<ComboBox>(std::move(item));
    }
    if (name == "Label") {
        return std::make_unique<Label>(std::move(item));
    }
    if (name == "ProgressBar") {
        return std::make_unique<ProgressBar>(std::move(item));
    }
    if (name == "Slider") {
        return std::make_unique<Slider>(std::move(item));
    }
    if (name == "Text") {
        return std::make_unique<Text>(std::move(item));
    }

    return item;
}

using DecoratorCreator = std::function<std::unique_ptr<GuiItemDecorator>(std::unique_ptr<GuiItem>)>;

static std::vector<const DecoratorCreator*>
collectDecoratorCreators(const juce::Identifier& itemType,
                         const std::vector<std::pair<juce::Identifier, DecoratorCreator>>& decorators) {
    std::vector<const DecoratorCreator*> creators;

    for (const auto& decorator : decorators) {
        if (decorator.first == itemType) {
            creators.push_back(&decorator.second);
        }
    }

    return creators;
}

static std::unique_ptr<GuiItem>
decorate(std::unique_ptr<GuiItem> item,
         const std::vector<std::pair<juce::Identifier, DecoratorCreator>>& customDecorators,
         [[maybe_unused]] juce::AudioProcessor* pluginProcessor) {
    item = std::make_unique<CommonGuiItem>(std::move(item));
    item = decorateWithHereditaryBehaviour(std::move(item));
    item = decorateWithWidgetBehaviour(std::move(item));

    if (!item->isContent()) {
        item = decorateWithDisplayBehaviour(std::move(item));
    }

    for (const auto* decorateWithCustomDecorations :
         collectDecoratorCreators(item->state.getType(), customDecorators)) {
        item = (*decorateWithCustomDecorations)(std::move(item));
    }

#if JIVE_IS_PLUGIN_PROJECT
    if (item->state.getType().toString() == "Editor") {
        item = std::make_unique<PluginEditor>(std::move(item), pluginProcessor);
    }
#endif

    return item;
}

void Interpreter::setupItemsRecursive(GuiItem& item) const {
    for (auto* child : item.getChildren()) {
        setupItemsRecursive(*child);
    }

    if (auto view = item.getView(); view != nullptr) {
        view->setup(item);
    }
}

std::unique_ptr<GuiItem> Interpreter::interpret(const juce::ValueTree& tree, GuiItem* const parent,
                                                juce::AudioProcessor* pluginProcessor) const {
    auto item = createUndecoratedItem(tree, parent);

    if (item != nullptr) {
        item = decorate(std::move(item), customDecorators, pluginProcessor);
        setChildItems(*item);

        if (item->isTopLevel()) {
            setupItemsRecursive(*item);
        }
    }

    return item;
}

void Interpreter::expandAlias(juce::ValueTree& tree) const {
    if (const auto alias = aliases.find(tree.getType()); alias != std::end(aliases)) {
        auto replacement = alias->second.createCopy();

        for (auto i = 0; i < tree.getNumProperties(); i++) {
            auto propertyName = tree.getPropertyName(i);
            replacement.setProperty(propertyName, tree[propertyName], nullptr);
        }

        for (auto child : tree) {
            replacement.addChild(child.createCopy(), tree.indexOf(child), nullptr);
        }

        auto parent = tree.getParent();
        const auto indexInParent = parent.indexOf(tree);

        parent.removeChild(tree, nullptr);
        parent.addChild(replacement, indexInParent, nullptr);
        tree = replacement;
    }
}

std::unique_ptr<GuiItem> Interpreter::createUndecoratedItem(const juce::ValueTree& tree, GuiItem* const parent) const {
    // jassert(tree.getType().toString() != "svg");
    auto expandedTree = tree;
    expandAlias(expandedTree);

    if (auto component = createComponent(expandedTree, parent)) {
        return std::make_unique<GuiItem>(std::move(component), expandedTree,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
                                         StyleSheet::create(*component, expandedTree),
#endif
                                         parent);
    }

    return nullptr;
}

void Interpreter::insertChild(GuiItem& item, int index, const juce::ValueTree& childState) const {
    auto childItem = interpret(childState, &item, nullptr);

    if (childItem != nullptr) {
        setupItemsRecursive(*childItem);

        if (item.isContainer() || childItem->isContent()) {
            item.insertChild(std::move(childItem), index);
        }
    }
}

void Interpreter::setChildItems(GuiItem& item) const {
    std::vector<std::unique_ptr<GuiItem>> children;

    for (auto i = 0; i < item.state.getNumChildren(); i++) {
        if (auto child = interpret(item.state.getChild(i), &item, nullptr); child != nullptr) {
            if (item.isContainer() || child->isContent()) {
                children.push_back(std::move(child));
            }
        }
    }

    item.setChildren(std::move(children));
}

std::unique_ptr<juce::Component> Interpreter::createComponent(const juce::ValueTree& tree,
                                                              const GuiItem* parent) const {
    if (auto* viewObject = dynamic_cast<jive::View*>(tree["view-object"].getObject())) {
        if (auto component = viewObject->createComponent(tree)) {
            return component;
        }
    }

    for (auto* ancestor = parent; ancestor != nullptr; ancestor = ancestor->getParent()) {
        if (auto component = ancestor->getView()->createComponent(tree)) {
            return component;
        }
    }

    return componentFactory.create(tree.getType());
}
} // namespace jive
