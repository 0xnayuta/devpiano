#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

namespace devpiano::midi {

// ============================================================================
// Per-channel configuration within the 16-channel MIDI matrix.
//
// Each of the 16 logical input channels carries its own transpose, octave
// shift, velocity curve, program, bank, and sustain CC.  The matrix acts as
// a global configuration layer applied to every note-on/off before it reaches
// the synth.
// ============================================================================
struct PerChannelConfig {
    // Actual MIDI output channel (0-15, 0 = channel 1).
    // Setting to the same as the input channel is the default pass-through.
    uint8_t outputChannel : 4 = 0;

    // Semitone transpose offset (-48 .. +48)
    int8_t transpose : 7 = 0;

    // Octave shift (-1, 0, +1)
    int8_t octaveShift : 2 = 0;

    // Fixed velocity override (0-127).  64 = no override (use original)
    uint8_t velocity : 7 = 64;

    // Program number (0-127).  Sent on matrix activate.
    uint8_t program : 7 = 0;

    // Bank MSB (0-127).
    uint8_t bankMSB : 7 = 0;

    // Sustain pedal CC number (default 64 = standard sustain)
    uint8_t sustainCC : 7 = 64;
    // When true and global midiTranspose is enabled, this channel's note
    // events are transposed by the global keySignature offset.
    // Bit-field reuses existing struct padding — zero memory growth.
    bool followKey : 1 = false;
};

// ============================================================================
// Full 16-channel matrix.
// ============================================================================
struct ChannelMatrix {
    std::array<PerChannelConfig, 16> channels;

    // When false, the matrix is inactive and all MIDI messages pass through
    // unchanged.  This preserves backward compatibility with existing code
    // that doesn't know about the matrix.
    bool active = false;
};

// ============================================================================
// Matrix application helpers
// ============================================================================

// Apply the given per-channel config to a note-on event.
// Returns a new MidiMessage with the transformed channel, note, and velocity.
inline juce::MidiMessage applyMatrixToNoteOn(const PerChannelConfig& cfg, int originalChannel, int originalNote,
                                             float originalVelocity) {
    juce::ignoreUnused(originalChannel);
    auto outChannel = static_cast<uint8_t>(cfg.outputChannel + 1); // 0-based → 1-based
    auto outNote = static_cast<uint8_t>(
        juce::jlimit(0, 127, originalNote + cfg.transpose + static_cast<int>(cfg.octaveShift) * 12));
    auto outVel = cfg.velocity != 64
        ? cfg.velocity
        : static_cast<uint8_t>(juce::jlimit(0, 127, static_cast<int>(originalVelocity * 127.0f)));

    return juce::MidiMessage::noteOn(outChannel, outNote, outVel);
}

// Apply the given config to a note-off event (channel + note transforms only).
inline juce::MidiMessage applyMatrixToNoteOff(const PerChannelConfig& cfg, int originalChannel, int originalNote,
                                              float originalVelocity) {
    juce::ignoreUnused(originalChannel);
    auto outChannel = static_cast<uint8_t>(cfg.outputChannel + 1);
    auto outNote = static_cast<uint8_t>(
        juce::jlimit(0, 127, originalNote + cfg.transpose + static_cast<int>(cfg.octaveShift) * 12));
    auto outVel = static_cast<uint8_t>(juce::jlimit(0, 127, static_cast<int>(originalVelocity * 127.0f)));

    return juce::MidiMessage::noteOff(outChannel, outNote, outVel);
}

// Build a MIDI program-change message for the channel config.
inline juce::MidiMessage makeProgramChange(const PerChannelConfig& cfg) {
    return juce::MidiMessage::programChange(cfg.outputChannel + 1, cfg.program);
}

} // namespace devpiano::midi
