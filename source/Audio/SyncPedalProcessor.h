#pragma once
#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "Core/KeyMapTypes.h"

namespace devpiano::audio {

// ============================================================================
// SyncPedalProcessor (Phase 34-C)
//
// Realtime-safe, sample-accurate syncopated legato pedal scheduler.
// While the pedal is physically held, a new attack at sample N is rewritten to
// CC64(0) -> NoteOn -> CC64(127). A release only arms cutPending; the damp
// CC64(0) is emitted in front of the next NoteOn and sustain is not re-engaged.
//
// JUCE inserts a MidiBuffer event after every event already stored at the same
// sample (findEventAfter uses <=). keyboardState therefore places a NoteOn
// behind a CC64(0) that the collector queued earlier in the same block. This
// scheduler owns that ordering: one damp, then the attack, then at most one
// re-engage, with every other same-sample CC64 dropped.
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
        sustainChannels.store(0, std::memory_order_relaxed);
    }

    /// Schedule sample-accurate sync pedal events within an audio block's MidiBuffer.
    /// Held pedal: CC64(0) -> NoteOn -> CC64(127) at one sample.
    /// Armed cut:  CC64(0) -> NoteOn, then the pedal stays up.
    void processMidiBlock(juce::MidiBuffer& buffer, juce::MidiBuffer& tempBuffer) noexcept {
        if (currentPolicy.load(std::memory_order_relaxed) == devpiano::core::SustainPolicy::normal) {
            observeSustainChannels(buffer);
            return;
        }

        const auto pedalDown = pedalPhysicallyDown.load(std::memory_order_relaxed);
        auto cut = cutPending.load(std::memory_order_relaxed);

        bool hasEventsToModify = false;
        for (const auto meta : buffer) {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn() && msg.getVelocity() > 0 && (pedalDown || cut)) {
                hasEventsToModify = true;
                break;
            }
        }

        if (!hasEventsToModify) {
            observeSustainChannels(buffer);
            return;
        }

        tempBuffer.clear();
        auto heldChannels = sustainChannels.load(std::memory_order_relaxed);

        auto index = buffer.begin();
        const auto end = buffer.end();
        while (index != end) {
            const auto anchor = (*index).samplePosition;
            const auto groupEnd = std::find_if(
                index, end, [anchor](const juce::MidiMessageMetadata& meta) { return meta.samplePosition != anchor; });

            const auto attack = std::find_if(index, groupEnd, [](const juce::MidiMessageMetadata& meta) {
                const auto msg = meta.getMessage();
                return msg.isNoteOn() && msg.getVelocity() > 0;
            });

            if (attack != groupEnd && (pedalDown || cut)) {
                auto affectedChannels = heldChannels;
                for (auto cursor = index; cursor != groupEnd; ++cursor) {
                    const auto msg = (*cursor).getMessage();
                    if (msg.isNoteOn() || msg.isControllerOfType(64)) {
                        affectedChannels |= static_cast<std::uint16_t>(1U << (msg.getChannel() - 1));
                    }
                }
                std::array<int, 16> channels {};
                std::size_t numChannels = 0;
                for (int channel = 1; channel <= 16; ++channel) {
                    if ((affectedChannels & (1U << (channel - 1))) != 0) {
                        channels[numChannels++] = channel;
                    }
                }

                // 1. CC64(0) damp for each attack channel. A CC64 already queued
                // at this sample (e.g. collector release landing behind the NoteOn)
                // is absorbed here and not forwarded.
                for (std::size_t i = 0; i < numChannels; ++i) {
                    tempBuffer.addEvent(juce::MidiMessage::controllerEvent(channels[i], 64, 0), anchor);
                }

                // 2. NoteOffs first! Releasing old notes before striking new notes
                // prevents same-sample voice choke on fast re-strikes / legato.
                for (auto cursor = index; cursor != groupEnd; ++cursor) {
                    const auto msg = (*cursor).getMessage();
                    if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0)) {
                        tempBuffer.addEvent(msg, anchor);
                    }
                }

                // 3. Other non-Note, non-CC64 events (e.g. pitch bend, mod wheel)
                for (auto cursor = index; cursor != groupEnd; ++cursor) {
                    const auto msg = (*cursor).getMessage();
                    if (!msg.isControllerOfType(64) && !msg.isNoteOff() && !msg.isNoteOn()) {
                        tempBuffer.addEvent(msg, anchor);
                    }
                }

                // 4. NoteOn attacks
                for (auto cursor = index; cursor != groupEnd; ++cursor) {
                    const auto msg = (*cursor).getMessage();
                    if (msg.isNoteOn() && msg.getVelocity() > 0) {
                        tempBuffer.addEvent(msg, anchor);
                    }
                }

                // 5. If pedal physically held, re-engage sustain for all attack channels
                if (pedalDown) {
                    for (std::size_t i = 0; i < numChannels; ++i) {
                        tempBuffer.addEvent(juce::MidiMessage::controllerEvent(channels[i], 64, 127), anchor);
                    }
                }
                heldChannels = pedalDown ? affectedChannels : std::uint16_t { 0 };

                if (cut) {
                    cutPending.store(false, std::memory_order_relaxed);
                    cut = false;
                }
            } else {
                for (auto cursor = index; cursor != groupEnd; ++cursor) {
                    const auto message = (*cursor).getMessage();
                    tempBuffer.addEvent(message, anchor);
                    updateSustainChannels(heldChannels, message);
                }
            }

            index = groupEnd;
        }

        if (!cut && cutPending.load(std::memory_order_relaxed)) {
            cutPending.store(false, std::memory_order_relaxed);
        }
        sustainChannels.store(heldChannels, std::memory_order_relaxed);

        buffer.swapWith(tempBuffer);
        tempBuffer.clear();
    }

private:
    static void updateSustainChannels(std::uint16_t& mask, const juce::MidiMessage& message) noexcept {
        const auto channel = message.getChannel();
        if (channel < 1 || channel > 16 || !message.isController()) {
            return;
        }
        const auto bit = static_cast<std::uint16_t>(1U << (channel - 1));
        if (message.isControllerOfType(64)) {
            mask = message.getControllerValue() >= 64 ? static_cast<std::uint16_t>(mask | bit)
                                                      : static_cast<std::uint16_t>(mask & ~bit);
        } else if (message.isControllerOfType(120) || message.isControllerOfType(121)
                   || message.isControllerOfType(123)) {
            mask &= static_cast<std::uint16_t>(~bit);
        }
    }

    void observeSustainChannels(const juce::MidiBuffer& buffer) noexcept {
        auto mask = sustainChannels.load(std::memory_order_relaxed);
        for (const auto metadata : buffer) {
            updateSustainChannels(mask, metadata.getMessage());
        }
        sustainChannels.store(mask, std::memory_order_relaxed);
    }

    std::atomic<std::uint16_t> sustainChannels { 0 };
    std::atomic<devpiano::core::SustainPolicy> currentPolicy { devpiano::core::SustainPolicy::normal };
    std::atomic<bool> pedalPhysicallyDown { false };
    std::atomic<bool> cutPending { false };
};

} // namespace devpiano::audio
