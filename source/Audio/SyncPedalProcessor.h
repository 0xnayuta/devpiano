#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "Core/KeyMapTypes.h"

namespace devpiano::audio {

// ============================================================================
// SyncPedalProcessor (Phase 34-C)
//
// Realtime-safe, sample-accurate syncopated legato pedal scheduler.
// Enforces: CC64(0) -> NoteOn(newNote) -> CC64(127) at the identical sample
// offset during new note attacks while the pedal is held or in pending cut state.
// ============================================================================
class SyncPedalProcessor {
public:
    SyncPedalProcessor() = default;

    void setPolicy(devpiano::core::SustainPolicy policy) noexcept {
        currentPolicy = policy;
        if (policy == devpiano::core::SustainPolicy::normal) {
            cutPending = false;
        }
    }

    [[nodiscard]] devpiano::core::SustainPolicy getPolicy() const noexcept {
        return currentPolicy;
    }

    void setPedalDown(bool isDown) noexcept {
        pedalPhysicallyDown = isDown;
        if (isDown) {
            cutPending = false;
        } else if (currentPolicy == devpiano::core::SustainPolicy::syncPedal) {
            cutPending = true;
        }
    }

    [[nodiscard]] bool isPedalDown() const noexcept {
        return pedalPhysicallyDown;
    }

    [[nodiscard]] bool isCutPending() const noexcept {
        return cutPending;
    }

    void reset() noexcept {
        pedalPhysicallyDown = false;
        cutPending = false;
    }

    /// Schedule sample-accurate sync pedal events within an audio block's MidiBuffer.
    /// Guarantees: CC64(0) -> NoteOn -> CC64(127) at the identical samplePosition.
    void processMidiBlock(juce::MidiBuffer& buffer, juce::MidiBuffer& tempBuffer) noexcept {
        if (currentPolicy == devpiano::core::SustainPolicy::normal) {
            return;
        }

        bool hasEventsToModify = false;
        for (const auto meta : buffer) {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn() && msg.getVelocity() > 0) {
                if (pedalPhysicallyDown || cutPending) {
                    hasEventsToModify = true;
                    break;
                }
            }
        }

        if (!hasEventsToModify) {
            return;
        }

        tempBuffer.clear();

        for (const auto meta : buffer) {
            const auto msg = meta.getMessage();
            const auto samplePos = meta.samplePosition;

            if (msg.isNoteOn() && msg.getVelocity() > 0 && (pedalPhysicallyDown || cutPending)) {
                const auto ch = msg.getChannel();
                // 1. CC64 = 0: damp prior chord sustain at exact sample position
                tempBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 0), samplePos);
                // 2. NoteOn: attack new note
                tempBuffer.addEvent(msg, samplePos);
                // 3. CC64 = 127: re-engage sustain immediately for legato hold
                tempBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 127), samplePos);

                cutPending = false;
            } else {
                tempBuffer.addEvent(msg, samplePos);
            }
        }

        buffer.swapWith(tempBuffer);
        tempBuffer.clear();
    }

private:
    devpiano::core::SustainPolicy currentPolicy = devpiano::core::SustainPolicy::normal;
    bool pedalPhysicallyDown = false;
    bool cutPending = false;
};

} // namespace devpiano::audio
