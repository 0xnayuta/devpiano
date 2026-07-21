#pragma once

#include <JuceHeader.h>

/// Custom LookAndFeel providing a dark audio-production visual theme.
///
/// Installed once on MainComponent; propagates automatically to all child
/// Components and any DialogWindow that explicitly inherits it.
class DevPianoLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DevPianoLookAndFeel();
    ~DevPianoLookAndFeel() override = default;

    // ── Palette constants (shared with CustomKeyboard, status bar, dialogs) ──
    static inline const auto kMainBg = juce::Colour(0xff1a1c1e); // deep charcoal
    static inline const auto kPanelBg = juce::Colour(0xff24262a); // cool dark grey
    static inline const auto kControlBg = juce::Colour(0xff2d3035); // widget base
    static inline const auto kPrimary = juce::Colour(0xff00b4d8); // ice blue
    static inline const auto kRecordActive = juce::Colour(0xffe07b3c); // soft orange
    static inline const auto kPlayActive = juce::Colour(0xff4ecdc4); // mint green
    static inline const auto kTextPrimary = juce::Colour(0xffeeeeee);
    static inline const auto kTextSecondary = juce::Colour(0xff999999);
    static inline const auto kTextDisabled = juce::Colour(0xff555555);
    // ── Drawing overrides ──
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& bg, bool highlighted,
                              bool down) override;
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

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevPianoLookAndFeel)
};
