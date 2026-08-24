#include "DevPianoLookAndFeel.h"

#include "UI/jive/DesignTokens.h"
namespace {
const auto& tokens = devpiano::jive::DesignTokens::get();

juce::Font getUnifiedUiFont(float height = 14.0f, int styleFlags = juce::Font::plain) {
    // JUCE 在 Linux 上对字体族名做 FreeType 精确匹配（不经 fontconfig 别名），
    // 因此 Linux 首选必须是在系统真实安装的字体，否则整条链失效并触发
    // fontconfig 字符级 fallback（中文会落到 wqy-zenhei 等低清字体）。
#if JUCE_LINUX
    return juce::Font(juce::FontOptions("Noto Sans CJK SC", height, styleFlags)
                          .withFallbacks({
                              // Linux 主流高清晰度 CJK 黑体（覆盖中文与西文字形）
                              "Source Han Sans SC",
                              "Source Han Sans CN",
                              "Noto Sans SC",
                              "WenQuanYi Micro Hei",
                              "WenQuanYi Zen Hei",
                              // Linux / 跨平台现代西文 UI 字体
                              "Ubuntu",
                              "Cantarell",
                              "Inter",
                              "Liberation Sans",
                              "DejaVu Sans",
                              // 通用后备
                              "sans-serif",
                          }));
#else
    return juce::Font(juce::FontOptions("Microsoft YaHei UI", height, styleFlags)
                          .withFallbacks({
                              // Windows UI & CJK (彻底移除 SimSun，避免 Linux 下误命中宋体)
                              "Microsoft YaHei",
                              "Segoe UI",
                              // macOS UI & CJK
                              "PingFang SC",
                              "Hiragino Sans GB",
                              // Linux 主流高清晰度中文字体 (Ubuntu / Debian / Fedora / Arch / CachyOS / Deepin 等)
                              "Noto Sans CJK SC",
                              "Source Han Sans SC",
                              "Source Han Sans CN",
                              "Noto Sans SC",
                              "WenQuanYi Micro Hei",
                              "WenQuanYi Zen Hei",
                              // Linux / 跨平台现代西文 UI 字体
                              "Ubuntu",
                              "Cantarell",
                              "Inter",
                              "Liberation Sans",
                              "DejaVu Sans",
                              // 通用后备
                              "sans-serif",
                          }));
#endif
}
} // namespace

DevPianoLookAndFeel::DevPianoLookAndFeel() {
    refreshColours();
}

void DevPianoLookAndFeel::refreshColours() {
    setColourScheme(ColourScheme {
        tokens.mainBg(), // windowBackground
        tokens.controlBg(), // widgetBackground
        tokens.panelBg(), // menuBackground
        tokens.textSecondary(), // outline
        tokens.textPrimary(), // defaultText
        tokens.primary(), // defaultFill
        tokens.textPrimary(), // highlightedText
        tokens.primary(), // highlightedFill
        tokens.textPrimary(), // menuText
    });

    // ── Window ──
    setColour(juce::ResizableWindow::backgroundColourId, tokens.mainBg());

    // ── Slider ──
    setColour(juce::Slider::thumbColourId, tokens.primary());
    setColour(juce::Slider::trackColourId, tokens.controlBg());
    setColour(juce::Slider::backgroundColourId, tokens.textDisabled());
    setColour(juce::Slider::textBoxTextColourId, tokens.textPrimary());
    setColour(juce::Slider::textBoxBackgroundColourId, tokens.panelBg());
    setColour(juce::Slider::textBoxOutlineColourId, tokens.textSecondary());

    // ── TextButton ──
    setColour(juce::TextButton::buttonColourId, tokens.controlBg());
    setColour(juce::TextButton::buttonOnColourId, tokens.primary());
    setColour(juce::TextButton::textColourOffId, tokens.textPrimary());
    setColour(juce::TextButton::textColourOnId, tokens.textPrimary());

    // ── ComboBox ──
    setColour(juce::ComboBox::backgroundColourId, tokens.controlBg());
    setColour(juce::ComboBox::textColourId, tokens.textPrimary());
    setColour(juce::ComboBox::outlineColourId, tokens.cardBorder());
    setColour(juce::ComboBox::arrowColourId, tokens.textPrimary());
    setColour(juce::ComboBox::buttonColourId, tokens.primary());
    setColour(juce::ComboBox::focusedOutlineColourId, tokens.primary());

    // ── PopupMenu ──
    setColour(juce::PopupMenu::backgroundColourId, tokens.panelBg());
    setColour(juce::PopupMenu::textColourId, tokens.textPrimary());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, tokens.primary());
    setColour(juce::PopupMenu::highlightedTextColourId, tokens.textPrimary());

    // ── TextEditor ──
    setColour(juce::TextEditor::backgroundColourId, tokens.controlBg());
    setColour(juce::TextEditor::textColourId, tokens.textPrimary());
    setColour(juce::TextEditor::outlineColourId, tokens.textSecondary());
    setColour(juce::TextEditor::focusedOutlineColourId, tokens.primary());
    setColour(juce::TextEditor::highlightColourId, tokens.primaryAlpha30());
    setColour(juce::TextEditor::highlightedTextColourId, tokens.textPrimary());

    // ── AlertWindow ──
    setColour(juce::AlertWindow::backgroundColourId, tokens.mainBg());
    setColour(juce::AlertWindow::textColourId, tokens.textPrimary());
    setColour(juce::AlertWindow::outlineColourId, tokens.textSecondary());
    // ── ProgressBar ──
    setColour(juce::ProgressBar::backgroundColourId, tokens.controlBg());
    setColour(juce::ProgressBar::foregroundColourId, tokens.primary());
    // ── Label ──
    setColour(juce::Label::textColourId, tokens.textPrimary());
    setColour(juce::Label::textWhenEditingColourId, tokens.textPrimary());

    // ── ToggleButton ──
    setColour(juce::ToggleButton::textColourId, tokens.textPrimary());
    setColour(juce::ToggleButton::tickColourId, tokens.primary());
    setColour(juce::ToggleButton::tickDisabledColourId, tokens.textDisabled());

    // ── GroupComponent ──
    setColour(juce::GroupComponent::outlineColourId, tokens.textSecondary());
    setColour(juce::GroupComponent::textColourId, tokens.textPrimary());

    // ── ListBox ──
    setColour(juce::ListBox::backgroundColourId, tokens.panelBg());
    setColour(juce::ListBox::textColourId, tokens.textPrimary());

    // ── ScrollBar ──
    setColour(juce::ScrollBar::thumbColourId, tokens.textSecondary());

    // ── Caret ──
    setColour(juce::CaretComponent::caretColourId, tokens.primary());
}

// ============================================================================
//  drawButtonBackground
// ============================================================================
void DevPianoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& bg,
                                               bool highlighted, bool down) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    constexpr float corner = 5.0f;

    // Base fill with subtle top-to-bottom gradient for slight convexity
    juce::ColourGradient grad(bg.brighter(0.09f), bounds.getX(), bounds.getY(), bg.darker(0.06f), bounds.getX(),
                              bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, corner);

    // Disabled state: flat, faded, low-contrast. A latched accent (e.g. the
    // record button while recording) is kept but faded so the active state
    // stays visible even though the button cannot be clicked.
    if (!button.isEnabled()) {
        if (button.getToggleState()) {
            g.setColour(bg.withAlpha(0.30f));
            g.fillRoundedRectangle(bounds, corner);
            g.setColour(bg.withAlpha(0.55f));
            g.drawRoundedRectangle(bounds, corner, 1.2f);
        } else {
            g.setColour(bg.darker(0.12f).withAlpha(0.5f));
            g.fillRoundedRectangle(bounds, corner);
            g.setColour(juce::Colour(0xFF262B34));
            g.drawRoundedRectangle(bounds, corner, 1.0f);
        }
        return;
    }

    // Latched state (e.g. recording/playing): inner colour glow
    if (button.getToggleState()) {
        g.setColour(bg.withAlpha(0.22f));
        g.fillRoundedRectangle(bounds, corner);
    }

    // Top 1px bevel highlight for physical tactile feel
    if (!down) {
        g.setColour(juce::Colour(0x28FFFFFF));
        g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1.0f), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
    }

    // Border: latched gets a bright accent, hover gets the primary tint
    juce::Colour borderCol;
    if (button.getToggleState()) {
        borderCol = bg.brighter(0.55f);
    } else if (highlighted) {
        borderCol = tokens.primary().withAlpha(0.6f);
    } else {
        borderCol = bg.brighter(0.18f);
    }
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, corner, button.getToggleState() ? 1.6f : 1.0f);

    // Highlight overlay
    if (highlighted && !down) {
        g.setColour(tokens.highlightOverlay());
        g.fillRoundedRectangle(bounds, corner);
    }

    // Pressed state — sink effect with darken
    if (down) {
        g.setColour(tokens.pressOverlay());
        g.fillRoundedRectangle(bounds, corner);
    }
}

// ============================================================================
//  drawToggleButton
// ============================================================================
void DevPianoLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) {
    juce::ignoreUnused(down);
    constexpr float fontSize = 13.0f;
    constexpr float tickWidth = 14.0f;
    const auto buttonHeight = static_cast<float>(button.getHeight());
    const float tickY = (buttonHeight - tickWidth) * 0.5f;

    juce::Rectangle<float> tickBounds(2.0f, tickY, tickWidth, tickWidth);

    // Box outline & background
    g.setColour(tokens.controlBg());
    g.fillRoundedRectangle(tickBounds, 3.0f);

    juce::Colour borderCol;
    if (!button.isEnabled()) {
        borderCol = tokens.textDisabled().withAlpha(0.4f);
    } else if (button.getToggleState()) {
        borderCol = tokens.primary();
    } else if (highlighted) {
        borderCol = tokens.textPrimary().withAlpha(0.6f);
    } else {
        borderCol = tokens.textSecondary().withAlpha(0.6f);
    }
    g.setColour(borderCol);
    g.drawRoundedRectangle(tickBounds, 3.0f, 1.0f);

    // Tick mark
    if (button.getToggleState()) {
        g.setColour(button.isEnabled() ? tokens.primary() : tokens.textDisabled());
        auto tick = getTickShape(0.75f);
        g.fillPath(tick, tick.getTransformToScaleToFit(tickBounds.reduced(3.0f, 3.0f), false));
    }

    // Text label
    if (button.getButtonText().isNotEmpty()) {
        g.setColour(button.isEnabled() ? button.findColour(juce::ToggleButton::textColourId) : tokens.textDisabled());
        g.setFont(getUnifiedUiFont(fontSize));

        const auto textBounds
            = button.getLocalBounds().withTrimmedLeft(juce::roundToInt(tickWidth + 6.0f)).withTrimmedRight(2);
        g.drawFittedText(button.getButtonText(), textBounds, juce::Justification::centredLeft, 1);
    }
}

// ============================================================================
//  drawDrawableButton
// ============================================================================
void DevPianoLookAndFeel::drawDrawableButton(juce::Graphics& g, juce::DrawableButton& /*button*/, bool /*highlighted*/,
                                             bool /*down*/) {
    // ImageFitted 图标按钮（transport 四键、settings 齿轮）的背景与边框由
    // JIVE BackgroundCanvas 绘制（圆角、含伪状态配色）。必须覆盖 V2 默认
    // 实现：否则 toggle 状态下 fillAll(backgroundOnColourId) 会在按钮圆角
    // 外露出一个直角底色（LAF ColourScheme highlightedFill = primary 蓝，
    // 播放/录制中 setLatched 置 toggle 时可见）。
    g.fillAll(juce::Colours::transparentBlack);
}

// ============================================================================
//  drawComboBox
// ============================================================================
void DevPianoLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /* isButtonDown */, int buttonX,
                                       int buttonY, int buttonW, int buttonH, juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(0.5f);
    constexpr float corner = 5.0f;

    // Background with soft top-down gradient
    const auto bg = box.findColour(juce::ComboBox::backgroundColourId);
    juce::ColourGradient grad(bg.brighter(0.04f), bounds.getX(), bounds.getY(), bg.darker(0.04f), bounds.getX(),
                              bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, corner);

    // Outline
    juce::Colour outlineColour;
    if (box.isEnabled()) {
        outlineColour = box.hasKeyboardFocus(true) ? box.findColour(juce::ComboBox::focusedOutlineColourId)
                                                   : box.findColour(juce::ComboBox::outlineColourId);
    } else {
        outlineColour = box.findColour(juce::ComboBox::outlineColourId).withAlpha(0.3f);
    }
    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // Drop-down chevron arrow
    juce::Path arrow;
    const float cx = (float)buttonX + (float)buttonW * 0.5f;
    const float cy = (float)buttonY + (float)buttonH * 0.5f;
    const float a = 3.5f;
    arrow.startNewSubPath(cx - a, cy - a * 0.5f);
    arrow.lineTo(cx, cy + a * 0.5f);
    arrow.lineTo(cx + a, cy - a * 0.5f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ============================================================================
//  drawPopupMenuItem
// ============================================================================
void DevPianoLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool sep, bool active,
                                            bool highlighted, bool ticked, bool submenu, const juce::String& text,
                                            const juce::String& shortcut, const juce::Drawable* icon,
                                            const juce::Colour* /* textColour */) {
    if (sep) {
        g.setColour(tokens.textSecondary().withAlpha(0.2f));
        g.fillRect(area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
        return;
    }

    // Highlight background
    if (highlighted && active) {
        g.setColour(tokens.primary().withAlpha(0.22f));
        g.fillRoundedRectangle(area.toFloat().reduced(2.0f, 1.0f), 4.0f);
        g.setColour(tokens.primary().withAlpha(0.6f));
        g.drawRoundedRectangle(area.toFloat().reduced(2.0f, 1.0f), 4.0f, 1.0f);
    }

    // Ticked item — draw check mark on the right side
    if (ticked) {
        g.setColour(highlighted ? tokens.textPrimary() : tokens.primary());
        const auto tick = getTickShape(4.0f);
        const auto tickArea
            = area.withTrimmedLeft(area.getWidth() - 24).reduced(4, (area.getHeight() - 14) / 2).toFloat();
        g.fillPath(tick, tick.getTransformToScaleToFit(tickArea, true));
    }
    juce::Colour textColour;
    if (highlighted && active) {
        textColour = tokens.textPrimary();
    } else {
        textColour = active ? tokens.textPrimary() : tokens.textDisabled();
    }
    g.setColour(textColour);
    g.setFont(juce::FontOptions(13.0f));
    const int iconW = icon != nullptr ? area.getHeight() : 0;
    const int rightTrim = (ticked ? 24 : 0) + (submenu ? 16 : 4);
    const auto textBounds = area.reduced(iconW > 0 ? 0 : 8, 0).withTrimmedLeft(iconW).withTrimmedRight(rightTrim);

    if (icon != nullptr) {
        auto iconArea = area.withWidth(area.getHeight()).reduced(4, 2).toFloat();
        icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
    }

    if (shortcut.isNotEmpty()) {
        g.setColour(tokens.textSecondary());
        g.drawText(shortcut, textBounds, juce::Justification::centredRight);
    }

    g.setColour(textColour);
    g.drawText(text, textBounds, juce::Justification::centredLeft);
    if (submenu) {
        juce::Path arrow;
        const auto cx = static_cast<float>(area.getRight()) - 8.0f;
        const auto cy = static_cast<float>(area.getCentreY());
        arrow.addTriangle(cx - 3.0f, cy - 4.0f, cx - 3.0f, cy + 4.0f, cx + 1.0f, cy);
        g.setColour(textColour);
        g.fillPath(arrow);
    }
}

// ============================================================================
//  drawLinearSlider
// ============================================================================
void DevPianoLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos,
                                           float maxPos, juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (!slider.isHorizontal()) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, slider);
        return;
    }

    constexpr float trackThickness = 4.0f;
    const auto trackY = static_cast<float>(y) + static_cast<float>(h) * 0.42f - trackThickness * 0.5f;
    const auto trackW = static_cast<float>(w);

    // Background recessed groove
    g.setColour(juce::Colour(0xFF14161A));
    g.fillRoundedRectangle((float)x, trackY, trackW, trackThickness, 2.0f);
    g.setColour(juce::Colour(0xFF282C35));
    g.drawRoundedRectangle((float)x, trackY, trackW, trackThickness, 2.0f, 1.0f);

    // Glowing active filled track
    const float fillW = pos - (float)x;
    if (fillW > 0.0f) {
        const auto primary = tokens.primary();
        // Soft glow bloom behind fill
        g.setColour(primary.withAlpha(0.35f));
        g.fillRoundedRectangle((float)x, trackY - 1.0f, fillW, trackThickness + 2.0f, 2.0f);

        // Solid core fill
        juce::ColourGradient fillGrad(primary.brighter(0.1f), (float)x, trackY, primary, (float)x + fillW, trackY,
                                      false);
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle((float)x, trackY, fillW, trackThickness, 2.0f);
    }

    // Subtle scale ticks below track
    const float tickY = trackY + trackThickness + 3.0f;
    constexpr int numTicks = 7;
    g.setColour(juce::Colour(0xFF383D47));
    for (int i = 0; i < numTicks; ++i) {
        const float tx = (float)x + ((float)i / (float)(numTicks - 1)) * trackW;
        const float th = (i == 0 || i == numTicks - 1 || i == (numTicks - 1) / 2) ? 4.0f : 2.5f;
        g.drawVerticalLine(static_cast<int>(tx), tickY, tickY + th);
    }

    // Value labels below the track (playback-speed slider: 0.5x–2.0x).
    // Four labels at 0.5 steps: the old 7-label set (0.75/1.25/1.75) put
    // adjacent labels only 1/6 of the track apart (~33px at 200px, less at
    // low resolution) while each label is 30px wide, so they overlapped.
    if (juce::approximatelyEqual(slider.getMinimum(), 0.5) && juce::approximatelyEqual(slider.getMaximum(), 2.0)) {
        const juce::StringArray labels { "0.5", "1.0", "1.5", "2.0" };
        g.setFont(juce::FontOptions(9.0f));
        g.setColour(juce::Colour(0xFF707888));
        // Extra clearance below the ticks so the 8x16 thumb never overlaps
        // the value labels.
        const float labelY = tickY + 6.0f;
        const auto span = static_cast<float>(slider.getMaximum() - slider.getMinimum());
        for (int i = 0; i < labels.size(); ++i) {
            const auto& label = labels[i];
            const auto posX
                = (float)x + (label.getFloatValue() - static_cast<float>(slider.getMinimum())) / span * trackW;
            if (i == 0) {
                // First label sits at the track start; draw left-aligned so it
                // stays fully inside the track instead of clamping its centre
                // onto the next label.
                g.drawText(label, juce::Rectangle<float>(posX, labelY, 30.0f, 9.0f), juce::Justification::left, false);
            } else if (i == labels.size() - 1) {
                // Last label sits at the track end; draw right-aligned.
                g.drawText(label, juce::Rectangle<float>(posX - 30.0f, labelY, 30.0f, 9.0f), juce::Justification::right,
                           false);
            } else {
                g.drawText(label, juce::Rectangle<float>(posX - 15.0f, labelY, 30.0f, 9.0f),
                           juce::Justification::centred, false);
            }
        }
    }

    // Metallic Thumb — 8 x 16 rounded block with center groove
    constexpr float thumbW = 8.0f;
    constexpr float thumbH = 16.0f;
    const float thumbX = juce::jlimit((float)x, (float)x + (float)w - thumbW, pos - thumbW * 0.5f);
    const float thumbY = (float)y + (float)h * 0.42f - thumbH * 0.5f;
    const auto thumbBounds = juce::Rectangle<float>(thumbX, thumbY, thumbW, thumbH);

    // Thumb drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(thumbBounds.translated(0.0f, 1.5f), 2.0f);

    // Metallic gradient body
    juce::ColourGradient thumbGrad(juce::Colour(0xFF5A606D), thumbX, thumbY, juce::Colour(0xFF22252B), thumbX,
                                   thumbY + thumbH, false);
    g.setGradientFill(thumbGrad);
    g.fillRoundedRectangle(thumbBounds, 2.0f);

    // Thumb border
    g.setColour(slider.isMouseOverOrDragging() ? tokens.primary() : juce::Colour(0xFF707888));
    g.drawRoundedRectangle(thumbBounds, 2.0f, 1.0f);

    // Thumb center grip notch
    g.setColour(slider.isMouseOverOrDragging() ? tokens.primary() : juce::Colour(0xFF14161A));
    g.drawVerticalLine(static_cast<int>(thumbX + thumbW * 0.5f), thumbY + 3.0f, thumbY + thumbH - 3.0f);
}

// ============================================================================
//  drawRotarySlider
// ============================================================================
void DevPianoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float startAng,
                                           float endAng, juce::Slider& /*slider*/) {
    constexpr float arcThickness = 3.2f;
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                                               static_cast<float>(h))
                            .reduced(2.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - arcThickness;
    const auto centre = bounds.getCentre();

    // 1. Outer Track & Background Depression
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAng, endAng, true);
    g.setColour(tokens.rotaryBgTrack());
    g.strokePath(bgArc,
                 juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float filledAngle = startAng + pos * (endAng - startAng);
    if (pos > 0.001f) {
        // Soft glowing halo behind active track
        juce::Path glowArc;
        glowArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAng, filledAngle, true);
        g.setColour(tokens.primary().withAlpha(0.32f * pos));
        g.strokePath(
            glowArc,
            juce::PathStrokeType(arcThickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Sharp neon cyan active track
        juce::Path fgArc;
        fgArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAng, filledAngle, true);
        g.setColour(tokens.primary());
        g.strokePath(fgArc,
                     juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 2. Metallic 3D Cap Body
    const float capRadius = radius * 0.74f;
    const auto capBounds
        = juce::Rectangle<float>(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    juce::ColourGradient capGrad(tokens.rotaryCapTop(), capBounds.getX(), capBounds.getY(), tokens.rotaryCapBottom(),
                                 capBounds.getRight(), capBounds.getBottom(), false);
    g.setGradientFill(capGrad);
    g.fillEllipse(capBounds);

    // Dark outer bevel rim
    g.setColour(tokens.rotaryCapRim());
    g.drawEllipse(capBounds, 1.0f);

    // Specular highlight inner ring (light reflection from top-left)
    const auto innerCapBounds = capBounds.reduced(1.0f);
    juce::ColourGradient ringGrad(tokens.rotaryRingTop(), innerCapBounds.getX(), innerCapBounds.getY(),
                                  tokens.rotaryRingBottom(), innerCapBounds.getRight(), innerCapBounds.getBottom(),
                                  false);
    g.setGradientFill(ringGrad);
    g.drawEllipse(innerCapBounds, 1.0f);

    // 3. Indicator Needle: sharp glowing line
    const float needleLen = capRadius * 0.85f;
    const float needleX = centre.x + needleLen * std::sin(filledAngle);
    const float needleY = centre.y - needleLen * std::cos(filledAngle);

    juce::Line<float> needleLine(centre.x, centre.y, needleX, needleY);
    // Glow line
    if (pos > 0.01f) {
        g.setColour(tokens.primary().withAlpha(0.4f));
        g.drawLine(needleLine, 3.5f);
    }
    // Core line
    juce::ColourGradient needleGrad(pos > 0.01f ? tokens.primary() : juce::Colour(0xFF888E99), centre.x, centre.y,
                                    pos > 0.01f ? juce::Colours::white : juce::Colour(0xFFB0B6C0), needleX, needleY,
                                    false);
    g.setGradientFill(needleGrad);
    g.drawLine(needleLine, 2.0f);
}
// ============================================================================
//  fillTextEditorBackground
// ============================================================================
void DevPianoLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor& editor) {
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)w, (float)h, 2.0f);
}

// ============================================================================
//  drawTextEditorOutline
// ============================================================================
void DevPianoLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& editor) {
    if (editor.isEnabled()) {
        const auto colour = editor.hasKeyboardFocus(true) ? editor.findColour(juce::TextEditor::focusedOutlineColourId)
                                                          : editor.findColour(juce::TextEditor::outlineColourId);
        g.setColour(colour);
        g.drawRoundedRectangle(0.5f, 0.5f, (float)w - 1.0f, (float)h - 1.0f, 2.0f, 1.0f);
    }
}

// ============================================================================
//  drawLabel
// ============================================================================
void DevPianoLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label) {
    g.setColour(label.findColour(juce::Label::textColourId));
    const auto font = label.getFont();
    g.setFont(font);

    const auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                     juce::jmax(1, (int)((float)textArea.getHeight() / font.getHeight())),
                     label.getMinimumHorizontalScale());
}

// ============================================================================
//  getLabelFont
// ============================================================================
juce::Font DevPianoLookAndFeel::getLabelFont(juce::Label& /*label*/) {
    return getUnifiedUiFont(14.0f);
}

juce::Font DevPianoLookAndFeel::getComboBoxFont(juce::ComboBox& /*box*/) {
    return getUnifiedUiFont(14.0f);
}

juce::Font DevPianoLookAndFeel::getPopupMenuFont() {
    return getUnifiedUiFont(14.0f);
}
// ============================================================================
//  drawTooltip
// ============================================================================
void DevPianoLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    constexpr float corner = 4.0f;
    // 深色圆角背景：TooltipWindow 透明，若不填充背景，文字会直接叠在
    // 下方 UI 内容上（深色主题下不可读）。
    g.setColour(tokens.panelBg());
    g.fillRoundedRectangle(bounds, corner);
    // Micro ice-blue border
    g.setColour(tokens.primary().withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // Matte gray text
    g.setColour(tokens.textSecondary());
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(text, bounds.reduced(6.0f, 2.0f), juce::Justification::centred, true);
}

juce::Rectangle<int> DevPianoLookAndFeel::getTooltipBounds(const juce::String& tip, juce::Point<int> screenPos,
                                                           juce::Rectangle<int> parentArea) {
    const auto font = juce::Font(juce::FontOptions(11.0f));
    const auto textW = juce::jmin(juce::GlyphArrangement::getStringWidthInt(font, tip) + 14, parentArea.getWidth());
    const auto textH = juce::jmin(22, parentArea.getHeight()); // single-line height + padding

    return juce::Rectangle<int>(textW, textH)
        .withPosition(juce::jmin(screenPos.x, parentArea.getRight() - textW),
                      juce::jmin(screenPos.y, parentArea.getBottom() - textH));
}

// ============================================================================
//  drawProgressBar
// ============================================================================
void DevPianoLookAndFeel::drawProgressBar(juce::Graphics& g, juce::ProgressBar& progressBar, int width, int height,
                                          double progress, const juce::String& textToShow) {
    juce::ignoreUnused(width, height);
    const auto bounds = progressBar.getLocalBounds().toFloat();
    constexpr float corner = 4.0f;

    g.setColour(tokens.controlBg());
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(tokens.textSecondary());
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    if (progress > 0.0) {
        const auto fillWidth = bounds.getWidth() * static_cast<float>(juce::jlimit(0.0, 1.0, progress));
        auto fillBounds = bounds.withWidth(fillWidth);

        juce::Path p;
        p.addRoundedRectangle(bounds, corner);
        g.reduceClipRegion(p);

        g.setColour(tokens.primary());
        g.fillRoundedRectangle(fillBounds, corner);
    }

    if (textToShow.isNotEmpty()) {
        g.setColour(tokens.textPrimary());
        g.setFont(juce::FontOptions(15.0f));
        g.drawText(textToShow, bounds.reduced(4.0f, 0.0f), juce::Justification::centred, true);
    }
}

// ============================================================================
//  drawAlertBox
// ============================================================================
void DevPianoLookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert,
                                       const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout) {
    const auto bounds = alert.getLocalBounds().toFloat();
    constexpr float corner = 8.0f;

    g.setColour(tokens.mainBg());
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(tokens.textSecondary());
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    textLayout.draw(g, textArea.toFloat());
}

// ============================================================================
//  AlertWindow fonts and dimensions
// ============================================================================
juce::Font DevPianoLookAndFeel::getAlertWindowTitleFont() {
    return getUnifiedUiFont(16.0f, juce::Font::bold);
}

juce::Font DevPianoLookAndFeel::getAlertWindowMessageFont() {
    return getUnifiedUiFont(14.0f);
}

juce::Font DevPianoLookAndFeel::getAlertWindowFont() {
    return getUnifiedUiFont(14.0f);
}

int DevPianoLookAndFeel::getAlertWindowButtonHeight() {
    return 28;
}

juce::Array<int> DevPianoLookAndFeel::getWidthsForTextButtons(juce::AlertWindow& alert,
                                                              const juce::Array<juce::TextButton*>& buttons) {
    juce::ignoreUnused(alert);
    juce::Array<int> widths;
    for (int i = 0; i < buttons.size(); ++i) {
        const int minW = 80;
        const int fittedW = getTextButtonWidthToFitText(*buttons.getReference(i), 28);
        widths.add(juce::jmax(minW, fittedW));
    }
    return widths;
}

juce::Font DevPianoLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight) {
    juce::ignoreUnused(buttonHeight);
    return getUnifiedUiFont(14.0f);
}
