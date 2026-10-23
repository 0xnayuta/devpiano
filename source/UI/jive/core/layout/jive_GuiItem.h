//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_core.h"

#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
#include "jive_style_sheets.h"
#endif

#if JIVE_IS_PLUGIN_PROJECT
#include <juce_audio_processors/juce_audio_processors.h>
#endif

namespace jive {
class GuiItem {
public:
    GuiItem(std::unique_ptr<juce::Component> component, const juce::ValueTree& sourceState,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
            StyleSheet::ReferenceCountedPointer styleSheet,
#endif
            GuiItem* parent = nullptr);

    GuiItem(const GuiItem& other);
    virtual ~GuiItem();

    [[nodiscard]] std::shared_ptr<const juce::Component> getComponent() const;
    [[nodiscard]] std::shared_ptr<juce::Component> getComponent();

    virtual void insertChild(std::unique_ptr<GuiItem> child, int index);
    virtual void setChildren(std::vector<std::unique_ptr<GuiItem>>&& children);
    virtual void removeChild(GuiItem& childToRemove);
    [[nodiscard]] virtual juce::Array<const GuiItem*> getChildren() const;
    [[nodiscard]] virtual juce::Array<GuiItem*> getChildren();
    [[nodiscard]] virtual const GuiItem* getParent() const;
    [[nodiscard]] virtual GuiItem* getParent();

    [[nodiscard]] bool isTopLevel() const;
    [[nodiscard]] virtual bool isContainer() const;
    [[nodiscard]] virtual bool isContent() const;

    void callLayoutChildrenWithRecursionLock();
    [[nodiscard]] bool isLayingOutChildren() const;

#if JIVE_IS_PLUGIN_PROJECT
    virtual void attachToParameter(juce::RangedAudioParameter*, juce::UndoManager* = nullptr);
#endif

    juce::ValueTree state;

protected:
    virtual void layOutChildren() {
    }
    virtual void childrenChanged() {
    }

private:
    friend class GuiItemDecorator;

    GuiItem(std::shared_ptr<juce::Component> component, GuiItem* parent,
#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
            StyleSheet::ReferenceCountedPointer sheet,
#endif
            const juce::ValueTree& sourceState);

    void insertChild(std::unique_ptr<GuiItem> child, int index, bool invokeCallback);

#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
    const StyleSheet::ReferenceCountedPointer styleSheet;
#endif

    const std::shared_ptr<juce::Component> component;
    GuiItem* const parent;
    juce::OwnedArray<GuiItem> children;

    bool layoutRecursionLock = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE(GuiItem)
    JUCE_LEAK_DETECTOR(GuiItem)
};

[[nodiscard]] BoxModel& boxModel(GuiItem& decoratedItem);
[[nodiscard]] const BoxModel& boxModel(const GuiItem& decoratedItem);

[[nodiscard]] GuiItem* findItemWithID(GuiItem& root, const juce::Identifier& id);
} // namespace jive
