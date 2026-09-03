//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_FlexContainer.h"

#include "jive_FlexItem.h"

#include "jive_CommonGuiItem.h"

namespace jive {
FlexContainer::FlexContainer(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem { std::move(itemToDecorate) }
    , flexDirection { state, "flex-direction" }
    , flexWrap { state, "flex-wrap" }
    , flexJustifyContent { state, "justify-content" }
    , flexAlignItems { state, "align-items" }
    , flexAlignContent { state, "align-content" } {
    jassert(state.hasProperty("display"));
    jassert(state["display"] == juce::VariantConverter<Display>::toVar(Display::flex));

    if (!flexDirection.exists()) {
        flexDirection = juce::FlexBox::Direction::column;
    }

    flexDirection.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    flexWrap.onValueChange = [this] { updateIdealSizeUnrestrained(); };
    flexJustifyContent.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    flexAlignItems.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };
    flexAlignContent.onValueChange = [this] { callLayoutChildrenWithRecursionLock(); };

    state.addListener(this);
}

FlexContainer::~FlexContainer() {
    state.removeListener(this);
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

    do {
        changesDuringLayout = false;
        auto flexBox = buildFlexBox(bounds, LayoutStrategy::real);
        flexBox.performLayout(bounds);
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
    flex.performLayout(constraints);

    juce::Point extremities { -1.0f, -1.0f };

    for (const auto& flexItem : flex.items) {
        const auto right = flexItem.currentBounds.getRight() + flexItem.margin.right;
        const auto bottom = flexItem.currentBounds.getBottom() + flexItem.margin.bottom;

        if (right > extremities.x) {
            extremities.x = right;
        }
        if (bottom > extremities.y) {
            extremities.y = bottom;
        }
    }

    auto& currentBoxModel = boxModel(*this);

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

juce::FlexBox FlexContainer::buildFlexBox(juce::Rectangle<float> bounds, LayoutStrategy strategy) {
    juce::FlexBox flex;

    flex.flexDirection = flexDirection;
    flex.flexWrap = flexWrap;

    appendChildren(*this, flex, bounds, strategy);

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
