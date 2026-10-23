//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_GuiItemDecorator.h"

#include "jive_LayoutStrategy.h"

namespace std {
template <> struct hash<std::pair<juce::Rectangle<float>, jive::LayoutStrategy>> {
    [[nodiscard]] std::size_t
    operator()(const std::pair<juce::Rectangle<float>, jive::LayoutStrategy>& pair) const noexcept {
        return (pair.first.toString() + jive::toString(pair.second)).hash();
    }
};
} // namespace std

namespace jive {
class ContainerItem : public GuiItemDecorator, protected juce::ValueTree::Listener, private BoxModel::Listener {
public:
    class Child : public GuiItemDecorator, private juce::ComponentListener, protected juce::ValueTree::Listener {
    public:
        explicit Child(std::unique_ptr<GuiItem> itemToDecorate);
        ~Child() override;

    protected:
        using ItemCacheKey = std::pair<juce::Rectangle<float>, LayoutStrategy>;

        void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

        void applyConstraints(
            std::variant<std::reference_wrapper<juce::FlexItem>, std::reference_wrapper<juce::GridItem>> flexOrGridItem,
            juce::Rectangle<float> parentContentBounds, Orientation orientation, LayoutStrategy strategy) const;

    private:
        void componentMovedOrResized(juce::Component&, bool, bool) final;

        class Pimpl;
        const std::unique_ptr<Pimpl> pimpl;

        PassiveProperty<float> idealWidth;
        PassiveProperty<float> idealHeight;
    };

    explicit ContainerItem(std::unique_ptr<GuiItem> itemToDecorate);
    ~ContainerItem() override;

    void insertChild(std::unique_ptr<GuiItem> child, int index) override;
    void setChildren(std::vector<std::unique_ptr<GuiItem>>&& newChildren) override;

    void updateIdealSizeUnrestrained();
    void updateIdealSizeWithinConstraints();

protected:
    virtual juce::Rectangle<float> calculateIdealSize(juce::Rectangle<float> constraints) const = 0;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    void updateIdealSize(juce::Rectangle<float> constraints);

    BoxModel& box;
    PassiveProperty<float> idealWidth;
    PassiveProperty<float> idealHeight;
};
} // namespace jive
