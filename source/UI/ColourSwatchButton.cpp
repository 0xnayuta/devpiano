#include "UI/ColourSwatchButton.h"
#include "UI/jive/DesignTokens.h"
#include <juce_gui_extra/juce_gui_extra.h>

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

        const auto& tokens = devpiano::jive::DesignTokens::get();

        okButton = std::make_unique<juce::TextButton>(TRANS("OK"));
        cancelButton = std::make_unique<juce::TextButton>(TRANS("Cancel"));

        // 对齐 JIVE 视觉规范：OK 主动作高亮强调色，Cancel 次动作暗色
        okButton->setColour(juce::TextButton::buttonColourId, tokens.primary());
        okButton->setColour(juce::TextButton::textColourOffId, tokens.mainBg());
        okButton->setColour(juce::TextButton::textColourOnId, tokens.mainBg());

        cancelButton->setColour(juce::TextButton::buttonColourId, tokens.controlBg());
        cancelButton->setColour(juce::TextButton::textColourOffId, tokens.textPrimary());
        cancelButton->setColour(juce::TextButton::textColourOnId, tokens.textPrimary());

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

        // 容器尺寸：宽 320, 高 417
        // 内部 ColourSelector 300x359 精确使得取色空间大矩形呈现 1:1 严格正方形 (233x233)
        setSize(320, 417);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().panelBg());
    }

    void resized() override {
        constexpr int pad = 10;
        constexpr int btnW = 80;
        constexpr int btnH = 28;
        constexpr int btnGap = 8;
        constexpr int selectorW = 300;
        constexpr int selectorH = 359;

        selector.setBounds(pad, pad, selectorW, selectorH);
        okButton->setBounds(getWidth() - pad - btnW - btnGap - btnW, getHeight() - pad - btnH, btnW, btnH);
        cancelButton->setBounds(getWidth() - pad - btnW, getHeight() - pad - btnH, btnW, btnH);
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
