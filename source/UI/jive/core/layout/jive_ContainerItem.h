//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "UI/jive/core/layout/jive_GuiItemDecorator.h"

#include "UI/jive/core/layout/jive_LayoutStrategy.h"

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

        // 子项的自然尺寸可能在不改变 state 的情况下变化（例如文本重排）。
        virtual void idealSizeChanged() {
        }

    protected:
        using ItemCacheKey = std::pair<juce::Rectangle<float>, LayoutStrategy>;
        static constexpr std::size_t maxNumCachedItems = 8;

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

    void updateIdealSize(bool informParentOfChanges = true, bool measureChildren = true);
    void invalidateIdealSize();
    void refreshLayout();

    /** 按当前组件内容区测高（fitToContent 使用，宽度由调用方先写入组件）。 */
    void updateIdealSizeWithinContentBounds();

protected:
    virtual juce::Rectangle<float> calculateIdealSize(juce::Rectangle<float> constraints) const = 0;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    void boxModelChanged(BoxModel&) final;
    void updateIdealSizeIfOutOfDate();
    void updateChildrensIdealSizesIfOutOfDate();
    void applyIdealSize(juce::Rectangle<float> newIdealSize, juce::Rectangle<float> constraints, bool informParent);
    [[nodiscard]] juce::Rectangle<float> getContentConstraints() const;
    [[nodiscard]] bool isSetupInProgress() const;

    BoxModel& box;
    PassiveProperty<float> idealWidth;
    PassiveProperty<float> idealHeight;
    juce::Rectangle<float> explicitConstraintsWhenIdealSizeWasMeasured;
    juce::Rectangle<float> constraintsWhenIdealSizeWasMeasured;
    bool idealSizeWasLimitedByConstraints = false;
    bool idealSizeIsOutOfDate = true;
    bool childrenMayBeOutOfDate = true;
};
} // namespace jive
