#include "DevPianoLookAndFeel.h"

DevPianoLookAndFeel::DevPianoLookAndFeel()
    : LookAndFeel_V4(ColourScheme {
          kMainBg, // windowBackground
          kControlBg, // widgetBackground
          kPanelBg, // menuBackground
          kTextSecondary, // outline
          kTextPrimary, // defaultText
          kPrimary, // defaultFill
          kTextPrimary, // highlightedText
          kPrimary, // highlightedFill
          kTextPrimary, // menuText
      }) {
    // ── Window ──
    setColour(juce::ResizableWindow::backgroundColourId, kMainBg);

    // ── Slider ──
    setColour(juce::Slider::thumbColourId, kPrimary);
    setColour(juce::Slider::trackColourId, kControlBg);
    setColour(juce::Slider::backgroundColourId, kTextDisabled);
    setColour(juce::Slider::textBoxTextColourId, kTextPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, kPanelBg);
    setColour(juce::Slider::textBoxOutlineColourId, kTextSecondary);

    // ── TextButton ──
    setColour(juce::TextButton::buttonColourId, kControlBg);
    setColour(juce::TextButton::buttonOnColourId, kPrimary);
    setColour(juce::TextButton::textColourOffId, kTextPrimary);
    setColour(juce::TextButton::textColourOnId, kTextPrimary);

    // ── ComboBox ──
    setColour(juce::ComboBox::backgroundColourId, kControlBg);
    setColour(juce::ComboBox::textColourId, kTextPrimary);
    setColour(juce::ComboBox::outlineColourId, kTextSecondary);
    setColour(juce::ComboBox::arrowColourId, kTextPrimary);
    setColour(juce::ComboBox::buttonColourId, kPrimary);
    setColour(juce::ComboBox::focusedOutlineColourId, kPrimary);

    // ── PopupMenu ──
    setColour(juce::PopupMenu::backgroundColourId, kPanelBg);
    setColour(juce::PopupMenu::textColourId, kTextPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kPrimary);
    setColour(juce::PopupMenu::highlightedTextColourId, kTextPrimary);

    // ── TextEditor ──
    setColour(juce::TextEditor::backgroundColourId, kControlBg);
    setColour(juce::TextEditor::textColourId, kTextPrimary);
    setColour(juce::TextEditor::outlineColourId, kTextSecondary);
    setColour(juce::TextEditor::focusedOutlineColourId, kPrimary);
    setColour(juce::TextEditor::highlightColourId, kPrimary.withAlpha(0.3f));
    setColour(juce::TextEditor::highlightedTextColourId, kTextPrimary);

    // ── Label ──
    setColour(juce::Label::textColourId, kTextPrimary);
    setColour(juce::Label::textWhenEditingColourId, kTextPrimary);

    // ── ToggleButton ──
    setColour(juce::ToggleButton::textColourId, kTextPrimary);
    setColour(juce::ToggleButton::tickColourId, kPrimary);
    setColour(juce::ToggleButton::tickDisabledColourId, kTextDisabled);

    // ── GroupComponent ──
    setColour(juce::GroupComponent::outlineColourId, kTextSecondary);
    setColour(juce::GroupComponent::textColourId, kTextPrimary);

    // ── ListBox ──
    setColour(juce::ListBox::backgroundColourId, kPanelBg);
    setColour(juce::ListBox::textColourId, kTextPrimary);

    // ── ScrollBar ──
    setColour(juce::ScrollBar::thumbColourId, kTextSecondary);

    // ── Caret ──
    setColour(juce::CaretComponent::caretColourId, kPrimary);
}

// ============================================================================
//  drawButtonBackground
// ============================================================================
void DevPianoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& bg,
                                               bool highlighted, bool down) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    constexpr float corner = 4.0f;

    // Base fill with subtle top-to-bottom gradient for slight convexity
    {
        juce::ColourGradient grad(bg.brighter(0.08f), bounds.getX(), bounds.getY(), bg.darker(0.04f), bounds.getX(),
                                  bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, corner);
    }

    // Highlight overlay
    if (highlighted && !down) {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(bounds, corner);
    }

    // Pressed state — darken
    if (down) {
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds, corner);
    }
}

// ============================================================================
//  drawToggleButton
// ============================================================================
void DevPianoLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) {
    // Use the tick-box path from V4 (unchanged), but with our colour overrides
    LookAndFeel_V4::drawToggleButton(g, button, highlighted, down);
}

// ============================================================================
//  drawComboBox
// ============================================================================
void DevPianoLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /* isButtonDown */, int buttonX,
                                       int buttonY, int buttonW, int buttonH, juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    constexpr float corner = 4.0f;

    // Background
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, corner);

    // Outline
    const auto outlineColour = box.isEnabled()
        ? (box.hasKeyboardFocus(true) ? box.findColour(juce::ComboBox::focusedOutlineColourId)
                                      : box.findColour(juce::ComboBox::outlineColourId))
        : box.findColour(juce::ComboBox::outlineColourId).withAlpha(0.4f);
    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // Drop-down arrow — simple triangle
    juce::Path arrow;
    const float cx = (float)buttonX + (float)buttonW * 0.5f;
    const float cy = (float)buttonY + (float)buttonH * 0.5f;
    const float a = 3.5f;
    arrow.addTriangle(cx - a, cy - a * 0.5f, cx + a, cy - a * 0.5f, cx, cy + a * 0.6f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

// ============================================================================
//  drawPopupMenuItem
// ============================================================================
void DevPianoLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool sep, bool active,
                                            bool highlighted, bool ticked, bool submenu, const juce::String& text,
                                            const juce::String& shortcut, const juce::Drawable* icon,
                                            const juce::Colour* /* textColour */) {
    if (sep) {
        g.setColour(kTextSecondary.withAlpha(0.3f));
        g.fillRect(area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
        return;
    }

    // Highlight background
    if (highlighted && active) {
        g.setColour(kPrimary);
        g.fillRect(area);
    }

    // Ticked item — draw check mark
    if (ticked) {
        g.setColour(highlighted ? kTextPrimary : kPrimary);
        const auto tick = getTickShape(6.0f);
        g.fillPath(tick,
                   tick.getTransformToScaleToFit(area.reduced(4, 0).removeFromLeft(area.getHeight()).toFloat(), true));
    }

    const auto textColour = (highlighted && active) ? kTextPrimary : (active ? kTextPrimary : kTextDisabled);
    g.setColour(textColour);
    g.setFont(juce::FontOptions(14.0f));

    const int iconW = icon != nullptr ? area.getHeight() : 0;
    const auto textBounds
        = area.reduced(iconW > 0 ? 0 : 8, 0).withTrimmedLeft(iconW).withTrimmedRight(submenu ? 16 : 4);

    if (icon != nullptr) {
        auto iconArea = area.withWidth(area.getHeight()).reduced(4, 2).toFloat();
        icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
    }

    if (shortcut.isNotEmpty()) {
        g.setColour(kTextSecondary);
        g.drawText(shortcut, textBounds, juce::Justification::centredRight);
    }

    g.setColour(textColour);
    g.drawText(text, textBounds, juce::Justification::centredLeft);

    if (submenu) {
        juce::Path arrow;
        const float cx = (float)area.getRight() - 8.0f;
        const float cy = (float)area.getCentreY();
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
    if (!slider.isHorizontal())
        return LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, slider);

    constexpr float trackThickness = 2.0f;
    const float trackY = (float)y + (float)h * 0.5f - trackThickness * 0.5f;
    const float trackW = (float)w;

    // Background track (full width)
    g.setColour(slider.findColour(juce::Slider::backgroundColourId));
    g.fillRoundedRectangle((float)x, trackY, trackW, trackThickness, 1.0f);

    // Filled track
    const float fillW = pos - (float)x;
    if (fillW > 0.0f) {
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillRoundedRectangle((float)x, trackY, fillW, trackThickness, 1.0f);
    }

    // Thumb — 6 x 18 rounded rect
    constexpr float thumbW = 6.0f;
    constexpr float thumbH = 18.0f;
    const float thumbX = juce::jlimit((float)x, (float)x + (float)w - thumbW, pos - thumbW * 0.5f);
    const float thumbY = (float)y + ((float)h - thumbH) * 0.5f;
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 2.0f);
}

// ============================================================================
//  drawRotarySlider  (placeholder for Phase 10b)
// ============================================================================
void DevPianoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float startAng,
                                           float endAng, juce::Slider& slider) {
    constexpr float arcThickness = 3.0f;
    const auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(4.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - arcThickness;
    const auto centre = bounds.getCentre();

    // Background arc
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAng, endAng, true);
    g.setColour(slider.findColour(juce::Slider::backgroundColourId));
    g.strokePath(bgArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved));

    // Filled arc
    const float filledAngle = startAng + pos * (endAng - startAng);
    juce::Path fgArc;
    fgArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAng, filledAngle, true);
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.strokePath(fgArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved));

    // Centre dot
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
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
    return juce::Font(juce::FontOptions(14.0f));
}
