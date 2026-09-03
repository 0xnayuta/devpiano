//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_BlockItem.h"

#include "jive_CommonGuiItem.h"
#include "jive_GuiItem.h"

namespace jive {
BlockItem::BlockItem(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem::Child { std::move(itemToDecorate) }
    , x { state, "x" }
    , y { state, "y" }
    , centreX { state, "centre-x" }
    , centreY { state, "centre-y" }
    , width { state, "width" }
    , height { state, "height" } {
    jassert(getParent() != nullptr);

    const auto updateBounds = [this] { getComponent()->setBounds(calculateBounds()); };

    x.onValueChange = [this, updateBounds] {
        centreX.clear();
        updateBounds();
    };
    x.onTransitionProgressed = updateBounds;

    y.onValueChange = [this, updateBounds]() {
        centreY.clear();
        updateBounds();
    };
    y.onTransitionProgressed = updateBounds;

    centreX.onValueChange = [this, updateBounds]() {
        x.clear();
        updateBounds();
    };
    centreX.onTransitionProgressed = updateBounds;

    centreY.onValueChange = [this, updateBounds]() {
        y.clear();
        updateBounds();
    };
    centreY.onTransitionProgressed = updateBounds;

    width.onTransitionProgressed = [this] { getComponent()->setBounds(calculateBounds()); };
    height.onTransitionProgressed = [this] { getComponent()->setBounds(calculateBounds()); };

    updateBounds();
}

int BlockItem::calculateX() const {
    const auto parentContentBounds = boxModel(*getParent()).getContentBounds();

    if (centreX.exists()) {
        return juce::roundToInt(centreX.toPixels(parentContentBounds) - boxModel(*this).getWidth() / 2.f);
    }

    return juce::roundToInt(x.toPixels(parentContentBounds));
}

int BlockItem::calculateY() const {
    const auto parentContentBounds = boxModel(*getParent()).getContentBounds();

    if (centreY.exists()) {
        return juce::roundToInt(centreY.toPixels(parentContentBounds) - boxModel(*this).getHeight() / 2.f);
    }

    return juce::roundToInt(y.toPixels(parentContentBounds));
}

juce::Rectangle<int> BlockItem::calculateBounds() const {
    juce::Rectangle<int> bounds;
    const auto& parentBoxModel = boxModel(*getParent());
    const auto parentBounds = parentBoxModel.getContentBounds();

    if (!width.isAuto()) {
        bounds.setWidth(juce::roundToInt(width.toPixels(parentBounds)));
    }
    if (!height.isAuto()) {
        bounds.setHeight(juce::roundToInt(height.toPixels(parentBounds)));
    }

    return bounds.withPosition(parentBoxModel.getContentBounds().getPosition().roundToInt()
                               + juce::Point {
                                   calculateX(),
                                   calculateY(),
                               });
}
} // namespace jive
