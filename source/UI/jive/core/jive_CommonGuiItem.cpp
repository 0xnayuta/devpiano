//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_CommonGuiItem.h"

namespace jive {
CommonGuiItem::CommonGuiItem(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , boxModel { state }
#if !JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
    , interactionState { *getComponent(), state }
#endif
    , name { state, "name" }
    , title { state, "title" }
    , id { state, "id" }
    , description { state, "description" }
    , tooltip { state, "tooltip" }
    , enabled { state, "enabled" }
    , accessible { state, "accessible" }
    , visibility { state, "visibility" }
    , alwaysOnTop { state, "always-on-top" }
    , bufferedToImage { state, "buffered-to-image" }
    , opaque { state, "opaque" }
    , focusable { state, "focusable" }
    , clickingGrabsFocus { state, "clicking-grabs-focus" }
    , focusOutline { state, "focus-outline" }
    , focusOrder { state, "focus-order" }
    , opacity { state, "opacity" }
    , cursor { state, "cursor" }
    , display { state, "display" }
    , width { state, "width" }
    , height { state, "height" } {
    if (!enabled.exists()) {
        enabled = true;
    }
    if (!accessible.exists()) {
        accessible = true;
    }
    if (!visibility.exists()) {
        visibility = true;
    }
    if (!clickingGrabsFocus.exists()) {
        clickingGrabsFocus = true;
    }
    if (!focusOrder.exists()) {
        focusOrder = state.getParent().indexOf(state) + 1;
    }
    if (!opacity.exists()) {
        opacity = 1.0f;
    }
    if (!cursor.exists()) {
        cursor = juce::MouseCursor::NormalCursor;
    }
    if (!display.exists()) {
        display = Display::flex;
    }

    getComponent()->addComponentListener(this);

    name.onValueChange = [this]() { getComponent()->setName(name); };
    getComponent()->setName(name);

    title.onValueChange = [this]() { getComponent()->setTitle(title); };
    getComponent()->setTitle(title);

    id.onValueChange = [this]() { getComponent()->setComponentID(id.get().toString()); };
    getComponent()->setComponentID(id.get().toString());

    description.onValueChange = [this]() { getComponent()->setDescription(description); };
    getComponent()->setDescription(description);

    tooltip.onValueChange = [this]() { getComponent()->setHelpText(tooltip); };
    getComponent()->setHelpText(tooltip);

    enabled.onValueChange = [this]() { getComponent()->setEnabled(enabled); };
    getComponent()->setEnabled(enabled);

    accessible.onValueChange = [this]() { getComponent()->setAccessible(accessible); };
    getComponent()->setAccessible(accessible);

    visibility.onValueChange = [this]() { getComponent()->setVisible(visibility); };
    getComponent()->setVisible(visibility);

    alwaysOnTop.onValueChange = [this]() { getComponent()->setAlwaysOnTop(alwaysOnTop); };
    getComponent()->setAlwaysOnTop(alwaysOnTop);

    bufferedToImage.onValueChange = [this]() { getComponent()->setBufferedToImage(bufferedToImage); };
    getComponent()->setBufferedToImage(bufferedToImage);

    opaque.onValueChange = [this]() { getComponent()->setOpaque(opaque); };
    getComponent()->setOpaque(opaque);

    focusable.onValueChange = [this]() { getComponent()->setWantsKeyboardFocus(focusable); };
    getComponent()->setWantsKeyboardFocus(focusable);

    focusOutline.onValueChange = [this]() { getComponent()->setHasFocusOutline(focusOutline); };
    getComponent()->setHasFocusOutline(focusOutline);

    clickingGrabsFocus.onValueChange
        = [this]() { getComponent()->setMouseClickGrabsKeyboardFocus(clickingGrabsFocus); };
    getComponent()->setMouseClickGrabsKeyboardFocus(clickingGrabsFocus);

    focusOrder.onValueChange = [this]() { getComponent()->setExplicitFocusOrder(focusOrder); };
    getComponent()->setExplicitFocusOrder(focusOrder);

    const auto updateOpacity = [this] { getComponent()->setAlpha(opacity.calculateCurrent()); };
    opacity.onValueChange = updateOpacity;
    opacity.onTransitionProgressed = updateOpacity;
    getComponent()->setAlpha(opacity);

    cursor.onValueChange = [this]() { getComponent()->setMouseCursor(juce::MouseCursor { cursor }); };
    getComponent()->setMouseCursor(juce::MouseCursor { cursor });

    if (isTopLevel()) {
        // Top-level items must have an explicit size!
        jassert(!boxModel.hasAutoWidth());
        jassert(!boxModel.hasAutoHeight());
    }

    getComponent()->setSize(juce::roundToInt(boxModel.getOuterBounds().getWidth()),
                            juce::roundToInt(boxModel.getOuterBounds().getHeight()));

    boxModel.addListener(*this);
}

CommonGuiItem::~CommonGuiItem() {
    getComponent()->removeComponentListener(this);
}

void CommonGuiItem::componentMovedOrResized(juce::Component& componentThatWasMovedOrResized, bool /*wasMoved*/,
                                            bool wasResized) {
    if (&componentThatWasMovedOrResized != getComponent().get()) {
        return;
    }

    if (!wasResized) {
        return;
    }

    const auto componentBounds = getComponent()->getBounds().toFloat();

    if (!width.isTransitioning() && !height.isTransitioning()) {
        boxModel.setSize(componentBounds.getWidth(), componentBounds.getHeight());
    } else if (!width.isTransitioning()) {
        boxModel.setWidth(componentBounds.getWidth());
    } else if (!height.isTransitioning()) {
        boxModel.setHeight(componentBounds.getHeight());
    }
}

void CommonGuiItem::componentVisibilityChanged(juce::Component& componentThatChangedVisiblity) {
    if (&componentThatChangedVisiblity != getComponent().get()) {
        return;
    }

    visibility = getComponent()->isVisible();
}

void CommonGuiItem::componentNameChanged(juce::Component& componentThatChangedName) {
    if (&componentThatChangedName != getComponent().get()) {
        return;
    }

    name = getComponent()->getName();
}

void CommonGuiItem::componentEnablementChanged(juce::Component& componentThatChangedEnablement) {
    if (&componentThatChangedEnablement != getComponent().get()) {
        return;
    }

    enabled = getComponent()->isEnabled();
}

[[nodiscard]] static auto hasWidgetRole(const juce::Component& component) {
    if (auto* handler = const_cast<juce::Component*>(&component)->getAccessibilityHandler()) {
        switch (handler->getRole()) {
        case juce::AccessibilityRole::button:
        case juce::AccessibilityRole::toggleButton:
        case juce::AccessibilityRole::radioButton:
        case juce::AccessibilityRole::comboBox:
        case juce::AccessibilityRole::image:
        case juce::AccessibilityRole::slider:
        case juce::AccessibilityRole::label:
        case juce::AccessibilityRole::staticText:
        case juce::AccessibilityRole::editableText:
        case juce::AccessibilityRole::hyperlink:
        case juce::AccessibilityRole::progressBar:
        case juce::AccessibilityRole::scrollBar:
        case juce::AccessibilityRole::tooltip:
            return true;

        case juce::AccessibilityRole::menuItem:
        case juce::AccessibilityRole::menuBar:
        case juce::AccessibilityRole::popupMenu:
        case juce::AccessibilityRole::table:
        case juce::AccessibilityRole::tableHeader:
        case juce::AccessibilityRole::column:
        case juce::AccessibilityRole::row:
        case juce::AccessibilityRole::cell:
        case juce::AccessibilityRole::list:
        case juce::AccessibilityRole::listItem:
        case juce::AccessibilityRole::tree:
        case juce::AccessibilityRole::treeItem:
        case juce::AccessibilityRole::group:
        case juce::AccessibilityRole::dialogWindow:
        case juce::AccessibilityRole::window:
        case juce::AccessibilityRole::splashScreen:
        case juce::AccessibilityRole::ignored:
        case juce::AccessibilityRole::unspecified:
        default:
            return false;
        }
    }

    return false;
}

void CommonGuiItem::componentParentHierarchyChanged(juce::Component& componentThatsParentChanged) {
    if (&componentThatsParentChanged != getComponent().get()) {
        return;
    }

    if (auto* parentComponent = getComponent()->getParentComponent()) {
        if (hasWidgetRole(*parentComponent)) {
            getComponent()->setAccessible(false);
        } else {
            getComponent()->setAccessible(accessible);
        }
    }
}

void CommonGuiItem::boxModelChanged(BoxModel& boxModelThatChanged) {
    jassertquiet(&boxModelThatChanged == &boxModel);

    getComponent()->removeComponentListener(this);
    getComponent()->setSize(juce::roundToInt(boxModel.getWidth()), juce::roundToInt(boxModel.getHeight()));
    getComponent()->addComponentListener(this);
    getTopLevelDecorator().callLayoutChildrenWithRecursionLock();
}

void CommonGuiItem::childrenChanged() {
    getTopLevelDecorator().callLayoutChildrenWithRecursionLock();
}
} // namespace jive
