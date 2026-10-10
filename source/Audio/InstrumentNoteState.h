#pragma once

#include "Audio/OrderedMidi.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

class InstrumentNoteState {
public:
    void reset() noexcept {
        notes = {};
    }

    void append(juce::MidiBuffer& destination, const juce::MidiMessageMetadata& metadata, std::size_t source) noexcept {
        if (metadata.numBytes > 3) {
            appendOrderedMidi(destination, metadata.data, metadata.numBytes, metadata.samplePosition);
            return;
        }
        const auto message = metadata.getMessage();
        const auto channel = message.getChannel() - 1;
        if (channel < 0 || channel >= 16) {
            appendOrderedMidi(destination, metadata.data, metadata.numBytes, metadata.samplePosition);
            return;
        }
        const auto ch = static_cast<std::size_t>(channel);
        const auto other = std::size_t { 1 } - source;
        auto& held = notes[source][ch];
        const auto& otherHeld = notes[other][ch];
        if (message.isNoteOn()) {
            held[static_cast<std::size_t>(message.getNoteNumber())] = true;
        } else if (message.isNoteOff()) {
            const auto note = static_cast<std::size_t>(message.getNoteNumber());
            if (!held[note]) {
                return;
            }
            held[note] = false;
            if (otherHeld[note]) {
                return;
            }
        } else if (message.isAllSoundOff() || message.isAllNotesOff()) {
            if (std::ranges::any_of(otherHeld, [](bool active) { return active; })) {
                for (std::size_t note = 0; note < 128; ++note) {
                    if (held[note] && !otherHeld[note]) {
                        appendOrderedMidi(destination, juce::MidiMessage::noteOff(channel + 1, static_cast<int>(note)),
                                          metadata.samplePosition);
                    }
                }
                held.fill(false);
                return;
            }
            held.fill(false);
        }
        appendOrderedMidi(destination, metadata.data, metadata.numBytes, metadata.samplePosition);
    }

private:
    std::array<std::array<std::array<bool, 128>, 16>, 2> notes {};
};

} // namespace devpiano::audio
