#pragma once

#include <functional>
#include <optional>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "Core/MidiTypes.h"
#include "Core/QwertyModel.h"

namespace devpiano::ui {

// ============================================================================
// QwertyComponent
//
// High-performance custom-drawn 5-row QWERTY keyboard Performance Map.
// Visualises physical keyboard layout, note mappings, active keypresses with
// tactile depression, and smooth analog phosphor fade-out animation.
// ============================================================================
class QwertyComponent final : public juce::Component, private juce::Timer {
public:
    QwertyComponent();
    ~QwertyComponent() override;

    // ---- View Model & State ------------------------------------------------
    void updateViewModel(const devpiano::core::QwertyViewModel& newModel);
    [[nodiscard]] const devpiano::core::QwertyViewModel& getViewModel() const noexcept {
        return viewModel;
    }
    [[nodiscard]] const devpiano::core::ChordInfo& getLastDisplayedChord() const noexcept {
        return lastDisplayedChord;
    }
    [[nodiscard]] float getChordFadeAlpha() const noexcept {
        return chordFadeAlpha;
    }

    // ---- Interaction Callbacks ---------------------------------------------
    std::function<devpiano::core::MidiNoteIdentity(int midiNote, int midiChannel, float velocity)> onNoteOn;
    std::function<devpiano::core::MidiNoteIdentity(int physicalKeyCode, int inputNote, int inputChannel,
                                                   float velocity)>
        onPhysicalNoteOn;
    std::function<void(const devpiano::core::MidiNoteIdentity&)> onNoteOff;
    std::function<void(const devpiano::core::MidiNoteIdentity&, int physicalKeyCode)> onPhysicalNoteOff;

    std::function<void(int midiNote, int physicalKeyCode)> onBindingEditRequested;
    // ---- Mouse Interaction -------------------------------------------------
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void releaseHeldMouseNote();
    // ---- Hit Testing -------------------------------------------------------
    struct HitResult {
        int rowIndex = -1;
        int keyIndex = -1;
        bool isNumpad = false;
        const devpiano::core::QwertyKeyVisualState* key = nullptr;
    };
    [[nodiscard]] HitResult findKeyAt(juce::Point<int> position) const;
    // ---- Public Test Hook --------------------------------------------------
    void triggerTimerForTest() {
        timerCallback();
    }
    [[nodiscard]] bool isTimerRunningForTest() {
        return isTimerRunning();
    }

private:
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Geometry calculation for layout bounds
    void recalculateKeyBounds();

    struct KeyGeometry {
        juce::Rectangle<float> bounds;
        float fadeAlpha = 0.0f;
    };

    devpiano::core::QwertyViewModel viewModel;
    std::array<std::vector<KeyGeometry>, 5> keyGeometries;
    std::array<std::vector<KeyGeometry>, 5> numpadGeometries;
    int scrollOffsetX = 0;
    int maxScrollOffset = 0;
    int lastMouseDownNote = -1;
    int lastMouseDownKeyCode = 0;
    std::optional<devpiano::core::MidiNoteIdentity> lastMouseDownIdentity;
    devpiano::core::ChordInfo lastDisplayedChord;
    float chordFadeAlpha = 0.0f;
    static constexpr float chordFadeDecayFactor = 0.70f;

    static constexpr int timerIntervalMs = 20; // 50 fps smooth decay
    static constexpr float fadeDecayFactor = 0.86f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QwertyComponent)
};

} // namespace devpiano::ui
