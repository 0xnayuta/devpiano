#pragma once

#include <array>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "Core/KeyMapTypes.h"
#include "Core/MusicTheory.h"

namespace devpiano::core {

// ============================================================================
// QWERTY Physical Key Visual State
// ============================================================================
struct QwertyKeyVisualState {
    int keyCode = 0; // Key code (ASCII uppercase or juce::KeyPress constants)
    juce::String mainLabel; // Physical key cap label ("Q", "Tab", "Space")
    float widthWeight = 1.0f; // Relative width weight within row (sum is 15.0 per row)
    int mappedMidiNote = -1; // Mapped MIDI note number (-1 if unmapped or non-note)
    int mappedMidiChannel = 1; // Target MIDI channel (1..16)
    float velocity = 1.0f; // Action velocity [0.0, 1.0]
    juce::String noteName; // Pitch display ("C4", "G#3")
    juce::String solfegeLabel; // Solfege or scale degree display ("1", "do", "+1")
    bool isDown = false; // Whether currently physically held down
    bool isSustainPedal = false; // Whether this key acts as sustain pedal
    bool isSoftPedal = false; // Whether this key acts as una corda soft pedal
};

// ============================================================================
// QWERTY Row Visual State
// ============================================================================
struct QwertyRowVisualState {
    std::vector<QwertyKeyVisualState> keys;
};

// ============================================================================
// QWERTY Full View Model Snapshot
// ============================================================================
struct QwertyViewModel {
    std::array<QwertyRowVisualState, 5> rows;
    bool isSustainPedalDown = false;
    bool isSoftPedalDown = false;
    bool isSyncPedalCutPending = false;
    devpiano::core::SustainPolicy sustainPolicy = devpiano::core::SustainPolicy::normal;
    bool isShiftActive = false;
    bool isAltActive = false;
    bool isCtrlActive = false;
    uint8_t activeGroupIndex = 0;
    juce::String activeGroupName { "A" };
};

// ============================================================================
// ANSI 5-Row Layout Template Generator
// ============================================================================
// Generates the standard 5-row physical keyboard layout with strict 15.0f total
// width weight per row for precise, adaptive rectangular alignment.
[[nodiscard]] inline QwertyViewModel makeDefaultQwertyLayoutTemplate() {
    QwertyViewModel vm;

    // Helper lambda to construct a key state
    const auto makeKey = [](int keyCode, const char* label, float weight, bool isSustain = false, bool isSoft = false) {
        QwertyKeyVisualState k;
        k.keyCode = keyCode;
        k.mainLabel = label;
        k.widthWeight = weight;
        k.isSustainPedal = isSustain;
        k.isSoftPedal = isSoft;
        return k;
    };

    // ── Row 0: Number Row (14 keys, sum = 15.0) ─────────────────────────────
    auto& r0 = vm.rows[0].keys;
    r0.reserve(14);
    r0.push_back(makeKey('`', "`", 1.0f));
    r0.push_back(makeKey('1', "1", 1.0f));
    r0.push_back(makeKey('2', "2", 1.0f));
    r0.push_back(makeKey('3', "3", 1.0f));
    r0.push_back(makeKey('4', "4", 1.0f));
    r0.push_back(makeKey('5', "5", 1.0f));
    r0.push_back(makeKey('6', "6", 1.0f));
    r0.push_back(makeKey('7', "7", 1.0f));
    r0.push_back(makeKey('8', "8", 1.0f));
    r0.push_back(makeKey('9', "9", 1.0f));
    r0.push_back(makeKey('0', "0", 1.0f));
    r0.push_back(makeKey('-', "-", 1.0f));
    r0.push_back(makeKey('=', "=", 1.0f));
    r0.push_back(makeKey(juce::KeyPress::backspaceKey, "Bksp", 2.0f));

    // ── Row 1: QWERTY Row (14 keys, sum = 15.0) ────────────────────────────
    auto& r1 = vm.rows[1].keys;
    r1.reserve(14);
    r1.push_back(makeKey(juce::KeyPress::tabKey, "Tab", 1.5f, false, true));
    r1.push_back(makeKey('Q', "Q", 1.0f));
    r1.push_back(makeKey('W', "W", 1.0f));
    r1.push_back(makeKey('E', "E", 1.0f));
    r1.push_back(makeKey('R', "R", 1.0f));
    r1.push_back(makeKey('T', "T", 1.0f));
    r1.push_back(makeKey('Y', "Y", 1.0f));
    r1.push_back(makeKey('U', "U", 1.0f));
    r1.push_back(makeKey('I', "I", 1.0f));
    r1.push_back(makeKey('O', "O", 1.0f));
    r1.push_back(makeKey('P', "P", 1.0f));
    r1.push_back(makeKey('[', "[", 1.0f));
    r1.push_back(makeKey(']', "]", 1.0f));
    r1.push_back(makeKey('\\', "\\", 1.5f));

    // ── Row 2: ASDF Row (13 keys, sum = 15.0) ──────────────────────────────
    auto& r2 = vm.rows[2].keys;
    r2.reserve(13);
    r2.push_back(makeKey(0, "Caps", 1.75f));
    r2.push_back(makeKey('A', "A", 1.0f));
    r2.push_back(makeKey('S', "S", 1.0f));
    r2.push_back(makeKey('D', "D", 1.0f));
    r2.push_back(makeKey('F', "F", 1.0f));
    r2.push_back(makeKey('G', "G", 1.0f));
    r2.push_back(makeKey('H', "H", 1.0f));
    r2.push_back(makeKey('J', "J", 1.0f));
    r2.push_back(makeKey('K', "K", 1.0f));
    r2.push_back(makeKey('L', "L", 1.0f));
    r2.push_back(makeKey(';', ";", 1.0f));
    r2.push_back(makeKey('\'', "'", 1.0f));
    r2.push_back(makeKey(juce::KeyPress::returnKey, "Enter", 2.25f));

    // ── Row 3: ZXCV Row (12 keys, sum = 15.0) ──────────────────────────────
    auto& r3 = vm.rows[3].keys;
    r3.reserve(12);
    r3.push_back(makeKey(0, "Shift", 2.25f));
    r3.push_back(makeKey('Z', "Z", 1.0f));
    r3.push_back(makeKey('X', "X", 1.0f));
    r3.push_back(makeKey('C', "C", 1.0f));
    r3.push_back(makeKey('V', "V", 1.0f));
    r3.push_back(makeKey('B', "B", 1.0f));
    r3.push_back(makeKey('N', "N", 1.0f));
    r3.push_back(makeKey('M', "M", 1.0f));
    r3.push_back(makeKey(',', ",", 1.0f));
    r3.push_back(makeKey('.', ".", 1.0f));
    r3.push_back(makeKey('/', "/", 1.0f));
    r3.push_back(makeKey(0, "Shift", 2.75f));

    // ── Row 4: Bottom Function Row (7 keys, sum = 15.0) ────────────────────
    auto& r4 = vm.rows[4].keys;
    r4.reserve(7);
    r4.push_back(makeKey(0, "Ctrl", 1.5f));
    r4.push_back(makeKey(0, "Win", 1.25f));
    r4.push_back(makeKey(0, "Alt", 1.25f));
    r4.push_back(makeKey(juce::KeyPress::spaceKey, "Space (Pedal)", 6.0f, true, false));
    r4.push_back(makeKey(0, "Alt", 1.25f));
    r4.push_back(makeKey(0, "Win", 1.25f));
    r4.push_back(makeKey(0, "Ctrl", 2.5f));

    return vm;
}

} // namespace devpiano::core
