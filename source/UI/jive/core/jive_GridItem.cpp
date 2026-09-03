//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_GridItem.h"

namespace jive {
GridItem::GridItem(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem::Child { std::move(itemToDecorate) }
    , order { state, "order" }
    , justifySelf { state, "justify-self" }
    , alignSelf { state, "align-self" }
    , gridColumn { state, "grid-column" }
    , gridRow { state, "grid-row" }
    , gridArea { state, "grid-area" } {
    static const juce::GridItem defaultGridItem;

    if (!justifySelf.exists()) {
        justifySelf = defaultGridItem.justifySelf;
    }
    if (!alignSelf.exists()) {
        alignSelf = defaultGridItem.alignSelf;
    }
    if (!gridColumn.exists()) {
        gridColumn = defaultGridItem.column;
    }
    if (!gridRow.exists()) {
        gridRow = defaultGridItem.row;
    }
    if (!gridArea.exists()) {
        gridArea = defaultGridItem.area;
    }

    const auto updateParentLayout = [this]() {
        cachedItems.clear();

        if (auto* containerParent
            = dynamic_cast<GuiItemDecorator&>(*getParent()).getTopLevelDecorator().toType<ContainerItem>()) {
            containerParent->updateIdealSizeUnrestrained();
        }
    };
    order.onValueChange = updateParentLayout;
    justifySelf.onValueChange = updateParentLayout;
    alignSelf.onValueChange = updateParentLayout;
    gridColumn.onValueChange = updateParentLayout;
    gridRow.onValueChange = updateParentLayout;
    gridArea.onValueChange = updateParentLayout;

    box.addListener(*this);
}

GridItem::~GridItem() {
    box.removeListener(*this);
}

juce::GridItem GridItem::toJuceGridItem(juce::Rectangle<float> parentContentBounds, LayoutStrategy strategy) {
    const auto key = std::make_pair(parentContentBounds, strategy);

    if (cachedItems.find(key) == std::end(cachedItems)) {
        juce::GridItem gridItem { *getComponent() };

        gridItem.column = gridColumn;
        gridItem.row = gridRow;
        gridItem.area = gridArea;

        applyConstraints(gridItem, parentContentBounds, Orientation::vertical, strategy);

        switch (strategy) {
        case LayoutStrategy::real:
            gridItem.justifySelf = justifySelf;
            gridItem.alignSelf = alignSelf;
            break;
        case LayoutStrategy::dummy:
            gridItem.justifySelf = juce::GridItem::JustifySelf::stretch;
            gridItem.alignSelf = juce::GridItem::AlignSelf::stretch;

            if (gridItem.width < 0.0f && gridItem.minWidth > 0.0f) {
                gridItem.width = gridItem.minWidth;
            }
            if (gridItem.height < 0.0f && gridItem.minHeight > 0.0f) {
                gridItem.height = gridItem.minHeight;
            }
        }

        cachedItems[key] = gridItem;
    }

    return cachedItems.find(key)->second;
}

void GridItem::boxModelChanged(BoxModel&) {
    cachedItems.clear();
}
} // namespace jive
