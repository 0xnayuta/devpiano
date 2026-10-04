#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace devpiano::audio {
struct PlaybackIdentityTracker {
    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFF;
    static constexpr std::size_t kDefaultCapacity = 1024;

    struct Node {
        std::uint8_t outputPitch = 0;
        std::uint32_t next = kInvalidIndex;
    };

    struct Queue {
        std::uint32_t head = kInvalidIndex;
        std::uint32_t tail = kInvalidIndex;
    };

    std::vector<Node> pool;
    std::uint32_t freeListHead = kInvalidIndex;
    std::uint64_t currentGeneration = 0;

    std::array<std::array<Queue, 128>, 16> sourceQueues {};
    std::array<std::array<std::size_t, 128>, 16> outputHolders {};

    PlaybackIdentityTracker() {
        pool.resize(kDefaultCapacity);
        resetOwnership();
    }

    void prepare(std::uint64_t generation, std::size_t noteOnCount) {
        const auto requiredCapacity = std::max(noteOnCount, kDefaultCapacity);
        if (currentGeneration == generation && pool.size() >= requiredCapacity) {
            return;
        }
        currentGeneration = generation;
        if (pool.size() < requiredCapacity) {
            pool.resize(requiredCapacity);
        }
        resetOwnership();
    }

    void resetOwnership() noexcept {
        for (auto& ch : sourceQueues) {
            ch.fill({ kInvalidIndex, kInvalidIndex });
        }
        for (auto& ch : outputHolders) {
            ch.fill(0);
        }
        initFreeList();
    }

    void resetChannel(int channel) noexcept {
        const auto chIdx = juce::jlimit(0, 15, channel - 1);
        for (int pitch = 0; pitch < 128; ++pitch) {
            auto& q = sourceQueues[chIdx][pitch];
            auto curr = q.head;
            while (curr != kInvalidIndex && curr < pool.size()) {
                const auto next = pool[curr].next;
                freeNode(curr);
                curr = next;
            }
            q = { kInvalidIndex, kInvalidIndex };
            outputHolders[chIdx][pitch] = 0;
        }
    }

    void initFreeList() noexcept {
        if (pool.empty()) {
            freeListHead = kInvalidIndex;
            return;
        }
        for (std::size_t i = 0; i + 1 < pool.size(); ++i) {
            pool[i].next = static_cast<std::uint32_t>(i + 1);
        }
        pool.back().next = kInvalidIndex;
        freeListHead = 0;
    }

    std::uint32_t allocateNode() noexcept {
        if (freeListHead == kInvalidIndex) {
            return kInvalidIndex;
        }
        const auto idx = freeListHead;
        freeListHead = pool[idx].next;
        pool[idx].next = kInvalidIndex;
        return idx;
    }

    void freeNode(std::uint32_t idx) noexcept {
        if (idx == kInvalidIndex || idx >= pool.size()) {
            return;
        }
        pool[idx].next = freeListHead;
        freeListHead = idx;
    }

    std::optional<std::uint8_t> noteOn(int sourceChannel, int sourcePitch, int candidateOutputPitch) noexcept {
        const auto chIdx = juce::jlimit(0, 15, sourceChannel - 1);
        const auto pitchIdx = juce::jlimit(0, 127, sourcePitch);
        const auto outPitch = static_cast<std::uint8_t>(juce::jlimit(0, 127, candidateOutputPitch));

        const auto nodeIdx = allocateNode();
        if (nodeIdx == kInvalidIndex) {
            return std::nullopt;
        }
        pool[nodeIdx].outputPitch = outPitch;
        pool[nodeIdx].next = kInvalidIndex;

        auto& q = sourceQueues[chIdx][pitchIdx];
        if (q.tail == kInvalidIndex) {
            q.head = q.tail = nodeIdx;
        } else {
            pool[q.tail].next = nodeIdx;
            q.tail = nodeIdx;
        }

        outputHolders[chIdx][outPitch]++;
        return outPitch;
    }

    struct NoteOffResult {
        std::uint8_t outputPitch = 0;
        bool shouldEmit = false;
        bool matched = false;
    };

    NoteOffResult noteOff(int sourceChannel, int sourcePitch) noexcept {
        const auto chIdx = juce::jlimit(0, 15, sourceChannel - 1);
        const auto pitchIdx = juce::jlimit(0, 127, sourcePitch);
        auto& q = sourceQueues[chIdx][pitchIdx];

        if (q.head == kInvalidIndex) {
            return { 0, false, false };
        }

        const auto nodeIdx = q.head;
        q.head = pool[nodeIdx].next;
        if (q.head == kInvalidIndex) {
            q.tail = kInvalidIndex;
        }

        const auto outPitch = pool[nodeIdx].outputPitch;
        freeNode(nodeIdx);

        auto& holders = outputHolders[chIdx][outPitch];
        bool shouldEmit = false;
        if (holders > 0) {
            holders--;
            if (holders == 0) {
                shouldEmit = true;
            }
        } else {
            shouldEmit = true;
        }

        return { outPitch, shouldEmit, true };
    }
};

} // namespace devpiano::audio
