#pragma once

#include <JuceHeader.h>

#include <functional>

#include "Core/KeyMapTypes.h"
#include "Core/KeyboardTypes.h"

// ============================================================================
// A custom-drawn piano keyboard Component.
//
// Replaces juce::MidiKeyboardComponent with JUCE Graphics-based rendering:
//  - Piano-key geometry (white + black keys)
//  - Fade in/out animation (timer-driven alpha lerp)
//  - Colour modes: classic, channel, velocity
//  - Note display modes: doReMi, fixedDo, noteName
//  - Mouse click → note on/off
//  - Double-click → binding editor (Phase 7-7)
// Steps completed: 1-4 (geometry + paint + mouse + fade), Group A (key labels +
// note display mode).  Next: Group B (channel/velocity colour modes).
// ============================================================================
class CustomKeyboard final : public juce::Component, private juce::Timer, private juce::MidiKeyboardStateListener {
public:
    explicit CustomKeyboard(juce::MidiKeyboardState& keyboardState);
    ~CustomKeyboard() override = default;

    // ---- Settings ----------------------------------------------------------
    void setKeyboardSettings(const devpiano::ui::KeyboardSettings& settings);
    [[nodiscard]] const devpiano::ui::KeyboardSettings& getKeyboardSettings() const noexcept;

    // ---- Callbacks ---------------------------------------------------------
    std::function<void(int midiNote, int sourceChannel)> onNoteOn;
    std::function<void(int midiNote, int sourceChannel)> onNoteOff;
    std::function<void(int midiNote)> onBindingEditRequested;

    // ---- Keyboard interface ------------------------------------------------
    void setLowestVisibleNote(int note);
    void setAvailableRange(int low, int high);

    // ---- Layout ------------------------------------------------------------
    void setKeyboardLayout(const devpiano::core::KeyboardLayout& layout);

    // ---- External notification ---------------------------------------------
    // Wake the fade timer when notes arrive from outside (physical keyboard,
    // external MIDI).  Safe to call redundantly; timer runs at ~30 fps.
    void notifyNoteActivity();

private:
    // juce::Component
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::MouseListener
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // juce::Timer

    // juce::MidiKeyboardStateListener
    void handleNoteOn(juce::MidiKeyboardState*, int, int, float) override;
    void handleNoteOff(juce::MidiKeyboardState*, int, int, float) override;
    void timerCallback() override;

    // Geometry
    void recalculateKeyBounds();
    [[nodiscard]] int findNoteAt(juce::Point<int> position) const;

    // Rendering helpers
    void paintWhiteKeys(juce::Graphics& g);
    void paintBlackKeys(juce::Graphics& g);
    void paintKeyLabels(juce::Graphics& g);

    // Fade animation
    void ensureTimerRunning();

    // Dependencies
    juce::MidiKeyboardState& keyboardState;

    // State
    devpiano::ui::KeyboardSettings settings;
    std::vector<devpiano::ui::KeyRenderState> keys;

    int lowestVisibleNote = 24;
    int rangeLow = 24;
    int rangeHigh = 96;
    int lastMouseDownNote = -1;

    // Per-key binding data for colour mode computation, indexed by MIDI note.
    // Populated by setKeyboardLayout().  Unbound notes default to channel 0 / vel 1.0.
    std::array<uint8_t, 128> perKeyChannel {};
    std::array<float, 128> perKeyVelocity {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomKeyboard)
};
