//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/layout/jive_GuiItem.h"

#include "UI/jive/core/layout/jive_CommonGuiItem.h"
#include "UI/jive/core/layout/jive_GuiItemDecorator.h"

namespace jive {
GuiItem::GuiItem(std::shared_ptr<juce::Component> comp, GuiItem* parentItem,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
                 StyleSheet::ReferenceCountedPointer sheet,
#endif
                 const juce::ValueTree& sourceState)
    : state { sourceState }
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
    , styleSheet { std::move(sheet) }
#endif
    , component { std::move(comp) }
    , parent { parentItem } {
    jassert(component != nullptr);
}

GuiItem::GuiItem(std::unique_ptr<juce::Component> comp, const juce::ValueTree& sourceState,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
                 StyleSheet::ReferenceCountedPointer sheet,
#endif
                 GuiItem* parentItem)
    : GuiItem {
        std::shared_ptr<juce::Component> { std::move(comp) },
        parentItem,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
        std::move(sheet),
#endif
        sourceState,
    } {
}

GuiItem::GuiItem(const GuiItem& other)
    : GuiItem {
        other.component,
        other.parent,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
        nullptr,
#endif
        other.state,
    } {
}

GuiItem::~GuiItem() {
    masterReference.clear();
}

std::shared_ptr<const juce::Component> GuiItem::getComponent() const {
    return component;
}

std::shared_ptr<juce::Component> GuiItem::getComponent() {
    return component;
}

void GuiItem::insertChild(std::unique_ptr<GuiItem> child, int index) {
    insertChild(std::move(child), index, true);
}

void GuiItem::insertChild(std::unique_ptr<GuiItem> child, int index, bool invokeCallback) {
    if (child == nullptr) {
        // Trying to add a nonexistant child!
        jassertfalse;
        return;
    }

    auto* newlyAddedChild = children.insert(index, std::move(child));
    component->addChildComponent(*newlyAddedChild->getComponent());

    if (invokeCallback) {
        childrenChanged();
    }
}

void GuiItem::setChildren(std::vector<std::unique_ptr<GuiItem>>&& newChildren) {
    children.clearQuick(true);

    for (auto& child : newChildren) {
        insertChild(std::move(child), children.size(), false);
    }

    childrenChanged();
}

void GuiItem::removeChild(GuiItem& childToRemove) {
    children.removeObject(&childToRemove);
}

juce::Array<GuiItem*> GuiItem::getChildren() {
    return { children.getRawDataPointer(), children.size() };
}

juce::Array<const GuiItem*> GuiItem::getChildren() const {
    return { const_cast<GuiItem*>(this)->children.getRawDataPointer(), children.size() };
}

const GuiItem* GuiItem::getParent() const {
    return parent;
}

GuiItem* GuiItem::getParent() {
    return parent;
}

bool GuiItem::isTopLevel() const {
    return getParent() == nullptr;
}

bool GuiItem::isContainer() const {
    return true;
}

bool GuiItem::isContent() const {
    return false;
}

void GuiItem::callLayoutChildrenWithRecursionLock() {
    if (isLayingOutChildren() || std::size(getChildren()) == 0) {
        return;
    }

    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };
    const juce::ScopedValueSetter svs { layoutRecursionLock, true };
    layOutChildren();
}

bool GuiItem::isLayingOutChildren() const {
    return layoutRecursionLock;
}

#if JIVE_IS_PLUGIN_PROJECT
void GuiItem::attachToParameter(juce::RangedAudioParameter*, juce::UndoManager*) {
    // It doesn't make sense to call this on an item that isn't a widget!
    // Did you mean to use jive::findItemWithID() to attach to a specific item?
    jassertfalse;
}
#endif

BoxModel& boxModel(GuiItem& item) {
    // This is a convenience function that only works if the given GUI item
    // is decorated as a CommonGuiItem!
    auto* decorated = dynamic_cast<jive::GuiItemDecorator*>(&item);
    jassert(decorated != nullptr);
    auto* common = decorated->getTopLevelDecorator().toType<jive::CommonGuiItem>();
    jassert(common != nullptr);

    return common->boxModel;
}

const BoxModel& boxModel(const GuiItem& item) {
    return boxModel(*const_cast<GuiItem*>(&item));
}

GuiItem* findItemWithID(GuiItem& root, const juce::Identifier& id) {
    if (root.state["id"].toString() == id.toString()) {
        return &root;
    }

    for (auto* child : root.getChildren()) {
        if (auto* item = findItemWithID(*child, id)) {
            return item;
        }
    }

    return nullptr;
}
} // namespace jive
