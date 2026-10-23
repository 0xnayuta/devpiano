//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/layout/jive_FlexItem.h"

namespace jive {
class FlexLayoutDummy : public juce::Component {
public:
    explicit FlexLayoutDummy(GuiItem& owner)
        : item { owner } {
    }

    void resized() final {
        if (strategy == LayoutStrategy::real) {
            boxModel(item).setSize(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        }
    }

    void moved() final {
        if (strategy == LayoutStrategy::real) {
            item.getComponent()->setTopLeftPosition(getPosition());
        }
    }

    void setStrategy(LayoutStrategy layoutStrategy) {
        strategy = layoutStrategy;
    }

private:
    LayoutStrategy strategy;
    GuiItem& item;
};

FlexItem::FlexItem(std::unique_ptr<GuiItem> itemToDecorate)
    : ContainerItem::Child { std::move(itemToDecorate) }
    , order { state, "order" }
    , flexGrow { state, "flex-grow" }
    , flexShrink { state, "flex-shrink" }
    , flexBasis { state, "flex-basis" }
    , alignSelf { state, "align-self" }
    , parentDirection { state.getParent(), "flex-direction" }
    , layoutDummy { std::make_unique<FlexLayoutDummy>(*this) } {
    if (!flexShrink.exists()) {
        flexShrink = juce::FlexItem {}.flexShrink;
    }

    const auto updateParentLayout = [this]() {
        cachedItems.clear();

        if (auto* containerParent
            = dynamic_cast<GuiItemDecorator&>(*getParent()).getTopLevelDecorator().toType<ContainerItem>()) {
            containerParent->updateIdealSize();
        }
    };
    order.onValueChange = updateParentLayout;
    flexGrow.onValueChange = updateParentLayout;
    flexGrow.onTransitionProgressed = updateParentLayout;
    flexShrink.onValueChange = updateParentLayout;
    flexShrink.onTransitionProgressed = updateParentLayout;
    flexBasis.onValueChange = updateParentLayout;
    flexBasis.onTransitionProgressed = updateParentLayout;
    alignSelf.onValueChange = updateParentLayout;
}

FlexItem::~FlexItem() = default;

void FlexItem::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    ContainerItem::Child::valueTreePropertyChanged(tree, property);
    forwardPropertyChange(tree, property, order, flexGrow, flexShrink, flexBasis, alignSelf);
}

juce::FlexItem FlexItem::toJuceFlexItem(juce::Rectangle<float> parentContentBounds, LayoutStrategy strategy) {
    const auto direction = parentDirection.getOr(juce::FlexBox::Direction::column);
    const auto orientation
        = direction == juce::FlexBox::Direction::row || direction == juce::FlexBox::Direction::rowReverse
        ? Orientation::horizontal
        : Orientation::vertical;

    // 方向与盒模型约束构成缓存失效键：任一变化都会改变 applyConstraints 的结果。
    if (box.getConstraintsVersion() != cachedItemsConstraintsVersion || orientation != cachedItemsOrientation) {
        cachedItems.clear();
        cachedItemsConstraintsVersion = box.getConstraintsVersion();
        cachedItemsOrientation = orientation;
    }

    const auto key = std::make_pair(parentContentBounds, strategy);

    if (cachedItems.find(key) == std::end(cachedItems)) {
        juce::FlexItem flexItem { *layoutDummy };

        flexItem.flexShrink = flexShrink.calculateCurrent();

        if (strategy == LayoutStrategy::real) {
            flexItem.flexGrow = flexGrow.calculateCurrent();
            flexItem.flexBasis = flexBasis.calculateCurrent();
            flexItem.alignSelf = alignSelf;
        }

        applyConstraints(flexItem, parentContentBounds, orientation, strategy);

        if (cachedItems.size() >= maxNumCachedItems) {
            cachedItems.clear();
        }

        cachedItems[key] = flexItem;
    }

    dynamic_cast<FlexLayoutDummy&>(*layoutDummy).setStrategy(strategy);
    std::fprintf(stderr, "[flex] exists=%d ideal=%f bounds=%fx%f strategy=%d width=%f minWidth=%f\n",
                 static_cast<int>(flexGrow.exists()), flexGrow.calculateCurrent(), parentContentBounds.getWidth(),
                 parentContentBounds.getHeight(), static_cast<int>(strategy), cachedItems.find(key)->second.width,
                 cachedItems.find(key)->second.minWidth);
    return cachedItems.find(key)->second;
}

void FlexItem::idealSizeChanged() {
    cachedItems.clear();
}
} // namespace jive
