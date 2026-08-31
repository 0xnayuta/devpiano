#pragma once

#include <JuceHeader.h>
#include <jive_layouts/jive_layouts.h>

#include "UI/CustomKeyboard.h"
#include "UI/jive/DesignTokens.h"
#include "UI/native/AdsrCurveComponent.h"
#include "UI/native/StatusBarMidiDot.h"

namespace devpiano::ui::jive {

/**
 * 负责生成 JIVE UI 中使用的常用矢量图标 Drawable。
 */
class VectorIconFactory {
public:
    /// 生成设置齿轮矢量图标
    [[nodiscard]] static std::unique_ptr<juce::Drawable> createGearIcon(juce::Colour colour) {
        juce::Path p;
        p.addEllipse(-8.0f, -8.0f, 16.0f, 16.0f);
        for (int i = 0; i < 4; ++i) {
            auto angle
                = juce::MathConstants<float>::halfPi * static_cast<float>(i) - juce::MathConstants<float>::pi / 4.0f;
            auto cx = 9.0f * std::cos(angle);
            auto cy = 9.0f * std::sin(angle);
            p.addRectangle(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
        }
        p.setUsingNonZeroWinding(true);
        auto drawable = std::make_unique<juce::DrawablePath>();
        drawable->setPath(p);
        drawable->setFill(colour);
        return drawable;
    }

    /// 录音圆点图标
    [[nodiscard]] static std::unique_ptr<juce::Drawable> createRecordIcon() {
        juce::Path p;
        p.addEllipse(-7.0f, -7.0f, 14.0f, 14.0f);
        auto drawable = std::make_unique<juce::DrawablePath>();
        drawable->setPath(p);
        drawable->setFill(juce::Colours::white);
        return drawable;
    }

    /// 播放三角形图标
    [[nodiscard]] static std::unique_ptr<juce::Drawable> createPlayIcon() {
        juce::Path p;
        p.addTriangle(-5.0f, -7.0f, -5.0f, 7.0f, 7.0f, 0.0f);
        auto drawable = std::make_unique<juce::DrawablePath>();
        drawable->setPath(p);
        drawable->setFill(juce::Colours::white);
        return drawable;
    }

    /// 暂停双竖条图标
    [[nodiscard]] static std::unique_ptr<juce::Drawable> createPauseIcon() {
        juce::Path p;
        p.addRectangle(-6.0f, -7.0f, 4.0f, 14.0f);
        p.addRectangle(2.0f, -7.0f, 4.0f, 14.0f);
        auto drawable = std::make_unique<juce::DrawablePath>();
        drawable->setPath(p);
        drawable->setFill(juce::Colours::white);
        return drawable;
    }

    /// 停止正方形图标
    [[nodiscard]] static std::unique_ptr<juce::Drawable> createStopIcon() {
        juce::Path p;
        p.addRectangle(-6.0f, -6.0f, 12.0f, 12.0f);
        auto drawable = std::make_unique<juce::DrawablePath>();
        drawable->setPath(p);
        drawable->setFill(juce::Colours::white);
        return drawable;
    }

    /// 回到开头快退图标（竖条 + 双倒三角）
    [[nodiscard]] static std::unique_ptr<juce::Drawable> createBackIcon() {
        juce::Path p;
        p.addRectangle(-7.0f, -7.0f, 2.5f, 14.0f);
        p.addTriangle(0.0f, -7.0f, 0.0f, 7.0f, -6.0f, 0.0f);
        p.addTriangle(7.0f, -7.0f, 7.0f, 7.0f, 1.0f, 0.0f);
        auto drawable = std::make_unique<juce::DrawablePath>();
        drawable->setPath(p);
        drawable->setFill(juce::Colours::white);
        return drawable;
    }
};

/**
 * 负责向 JIVE Interpreter 注册所有自定义 GUI 组件工厂。
 */
class JiveComponentRegistry {
public:
    static void registerCustomComponents(::jive::Interpreter& interpreter, juce::MidiKeyboardState& keyboardState) {
        auto& factory = interpreter.getComponentFactory();

        factory.set("SettingsButton", [] {
            auto btn = std::make_unique<juce::DrawableButton>("settings", juce::DrawableButton::ImageFitted);
            btn->setImages(VectorIconFactory::createGearIcon(devpiano::jive::DesignTokens::get().textSecondary()).get(),
                           VectorIconFactory::createGearIcon(devpiano::jive::DesignTokens::get().primary()).get(),
                           nullptr);
            return btn;
        });

        factory.set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            editor->setReturnKeyStartsNewLine(false);
            return editor;
        });

        factory.set("ListEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(true);
            editor->setReadOnly(true);
            editor->setScrollbarsShown(true);
            editor->setCaretVisible(false);
            editor->setPopupMenuEnabled(true);
            editor->setWantsKeyboardFocus(false);
            editor->setMouseClickGrabsKeyboardFocus(false);
            return editor;
        });

        factory.set("DevKnob", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 16);
            slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f,
                                        true);
            return slider;
        });

        factory.set("SpeedSlider", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
            return slider;
        });

        factory.set("AdsrCurve", [] { return std::make_unique<AdsrCurveComponent>(); });

        // 图标在应用生命周期内由静态存储所有
        static const auto recordIcon = VectorIconFactory::createRecordIcon();
        static const auto playIcon = VectorIconFactory::createPlayIcon();
        static const auto pauseIcon = VectorIconFactory::createPauseIcon();
        static const auto stopIcon = VectorIconFactory::createStopIcon();
        static const auto backIcon = VectorIconFactory::createBackIcon();

        const auto registerIconButton
            = [&factory](const char* type, const juce::Drawable* image, const juce::String& tooltip,
                         const juce::Drawable* onImage = nullptr) {
                  factory.set(type, [image, tooltip, onImage] {
                      auto btn = std::make_unique<juce::DrawableButton>(tooltip, juce::DrawableButton::ImageFitted);
                      btn->setImages(image, nullptr, nullptr, nullptr, onImage, nullptr, nullptr, nullptr);
                      btn->setEdgeIndent(10);
                      btn->setTooltip(tooltip);
                      return btn;
                  });
              };

        registerIconButton("RecordButton", recordIcon.get(), TRANS("Record"));
        registerIconButton("PlayButton", playIcon.get(), TRANS("Play"), pauseIcon.get());
        registerIconButton("StopButton", stopIcon.get(), TRANS("Stop"));
        registerIconButton("BackButton", backIcon.get(), TRANS("Back to Start"));

        factory.set("CustomKeyboard", [&keyboardState] { return std::make_unique<KeyboardViewport>(keyboardState); });
        factory.set("StatusBarMidiDot", [] { return std::make_unique<StatusBarMidiDot>(); });
    }
};

} // namespace devpiano::ui::jive
