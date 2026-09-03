//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_GridContainer.h"

#include "jive_GridItem.h"

#include "jive_CommonGuiItem.h"
#include "jive_Text.h"

namespace jive {
GridContainer::GridContainer(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem(std::move(itemToDecorate))
    , justifyItems { state, "justify-items" }
    , alignItems { state, "align-items" }
    , justifyContent { state, "justify-content" }
    , alignContent { state, "align-content" }
    , gridAutoFlow { state, "grid-auto-flow" }
    , gridTemplateColumns { state, "grid-template-columns" }
    , gridTemplateRows { state, "grid-template-rows" }
    , gridTemplateAreas { state, "grid-template-areas" }
    , gridAutoRows { state, "grid-auto-rows" }
    , gridAutoColumns { state, "grid-auto-columns" }
    , gap { state, "gap" } {
    jassert(state.hasProperty("display"));
    jassert(state["display"] == juce::VariantConverter<Display>::toVar(Display::grid));

    static const juce::Grid defaultGrid;

    if (!justifyItems.exists()) {
        justifyItems = defaultGrid.justifyItems;
    }
    if (!alignItems.exists()) {
        alignItems = defaultGrid.alignItems;
    }
    if (!justifyContent.exists()) {
        justifyContent = defaultGrid.justifyContent;
    }
    if (!alignContent.exists()) {
        alignContent = defaultGrid.alignContent;
    }
    if (!gridAutoFlow.exists()) {
        gridAutoFlow = defaultGrid.autoFlow;
    }
    if (!gridAutoRows.exists()) {
        gridAutoRows = defaultGrid.autoRows;
    }
    if (!gridAutoColumns.exists()) {
        gridAutoColumns = defaultGrid.autoColumns;
    }

    justifyItems.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    alignItems.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    justifyContent.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    alignContent.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    gridAutoFlow.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gridTemplateColumns.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gridTemplateColumns.onTransitionProgressed = [this] { updateIdealSizeUnrestrained(); };
    gridTemplateRows.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gridTemplateRows.onTransitionProgressed = [this] { updateIdealSizeUnrestrained(); };
    gridTemplateAreas.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gridAutoRows.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gridAutoColumns.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gap.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    gap.onTransitionProgressed = [this] { updateIdealSizeUnrestrained(); };

    state.addListener(this);
}

GridContainer::~GridContainer() {
    state.removeListener(this);
}

void GridContainer::layOutChildren() {
    if (layoutRecursionLock) {
        return;
    }

    const juce::ScopedValueSetter svs { layoutRecursionLock, true };

    GuiItemDecorator::layOutChildren();

    const auto bounds = boxModel(*this).getContentBounds().toNearestInt();

    if (bounds.isEmpty()) {
        return;
    }

    do {
        changesDuringLayout = false;
        buildGrid(bounds, LayoutStrategy::real).performLayout(bounds);
    } while (changesDuringLayout);
}

GridContainer::operator juce::Grid() {
    return buildGrid(boxModel(*this).getContentBounds().toNearestInt(), LayoutStrategy::real);
}

juce::Rectangle<float> GridContainer::calculateIdealSize(juce::Rectangle<float> constraints) const {
    auto integerConstraints = constraints.toNearestInt().withZeroOrigin();
    integerConstraints.setHeight(static_cast<int>(std::numeric_limits<juce::uint16>::max()));

    auto grid = const_cast<GridContainer&>(*this).buildGrid(integerConstraints, LayoutStrategy::dummy);
    grid.performLayout(integerConstraints);

    juce::Point extremities { -1.0f, -1.0f };

    for (const auto& gridItem : grid.items) {
        const auto right = gridItem.currentBounds.getRight() + gridItem.margin.right;
        extremities.x = std::max(extremities.x, right);

        const auto bottom = gridItem.currentBounds.getBottom() + gridItem.margin.bottom;
        extremities.y = std::max(extremities.y, bottom);
    }

    const auto& currentBoxModel = boxModel(*this);

    return {
        extremities.x + currentBoxModel.getPadding().getLeftAndRight() + currentBoxModel.getBorder().getLeftAndRight(),
        extremities.y + currentBoxModel.getPadding().getTopAndBottom() + currentBoxModel.getBorder().getTopAndBottom(),
    };
}

static void appendChildren(GuiItem& container, juce::Grid& grid, juce::Rectangle<int> bounds, LayoutStrategy strategy) {
    for (auto* child : container.getChildren()) {
        if (auto* const decoratedItem = dynamic_cast<GuiItemDecorator*>(child)) {
            if (auto* const gridItem = decoratedItem->toType<GridItem>()) {
                grid.items.add(gridItem->toJuceGridItem(bounds.toFloat(), strategy));
            }
        }
    }
}

void GridContainer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& id) {
    if (tree != state && tree.getParent() != state) {
        return;
    }

    if (layoutRecursionLock) {
        static const juce::Array<juce::Identifier> propertiesForWhichChangesRequireAnotherLayOut {
            "ideal-width",
            "ideal-height",
        };

        if (propertiesForWhichChangesRequireAnotherLayOut.contains(id)) {
            changesDuringLayout = true;
        }
    }
}

juce::Grid GridContainer::buildGrid(juce::Rectangle<int> bounds, LayoutStrategy strategy) {
    juce::Grid grid;

    grid.autoFlow = gridAutoFlow;
    grid.templateColumns = gridTemplateColumns.calculateCurrent();
    grid.templateRows = gridTemplateRows.calculateCurrent();
    grid.templateAreas = gridTemplateAreas;
    grid.autoRows = gridAutoRows;
    grid.autoColumns = gridAutoColumns;

    const auto gaps = gap.calculateCurrent();
    grid.rowGap = gaps.size() > 0 ? gaps.getUnchecked(0) : juce::Grid::Px { 0 };
    grid.columnGap = gaps.size() > 1 ? gaps.getUnchecked(1) : grid.rowGap;

    appendChildren(*this, grid, bounds, strategy);

    switch (strategy) {
    case LayoutStrategy::real:
        grid.justifyItems = justifyItems;
        grid.alignItems = alignItems;
        grid.justifyContent = justifyContent;
        grid.alignContent = alignContent;
        break;
    case LayoutStrategy::dummy:
        grid.justifyItems = juce::Grid::JustifyItems::start;
        grid.alignItems = juce::Grid::AlignItems::start;
        grid.justifyContent = juce::Grid::JustifyContent::start;
        grid.alignContent = juce::Grid::AlignContent::start;

        for (auto& column : grid.templateColumns) {
            if (column.isFractional()) {
                column = juce::Grid::TrackInfo {};
            }
        }
        for (auto& row : grid.templateRows) {
            if (row.isFractional()) {
                row = juce::Grid::TrackInfo {};
            }
        }

        break;
    }

    return grid;
}
} // namespace jive
