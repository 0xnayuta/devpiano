#include "UI/native/TimelineBar.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "UI/jive/DesignTokens.h"

void TimelineBar::setTimeline(std::int64_t positionSamplesIn, std::int64_t lengthSamplesIn, double sampleRateIn,
                              devpiano::recording::AbLoopRange loopRangeIn) {
    lengthSamples = std::max<std::int64_t>(lengthSamplesIn, 0);
    positionSamples = std::clamp(positionSamplesIn, std::int64_t { 0 }, lengthSamples);
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 0.0;
    loopRange = loopRangeIn;
    repaint();
}

void TimelineBar::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(4.0f, 1.0f);
    if (bounds.isEmpty()) {
        return;
    }

    const auto primary = devpiano::jive::DesignTokens::get().primary();
    const auto textColour = isEnabled() ? juce::Colour(0xFFE5E8EF) : juce::Colour(0xFF737986);
    const auto trackColour = juce::Colour(0xFF252932);
    const auto markerColour = juce::Colour(0xFFFFC857);

    const auto headerBounds = bounds.withHeight(16.0f);
    const auto startButton = getActionBounds(0);
    const auto endButton = getActionBounds(1);
    const auto clearButton = getActionBounds(2);
    const auto textRight = startButton.getX() - 6.0f;
    const auto timeText = formatTime(isDragging ? dragPositionSamples : positionSamples, sampleRate) + " / "
        + formatTime(lengthSamples, sampleRate);

    g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(10.0f));
    g.setColour(textColour);
    g.drawText(timeText, headerBounds.withRight(textRight), juce::Justification::centredLeft, true);
    const juce::Rectangle<float> buttons[] { startButton, endButton, clearButton };
    const juce::String labels[] { TRANS("Set A"), TRANS("Set B"), TRANS("Clear Loop") };
    for (int i = 0; i < 3; ++i) {
        g.setColour(isEnabled() ? juce::Colour(0xFF262B34) : juce::Colour(0xFF1C1F25));
        g.fillRoundedRectangle(buttons[i], 3.0f);
        g.setColour(textColour);
        g.drawText(labels[i], buttons[i], juce::Justification::centred, true);
    }

    const auto track = getTrackBounds();
    if (track.isEmpty()) {
        return;
    }

    g.setColour(trackColour);
    g.fillRoundedRectangle(track, track.getHeight() * 0.5f);

    if (loopRange.isValid() && lengthSamples > 0) {
        const auto startX = track.getX()
            + track.getWidth()
                * static_cast<float>(static_cast<double>(loopRange.startSamples) / static_cast<double>(lengthSamples));
        const auto endX = track.getX()
            + track.getWidth()
                * static_cast<float>(static_cast<double>(loopRange.endSamples) / static_cast<double>(lengthSamples));
        if (endX > startX) {
            g.setColour(primary.withAlpha(isEnabled() ? 0.28f : 0.12f));
            g.fillRoundedRectangle(juce::Rectangle<float>(startX, track.getY(), endX - startX, track.getHeight()),
                                   track.getHeight() * 0.5f);
        }
    }

    const auto shownPosition = isDragging ? dragPositionSamples : positionSamples;
    const auto progress = lengthSamples > 0
        ? static_cast<float>(static_cast<double>(shownPosition) / static_cast<double>(lengthSamples))
        : 0.0f;
    const auto playheadX = track.getX() + track.getWidth() * juce::jlimit(0.0f, 1.0f, progress);
    if (playheadX > track.getX()) {
        g.setColour(primary.withAlpha(isEnabled() ? 0.78f : 0.32f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(track.getX(), track.getY(), playheadX - track.getX(), track.getHeight()),
            track.getHeight() * 0.5f);
    }

    const auto drawMarker = [&g, &track](std::int64_t marker, std::int64_t length, juce::Colour colour) {
        if (length <= 0) {
            return;
        }
        const auto proportion = juce::jlimit(0.0, 1.0, static_cast<double>(marker) / static_cast<double>(length));
        const auto x = track.getX() + track.getWidth() * static_cast<float>(proportion);
        g.setColour(colour);
        g.drawLine(x, track.getY() - 3.0f, x, track.getBottom() + 3.0f, 1.5f);
    };

    if (loopRange.hasStart) {
        drawMarker(loopRange.startSamples, lengthSamples, markerColour);
    }
    if (loopRange.hasEnd) {
        drawMarker(loopRange.endSamples, lengthSamples, markerColour);
    }

    g.setColour(isEnabled() ? juce::Colours::white : textColour);
    g.fillEllipse(playheadX - 4.0f, track.getCentreY() - 4.0f, 8.0f, 8.0f);
}

void TimelineBar::mouseDown(const juce::MouseEvent& event) {
    if (!isEnabled()) {
        return;
    }

    const auto point = event.position;
    if (getActionBounds(0).contains(point)) {
        if (onSetStart) {
            onSetStart();
        }
        return;
    }
    if (getActionBounds(1).contains(point)) {
        if (onSetEnd) {
            onSetEnd();
        }
        return;
    }
    if (getActionBounds(2).contains(point)) {
        if (onClearLoop) {
            onClearLoop();
        }
        return;
    }

    if (getTrackBounds().expanded(0.0f, 5.0f).contains(point)) {
        isDragging = true;
        seekToPosition(point.x);
    }
}

void TimelineBar::mouseDrag(const juce::MouseEvent& event) {
    if (isDragging) {
        seekToPosition(event.position.x);
    }
}

void TimelineBar::mouseUp(const juce::MouseEvent&) {
    if (isDragging) {
        positionSamples = dragPositionSamples;
    }
    isDragging = false;
    repaint();
}

juce::Rectangle<float> TimelineBar::getTrackBounds() const noexcept {
    const auto width = std::max(0.0f, static_cast<float>(getWidth()) - 16.0f);
    return { 8.0f, static_cast<float>(getHeight()) - 12.0f, width, 7.0f };
}

juce::Rectangle<float> TimelineBar::getActionBounds(int actionIndex) const noexcept {
    const auto right = static_cast<float>(getWidth()) - 8.0f;
    const auto y = 1.0f;
    constexpr auto height = 15.0f;
    constexpr auto gap = 3.0f;
    constexpr auto narrowWidth = 44.0f;
    constexpr auto clearWidth = 58.0f;

    if (actionIndex == 2) {
        return { right - clearWidth, y, clearWidth, height };
    }
    if (actionIndex == 1) {
        return { right - clearWidth - gap - narrowWidth, y, narrowWidth, height };
    }
    return { right - clearWidth - gap - narrowWidth - gap - narrowWidth, y, narrowWidth, height };
}

std::int64_t TimelineBar::getSampleAtPosition(float x) const noexcept {
    const auto track = getTrackBounds();
    if (lengthSamples <= 0 || track.getWidth() <= 0.0f) {
        return 0;
    }

    const auto proportion = juce::jlimit(0.0, 1.0, (static_cast<double>(x) - track.getX()) / track.getWidth());
    return std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::llround(proportion * static_cast<double>(lengthSamples))), 0, lengthSamples);
}

juce::String TimelineBar::formatTime(std::int64_t samples, double rate) {
    if (rate <= 0.0 || samples <= 0) {
        return "00:00.000";
    }

    const auto roundedMs = std::round(static_cast<long double>(samples) * 1000.0L / rate);
    const auto maxMs = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    const auto milliseconds = static_cast<std::int64_t>(std::min(roundedMs, maxMs));
    const auto minutes = milliseconds / 60000;
    const auto seconds = static_cast<int>((milliseconds / 1000) % 60);
    const auto fraction = static_cast<int>(milliseconds % 1000);
    const auto secondText = juce::String(seconds).paddedLeft('0', 2);
    const auto fractionText = juce::String(fraction).paddedLeft('0', 3);
    const auto minuteText = juce::String(minutes).paddedLeft('0', 2);
    return minuteText + ":" + secondText + "." + fractionText;
}

void TimelineBar::seekToPosition(float x) {
    dragPositionSamples = getSampleAtPosition(x);
    repaint();
    if (onSeek) {
        onSeek(dragPositionSamples);
    }
}
