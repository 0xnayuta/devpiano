#pragma once

#include <cstdint>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Recording/AbLoopEngine.h"

class TimelineBar final : public juce::Component {
public:
    using SeekCallback = std::function<void(std::int64_t)>;
    using MarkerCallback = std::function<void()>;

    void setTimeline(std::int64_t positionSamples, std::int64_t lengthSamples, double sampleRate,
                     devpiano::recording::AbLoopRange loopRange);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    SeekCallback onSeek;
    MarkerCallback onSetStart;
    MarkerCallback onSetEnd;
    MarkerCallback onClearLoop;

private:
    [[nodiscard]] juce::Rectangle<float> getTrackBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<float> getActionBounds(int actionIndex) const noexcept;
    [[nodiscard]] std::int64_t getSampleAtPosition(float x) const noexcept;
    [[nodiscard]] static juce::String formatTime(std::int64_t samples, double sampleRate);
    void seekToPosition(float x);

    std::int64_t positionSamples = 0;
    std::int64_t dragPositionSamples = 0;
    std::int64_t lengthSamples = 0;
    double sampleRate = 0.0;
    devpiano::recording::AbLoopRange loopRange;
    bool isDragging = false;
};
