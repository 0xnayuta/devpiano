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
//
// Threading contract (Phase 34-C hardening):
//   pedalPhysicallyDown / cutPending / currentPolicy are written by the
//   message thread (AudioEngine::sendController -> setPedalDown/setPolicy) and
//   read by the realtime audio callback (processMidiBlock).  They are stored
//   as std::atomic with std::memory_order_relaxed on both sides — matching
//   the pattern used by masterGain / pendingBrightness / allNotesOffPending
//   elsewhere in AudioEngine.  The constructor initialises cutPending and
//   pedalPhysicallyDown; currentPolicy is set via the load() default.
// ============================================================================
class SyncPedalProcessor {
public:
    SyncPedalProcessor() = default;

    void setPolicy(devpiano::core::SustainPolicy policy) noexcept {
        currentPolicy.store(policy, std::memory_order_relaxed);
        if (policy == devpiano::core::SustainPolicy::normal) {
            cutPending.store(false, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] devpiano::core::SustainPolicy getPolicy() const noexcept {
        return currentPolicy.load(std::memory_order_relaxed);
    }

    void setPedalDown(bool isDown) noexcept {
        pedalPhysicallyDown.store(isDown, std::memory_order_relaxed);
        if (isDown) {
            cutPending.store(false, std::memory_order_relaxed);
        } else if (currentPolicy.load(std::memory_order_relaxed) == devpiano::core::SustainPolicy::syncPedal) {
            cutPending.store(true, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] bool isPedalDown() const noexcept {
        return pedalPhysicallyDown.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool isCutPending() const noexcept {
        return cutPending.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        pedalPhysicallyDown.store(false, std::memory_order_relaxed);
        cutPending.store(false, std::memory_order_relaxed);
    }

    /// Schedule sample-accurate sync pedal events within an audio block's MidiBuffer.
    /// Guarantees: CC64(0) -> NoteOn -> CC64(127) at the identical samplePosition.
    void processMidiBlock(juce::MidiBuffer& buffer, juce::MidiBuffer& tempBuffer) noexcept {
        if (currentPolicy.load(std::memory_order_relaxed) == devpiano::core::SustainPolicy::normal) {
            return;
        }

        const auto pedalDown = pedalPhysicallyDown.load(std::memory_order_relaxed);
        auto cut = cutPending.load(std::memory_order_relaxed);

        bool hasEventsToModify = false;
        for (const auto meta : buffer) {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn() && msg.getVelocity() > 0) {
                if (pedalDown || cut) {
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

            if (msg.isNoteOn() && msg.getVelocity() > 0 && (pedalDown || cut)) {
                const auto ch = msg.getChannel();
                // 1. CC64 = 0: damp prior chord sustain at exact sample position
                tempBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 0), samplePos);
                // 2. NoteOn: attack new note
                tempBuffer.addEvent(msg, samplePos);
                // 3. CC64 = 127: re-engage sustain immediately for legato hold
                tempBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 127), samplePos);

                cut = false;
            } else {
                tempBuffer.addEvent(msg, samplePos);
            }
        }

        if (cut == false && cutPending.load(std::memory_order_relaxed)) {
            cutPending.store(false, std::memory_order_relaxed);
        }

        buffer.swapWith(tempBuffer);
        tempBuffer.clear();
    }

private:
    std::atomic<devpiano::core::SustainPolicy> currentPolicy { devpiano::core::SustainPolicy::normal };
    std::atomic<bool> pedalPhysicallyDown { false };
    std::atomic<bool> cutPending { false };
};

} // namespace devpiano::audio
