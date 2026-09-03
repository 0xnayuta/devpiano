//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_ContainerItem.h"

#include "jive_CommonGuiItem.h"

namespace jive {
ContainerItem::ContainerItem(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , box { boxModel(*this) }
    , idealWidth { state, "ideal-width" }
    , idealHeight { state, "ideal-height" } {
    box.addListener(*this);
}

ContainerItem::~ContainerItem() {
    box.removeListener(*this);
}

void ContainerItem::insertChild(std::unique_ptr<GuiItem> child, int index) {
    const auto numChildrenBefore = getChildren().size();
    GuiItemDecorator::insertChild(std::move(child), index);

    if (getChildren().size() != numChildrenBefore) {
        updateIdealSizeUnrestrained();
    }
}

void ContainerItem::setChildren(std::vector<std::unique_ptr<GuiItem>>&& newChildren) {
    {
        const BoxModel::ScopedCallbackLock boxModelLock { box };
        GuiItemDecorator::setChildren(std::move(newChildren));
    }

    if (!getChildren().isEmpty()) {
        updateIdealSizeUnrestrained();
    }
}

void ContainerItem::updateIdealSizeUnrestrained() {
    updateIdealSize({
        static_cast<float>(std::numeric_limits<juce::uint16>::max()),
        static_cast<float>(std::numeric_limits<juce::uint16>::max()),
    });
}

void ContainerItem::updateIdealSizeWithinConstraints() {
    updateIdealSize(box.getContentBounds());
}

void ContainerItem::updateIdealSize(juce::Rectangle<float> constraints) {
    const auto newIdealSize = calculateIdealSize(constraints);
    const auto widthChanged = !juce::approximatelyEqual(newIdealSize.getWidth(), idealWidth.get());
    const auto heightChanged = !juce::approximatelyEqual(newIdealSize.getHeight(), idealHeight.get());

    if (widthChanged && heightChanged) {
        {
            BoxModel::ScopedCallbackLock boxModelLock { box };
            idealWidth = newIdealSize.getWidth();
        }
        idealHeight = newIdealSize.getHeight();
    } else if (widthChanged) {
        idealWidth = newIdealSize.getWidth();
    } else if (heightChanged) {
        idealHeight = newIdealSize.getHeight();
    } else {
        callLayoutChildrenWithRecursionLock();
    }

    if ((widthChanged || heightChanged) && getParent() != nullptr) {
        if (auto* containerParent
            = dynamic_cast<GuiItemDecorator&>(*getParent()).getTopLevelDecorator().toType<ContainerItem>()) {
            containerParent->updateIdealSizeUnrestrained();
        }
    }
}
} // namespace jive
