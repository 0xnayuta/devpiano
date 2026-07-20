#pragma once

#include <JuceHeader.h>

#include <array>
#include <optional>
#include <vector>

#include "Core/ChannelMatrix.h"
#include "Core/KeyboardTypes.h"
#include "Core/KeyMapTypes.h"

namespace devpiano::layout {

inline constexpr int performancePresetFormatVersion = 1;

struct PerformancePreset {
    juce::String name;

    devpiano::core::KeyboardLayout layout;
    devpiano::midi::ChannelMatrix channelMatrix;

    // Keyboard display / musical settings subset.
    // Mirrors the JSON "keyboard" section — maps directly to SettingsModel fields
    // without going through ui::KeyboardSettings indirection.
    int keySignature = 0;
    bool midiTranspose = false;
    devpiano::ui::KeyColourMode colourMode = devpiano::ui::KeyColourMode::classic;
    devpiano::ui::NoteDisplayMode noteDisplay = devpiano::ui::NoteDisplayMode::doReMi;
    float fadeSpeed = 0.92f;
    float previewAlpha = 0.0f;
    std::array<juce::String, 128> customKeyLabels;
    std::array<juce::Colour, 128> customKeyColours;
};

// ---- File management ----

[[nodiscard]] juce::File getPresetDirectory();
[[nodiscard]] juce::String sanitisePresetFileName(const juce::String& name);
[[nodiscard]] juce::String getPresetDisplayNameForFile(const juce::File& path);

// ---- I/O ----

[[nodiscard]] std::optional<PerformancePreset> loadPreset(const juce::File& path);
[[nodiscard]] bool savePreset(const PerformancePreset& preset, const juce::File& path);

// ---- Directory scanning ----

[[nodiscard]] std::vector<PerformancePreset> scanPresetDirectory();

// ---- Built-in defaults ----

[[nodiscard]] PerformancePreset makeDefaultPreset();

} // namespace devpiano::layout
