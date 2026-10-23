//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/layout/jive_FlexContainer.h"

#include "UI/jive/core/layout/jive_FlexItem.h"

#include "UI/jive/core/layout/jive_CommonGuiItem.h"

namespace jive {
FlexContainer::FlexContainer(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem { std::move(itemToDecorate) }
    , flexDirection { state, "flex-direction" }
    , flexWrap { state, "flex-wrap" }
    , flexJustifyContent { state, "justify-content" }
    , flexAlignItems { state, "align-items" }
    , flexAlignContent { state, "align-content" }
    , gap { state, "gap" } {
    jassert(state.hasProperty("display"));
    jassert(state["display"] == juce::VariantConverter<Display>::toVar(Display::flex));

    if (!flexDirection.exists()) {
        flexDirection = juce::FlexBox::Direction::column;
    }

    flexDirection.onValueChange = [this] { updateIdealSize(); };
    flexWrap.onValueChange = [this] { updateIdealSize(); };
    flexJustifyContent.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    flexAlignItems.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    flexAlignContent.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    gap.onValueChange = [this] { updateIdealSize(); };
    gap.onTransitionProgressed = [this] { updateIdealSize(); };
}

FlexContainer::~FlexContainer() = default;

// juce::FlexBox 没有 gap：给每项加半个 gap 的 margin，并把布局区域四周各扩展半个 gap，
// 使间距只出现在相邻项之间，且换行时每行一致。
static void applyGaps(juce::FlexBox& flex, juce::Point<float> gaps) {
    for (auto& flexItem : flex.items) {
        flexItem.margin.left += gaps.x * 0.5f;
        flexItem.margin.right += gaps.x * 0.5f;
        flexItem.margin.top += gaps.y * 0.5f;
        flexItem.margin.bottom += gaps.y * 0.5f;
    }
}

static juce::Rectangle<float> expandToAccountForGaps(juce::Rectangle<float> bounds, juce::Point<float> gaps) {
    return bounds.expanded(gaps.x * 0.5f, gaps.y * 0.5f);
}

juce::Point<float> FlexContainer::calculateGaps() const {
    const auto gaps = gap.exists() ? gap.calculateCurrent() : juce::Array<juce::Grid::Px> {};
    const auto rowGap = gaps.size() > 0 ? static_cast<float>(gaps.getUnchecked(0).pixels) : 0.0f;
    const auto columnGap = gaps.size() > 1 ? static_cast<float>(gaps.getUnchecked(1).pixels) : rowGap;

    return { columnGap, rowGap };
}

void FlexContainer::layOutChildren() {
    if (layoutRecursionLock) {
        return;
    }

    const juce::ScopedValueSetter svs { layoutRecursionLock, true };

    GuiItemDecorator::layOutChildren();

    const auto bounds = boxModel(*this).getContentBounds();

    if (bounds.isEmpty()) {
        return;
    }

    const auto gaps = calculateGaps();
    const auto boundsToLayOutIn = expandToAccountForGaps(bounds, gaps);

    do {
        changesDuringLayout = false;
        auto flexBox = buildFlexBox(bounds, LayoutStrategy::real);
        flexBox.performLayout(boundsToLayOutIn);
    } while (changesDuringLayout);
}

FlexContainer::operator juce::FlexBox() {
    return buildFlexBox(boxModel(*this).getContentBounds(), LayoutStrategy::real);
}

juce::Rectangle<float> FlexContainer::calculateIdealSize(juce::Rectangle<float> constraints) const {
    constraints = constraints.withZeroOrigin();

    switch (flexDirection.getOr(juce::FlexBox {}.flexDirection)) {
    case juce::FlexBox::Direction::column:
    case juce::FlexBox::Direction::columnReverse:
        constraints.setHeight(static_cast<float>(std::numeric_limits<juce::uint16>::max()));
        break;
    case juce::FlexBox::Direction::row:
    case juce::FlexBox::Direction::rowReverse:
        constraints.setWidth(static_cast<float>(std::numeric_limits<juce::uint16>::max()));
        break;
    default:
        jassertfalse;
    }

    auto flex = const_cast<FlexContainer&>(*this).buildFlexBox(constraints, LayoutStrategy::dummy);
    const auto gaps = calculateGaps();
    flex.performLayout(expandToAccountForGaps(constraints, gaps));

    juce::Point extremities { -1.0f, -1.0f };

    for (const auto& flexItem : flex.items) {
        const auto right = flexItem.currentBounds.getRight() + flexItem.margin.right - gaps.x * 0.5f;
        const auto bottom = flexItem.currentBounds.getBottom() + flexItem.margin.bottom - gaps.y * 0.5f;

        extremities.x = std::max(extremities.x, right);
        extremities.y = std::max(extremities.y, bottom);
    }

    const auto& currentBoxModel = boxModel(*this);

    return {
        extremities.x + currentBoxModel.getPadding().getLeftAndRight() + currentBoxModel.getBorder().getLeftAndRight(),
        extremities.y + currentBoxModel.getPadding().getTopAndBottom() + currentBoxModel.getBorder().getTopAndBottom(),
    };
}

static void appendChildren(GuiItem& container, juce::FlexBox& flex, juce::Rectangle<float> bounds,
                           LayoutStrategy strategy) {
    for (auto* child : container.getChildren()) {
        if (auto* const decoratedItem = dynamic_cast<GuiItemDecorator*>(child)) {
            if (auto* const flexItem = decoratedItem->toType<FlexItem>()) {
                flex.items.add(flexItem->toJuceFlexItem(bounds, strategy));
            }
        }
    }
}

void FlexContainer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& id) {
    ContainerItem::valueTreePropertyChanged(tree, id);

    if ((tree == state || tree.getParent() == state) && layoutRecursionLock) {
        static const juce::Array<juce::Identifier> propertiesForWhichChangesRequireAnotherLayOut {
            "ideal-width",
            "ideal-height",
        };

        if (propertiesForWhichChangesRequireAnotherLayOut.contains(id)) {
            changesDuringLayout = true;
        }
    }

    forwardPropertyChange(tree, id, flexDirection, flexWrap, flexJustifyContent, flexAlignItems, flexAlignContent, gap);
}

juce::FlexBox FlexContainer::buildFlexBox(juce::Rectangle<float> bounds, LayoutStrategy strategy) {
    juce::FlexBox flex;

    flex.flexDirection = flexDirection;
    flex.flexWrap = flexWrap;

    appendChildren(*this, flex, bounds, strategy);
    applyGaps(flex, calculateGaps());

    switch (strategy) {
    case LayoutStrategy::real:
        flex.justifyContent = flexJustifyContent;
        flex.alignItems = flexAlignItems;
        flex.alignContent = flexAlignContent;
        break;
    case LayoutStrategy::dummy:
        flex.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        flex.alignItems = juce::FlexBox::AlignItems::flexStart;
        flex.alignContent = juce::FlexBox::AlignContent::flexStart;
        break;
    default:
        jassertfalse;
    }

    return flex;
}
} // namespace jive
