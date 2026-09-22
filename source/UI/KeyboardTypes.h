#pragma once

#include <array>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "Core/MusicTheory.h"
namespace devpiano::ui {

using devpiano::core::doReMiNames;
using devpiano::core::getContrastingTextColour;
using devpiano::core::getNoteDisplayName;
using devpiano::core::getPitchClassHarmonyColour;
using devpiano::core::isWhiteKey;
using devpiano::core::NoteDisplayMode;
using devpiano::core::noteLetterNames;
using devpiano::core::pitchClassHarmonyHues;
using devpiano::core::whiteKeyIndexForNote;
using devpiano::core::whiteKeysPerOctave;

// ============================================================================
// Keyboard rendering enums and data types
// ============================================================================

// Per-key colouring mode
enum class KeyColourMode : uint8_t {
    classic = 0, // warm orange hue
    channel = 1, // 16-channel hue palette
    velocity = 2, // green-red velocity gradient
    harmony = 3, // 12-TET pitch-class chromatic harmony palette
};
// Per-key rendering state (recalculated every frame, not persisted)
struct KeyRenderState {
    int midiNote = -1;
    float fade = 0.0f; // [0, 1], decays when key not pressed
    juce::Colour colour1 { 0x00000000 };
    juce::Rectangle<float> bounds;
    juce::String keyLabel; // computer-key binding label ("A", "S", …)
    bool isWhite = false;
};

// Keyboard display settings (persisted via SettingsModel)
struct KeyboardSettings {
    int lowNote = 21;
    int highNote = 108;
    float keyWidth = 24.0f;

    KeyColourMode colourMode = KeyColourMode::classic;
    NoteDisplayMode noteDisplay = NoteDisplayMode::doReMi;

    float fadeSpeed = 0.92f; // per-tick fade decay factor
    float previewAlpha = 0.0f; // fade floor after release
    int keySignature = 0; // semitone offset for fixedDo / noteName
    int baseOctave = 4; // reference octave for note names

    // Per-key custom colour (transparent = not set, use colourMode instead)
    std::array<juce::Colour, 128> customKeyColours;

    // Per-key custom label (empty = not set, use binding displayText or note name)
    std::array<juce::String, 128> customKeyLabels;
};

} // namespace devpiano::ui
