//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_FlexItem.h"

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
    , layoutDummy { std::make_unique<FlexLayoutDummy>(*this) } {
    if (!flexShrink.exists()) {
        flexShrink = juce::FlexItem {}.flexShrink;
    }

    const auto updateParentLayout = [this]() {
        cachedItems.clear();

        if (auto* containerParent
            = dynamic_cast<GuiItemDecorator&>(*getParent()).getTopLevelDecorator().toType<ContainerItem>()) {
            containerParent->updateIdealSizeUnrestrained();
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

    box.addListener(*this);
}

FlexItem::~FlexItem() {
    box.removeListener(*this);
}

juce::FlexItem FlexItem::toJuceFlexItem(juce::Rectangle<float> parentContentBounds, LayoutStrategy strategy) {
    const auto key = std::make_pair(parentContentBounds, strategy);

    if (cachedItems.find(key) == std::end(cachedItems)) {
        juce::FlexItem flexItem { *layoutDummy };

        flexItem.flexShrink = flexShrink.calculateCurrent();

        if (strategy == LayoutStrategy::real) {
            flexItem.flexGrow = flexGrow.calculateCurrent();
            flexItem.flexBasis = flexBasis.calculateCurrent();
            flexItem.alignSelf = alignSelf;
        }

        const auto orientation = [this]() {
            const Property<juce::FlexBox::Direction> parentDirection {
                state.getParent(),
                "flex-direction",
            };
            const auto direction = parentDirection.get();

            if (direction == juce::FlexBox::Direction::row || direction == juce::FlexBox::Direction::rowReverse) {
                return Orientation::horizontal;
            }

            return Orientation::vertical;
        };
        applyConstraints(flexItem, parentContentBounds, orientation(), strategy);

        cachedItems[key] = flexItem;
    }

    dynamic_cast<FlexLayoutDummy&>(*layoutDummy).setStrategy(strategy);
    return cachedItems.find(key)->second;
}

void FlexItem::boxModelChanged(BoxModel&) {
    cachedItems.clear();
}
} // namespace jive
