//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_Display.h"
#include "jive_GuiItemDecorator.h"

namespace jive {
class CommonGuiItem : public GuiItemDecorator,
                      private juce::ComponentListener,
                      private juce::ValueTree::Listener,
                      private BoxModel::Listener {
public:
    explicit CommonGuiItem(std::unique_ptr<GuiItem> itemToDecorate);
    ~CommonGuiItem() override;

    BoxModel boxModel;

protected:
    void componentMovedOrResized(juce::Component& component, bool wasMoved, bool wasResized) override;
    void componentVisibilityChanged(juce::Component& component) override;
    void componentNameChanged(juce::Component& component) override;
    void componentEnablementChanged(juce::Component& component) override;
    void componentParentHierarchyChanged(juce::Component& component) override;
    void boxModelChanged(BoxModel& boxModel) override;

    void childrenChanged() override;

private:
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) final;

#if !JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
    ComponentInteractionState interactionState;
#endif
    PassiveProperty<juce::String> name;
    PassiveProperty<juce::String> title;
    PassiveProperty<juce::Identifier> id;
    PassiveProperty<juce::String> description;
    PassiveProperty<juce::String> tooltip;
    PassiveProperty<bool> enabled;
    PassiveProperty<bool> accessible;
    PassiveProperty<bool> visibility;
    PassiveProperty<bool> alwaysOnTop;
    PassiveProperty<bool> bufferedToImage;
    PassiveProperty<bool> opaque;
    PassiveProperty<bool> focusable;
    PassiveProperty<bool> clickingGrabsFocus;
    PassiveProperty<bool> focusOutline;
    PassiveProperty<int> focusOrder;
    PassiveProperty<float> opacity;
    PassiveProperty<juce::MouseCursor::StandardCursorType> cursor;
    PassiveProperty<Display> display;
    PassiveProperty<Length> width;
    PassiveProperty<Length> height;

    JUCE_LEAK_DETECTOR(CommonGuiItem)
};
} // namespace jive
