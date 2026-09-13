#pragma once

#include <array>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <optional>
#include <vector>

#include "../Audio/RoomReverbEngine.h"
#include "../Audio/TemperamentEngine.h"
#include "../Midi/ChannelMatrix.h"
#include "../Settings/SettingsModel.h"
#include "../UI/KeyboardTypes.h"
#include "Core/KeyMapTypes.h"

namespace devpiano::layout {

inline constexpr int performancePresetFormatVersion = 1;

struct PerformancePreset {
    juce::String name;

    devpiano::core::KeyboardLayout layout;
    devpiano::midi::ChannelMatrix channelMatrix;

    // Acoustic settings
    SettingsModel::LidPosition lidPosition = SettingsModel::LidPosition::fullOpen;
    devpiano::input::TouchVelocityCurve touchVelocityCurve = devpiano::input::TouchVelocityCurve::standard;
    bool unaCorda = false;
    devpiano::audio::Temperament temperament = devpiano::audio::Temperament::equal;
    double referencePitchA4 = devpiano::audio::TemperamentEngine::kDefaultReferencePitch;
    devpiano::audio::SoundPerspective soundPerspective = devpiano::audio::SoundPerspective::player;
    devpiano::audio::ReverbSpace reverbSpace = devpiano::audio::ReverbSpace::chamber;
    float reverbWet = 0.0f;
    // 机械物理噪声与琴体微衰退 (Phase 32-A/C)
    float pedalNoiseLevel = 0.6f;
    float feltAgeingAmount = 0.0f;

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

[[nodiscard]] std::vector<PerformancePreset> scanPresetDirectory(const juce::File& dir = getPresetDirectory());

// ---- Built-in defaults ----

[[nodiscard]] PerformancePreset makeDefaultPreset();

} // namespace devpiano::layout
