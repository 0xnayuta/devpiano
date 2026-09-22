#pragma once

#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

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

    // ---- Interaction Callbacks ---------------------------------------------
    std::function<void(int midiNote, int midiChannel, float velocity)> onNoteOn;
    std::function<void(int midiNote, int midiChannel)> onNoteOff;
    std::function<void(int midiNote)> onBindingEditRequested;

    // ---- Mouse Interaction -------------------------------------------------
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // ---- Hit Testing -------------------------------------------------------
    struct HitResult {
        int rowIndex = -1;
        int keyIndex = -1;
        const devpiano::core::QwertyKeyVisualState* key = nullptr;
    };
    [[nodiscard]] HitResult findKeyAt(juce::Point<int> position) const;

    // ---- Public Test Hook --------------------------------------------------
    void triggerTimerForTest() {
        timerCallback();
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

    int lastMouseDownNote = -1;
    int lastMouseDownChannel = 1;

    static constexpr int timerIntervalMs = 20; // 50 fps smooth decay
    static constexpr float fadeDecayFactor = 0.86f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QwertyComponent)
};

} // namespace devpiano::ui
