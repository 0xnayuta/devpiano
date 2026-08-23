#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
/// Custom LookAndFeel providing a dark audio-production visual theme.
///
/// Installed once on MainComponent; propagates automatically to all child
/// Components and any DialogWindow that explicitly inherits it.
class DevPianoLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DevPianoLookAndFeel();
    ~DevPianoLookAndFeel() override = default;
    /// Re-read colors from DesignTokens singleton and update all JUCE colour IDs.
    void refreshColours();

    // ── Drawing overrides ──
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& bg, bool highlighted,
                              bool down) override;
    void drawDrawableButton(juce::Graphics&, juce::DrawableButton&, bool highlighted, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                      juce::ComboBox&) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool sep, bool active, bool highlighted,
                           bool ticked, bool submenu, const juce::String& text, const juce::String& shortcut,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float startAng, float endAng,
                          juce::Slider&) override;
    void fillTextEditorBackground(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tip, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawProgressBar(juce::Graphics&, juce::ProgressBar&, int width, int height, double progress,
                         const juce::String& textToShow) override;
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea,
                      juce::TextLayout&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    int getAlertWindowButtonHeight() override;
    juce::Array<int> getWidthsForTextButtons(juce::AlertWindow&,
                                             const juce::Array<juce::TextButton*>& buttons) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevPianoLookAndFeel)
};
