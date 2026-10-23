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

std::unique_ptr<GuiItem> Interpreter::interpret(const juce::ValueTree& tree,
                                                juce::AudioProcessor* pluginProcessor) const {
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED;
    return interpret(tree, nullptr, pluginProcessor);
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

static std::unique_ptr<GuiItem> decorate(std::unique_ptr<GuiItem> item,
                                         [[maybe_unused]] juce::AudioProcessor* pluginProcessor) {
    item = std::make_unique<CommonGuiItem>(std::move(item));
    item = decorateWithHereditaryBehaviour(std::move(item));
    item = decorateWithWidgetBehaviour(std::move(item));

    if (!item->isContent()) {
        item = decorateWithDisplayBehaviour(std::move(item));
    }

#if JIVE_IS_PLUGIN_PROJECT
    if (item->state.getType().toString() == "Editor") {
        item = std::make_unique<PluginEditor>(std::move(item), pluginProcessor);
    }
#endif

    return item;
}

std::unique_ptr<GuiItem> Interpreter::interpret(const juce::ValueTree& tree, GuiItem* const parent,
                                                juce::AudioProcessor* pluginProcessor) const {
    auto item = createUndecoratedItem(tree, parent);

    if (item != nullptr) {
        item = decorate(std::move(item), pluginProcessor);
        setChildItems(*item);
    }

    return item;
}

std::unique_ptr<GuiItem> Interpreter::createUndecoratedItem(const juce::ValueTree& tree, GuiItem* const parent) const {
    if (auto component = createComponent(tree)) {
        return std::make_unique<GuiItem>(std::move(component), tree,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
                                         StyleSheet::create(*component, tree),
#endif
                                         parent);
    }

    return nullptr;
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

std::unique_ptr<juce::Component> Interpreter::createComponent(const juce::ValueTree& tree) const {
    return componentFactory.create(tree.getType());
}
} // namespace jive
