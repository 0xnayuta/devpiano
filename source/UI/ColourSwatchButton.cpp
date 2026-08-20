#include "UI/ColourSwatchButton.h"

namespace devpiano::ui {
namespace {

// ============================================================================
// 取色器弹层内容：ColourSelector + OK/Cancel 按钮。
// 仅点击 OK 时才提交颜色；点击外部区域或 Cancel 直接关闭。
// ============================================================================
class ColourChooserContent final : public juce::Component {
public:
    explicit ColourChooserContent(juce::Colour initial)
        : selector(juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders
                   | juce::ColourSelector::showColourspace) {
        selector.setCurrentColour(initial, juce::dontSendNotification);
        addAndMakeVisible(selector);

        okButton = std::make_unique<juce::TextButton>(TRANS("OK"));
        cancelButton = std::make_unique<juce::TextButton>(TRANS("Cancel"));
        addAndMakeVisible(*okButton);
        addAndMakeVisible(*cancelButton);

        okButton->onClick = [this] {
            if (onAccept != nullptr) {
                onAccept(selector.getCurrentColour());
            }
            if (auto box = juce::Component::SafePointer<juce::CallOutBox>(callOutBox)) {
                box->dismiss();
            }
        };
        cancelButton->onClick = [this] {
            if (auto box = juce::Component::SafePointer<juce::CallOutBox>(callOutBox)) {
                box->dismiss();
            }
        };

        setSize(320, 320);
    }

    void resized() override {
        selector.setBounds(4, 4, getWidth() - 8, getHeight() - 44);
        okButton->setBounds(getWidth() - 88, getHeight() - 32, 80, 24);
        cancelButton->setBounds(getWidth() - 176, getHeight() - 32, 80, 24);
    }

    std::function<void(juce::Colour)> onAccept;
    juce::CallOutBox* callOutBox = nullptr;

private:
    juce::ColourSelector selector;
    std::unique_ptr<juce::TextButton> okButton;
    std::unique_ptr<juce::TextButton> cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColourChooserContent)
};

} // namespace

ColourSwatchButton::ColourSwatchButton()
    : juce::Button("ColourSwatch") {
}

void ColourSwatchButton::setSwatchColour(juce::Colour newColour) {
    if (swatchColour != newColour) {
        swatchColour = newColour;
        repaint();
    }
}

void ColourSwatchButton::setSelected(bool shouldBeSelected) {
    if (selected != shouldBeSelected) {
        selected = shouldBeSelected;
        repaint();
    }
}

void ColourSwatchButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float corner = 4.0f;

    // 纯色圆角色块
    g.setColour(swatchColour);
    g.fillRoundedRectangle(bounds, corner);

    // 深色描边增强边界感
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // 悬停高亮描边
    if (highlighted) {
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds, corner, 1.5f);
    }

    // 按下状态轻微压暗
    if (down) {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds, corner);
    }

    // 当前选中色：白色描边环 + 高对比中心圆点
    if (selected) {
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds.reduced(1.0f), corner - 0.5f, 2.0f);

        const auto pipColour = swatchColour.getBrightness() > 0.65f ? juce::Colours::black : juce::Colours::white;
        g.setColour(pipColour);
        g.fillEllipse(bounds.getCentreX() - 2.5f, bounds.getCentreY() - 2.5f, 5.0f, 5.0f);
    }
}

void ColourSwatchButton::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        showColourChooser();
        return;
    }

    juce::Button::mouseDown(e);
}

void ColourSwatchButton::showColourChooser() {
    auto content = std::make_unique<ColourChooserContent>(swatchColour);
    content->setLookAndFeel(&getLookAndFeel());
    auto* contentPtr = content.get();

    contentPtr->onAccept = [safeThis = juce::Component::SafePointer<ColourSwatchButton>(this)](juce::Colour chosen) {
        if (safeThis != nullptr && safeThis->onColourChosen != nullptr) {
            safeThis->onColourChosen(chosen);
        }
    };

    juce::CallOutBox& box = juce::CallOutBox::launchAsynchronously(std::move(content), getScreenBounds(), nullptr);
    contentPtr->callOutBox = &box;
}

} // namespace devpiano::ui
