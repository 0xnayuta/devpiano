#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace devpiano::ui {

// ============================================================================
// 调色盘色块按钮：自行绘制纯色圆角块、悬停/按下反馈与选中环，完全绕开
// JIVE 样式表的 BackgroundCanvas 覆盖绘制问题。
//
// - 左键点击：选中该颜色（选中环由外部同步刷新）
// - 右键点击：弹出 JUCE ColourSelector 取色器，确认后替换自身颜色
// ============================================================================
class ColourSwatchButton final : public juce::Button {
public:
    ColourSwatchButton();

    void setSwatchColour(juce::Colour newColour);
    void setSelected(bool shouldBeSelected);

    // 右键取色器确认新颜色后的回调
    std::function<void(juce::Colour)> onColourChosen;

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void showColourChooser();

    juce::Colour swatchColour { juce::Colours::black };
    bool selected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColourSwatchButton)
};

} // namespace devpiano::ui
