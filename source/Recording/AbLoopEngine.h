#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace devpiano::recording {

struct AbLoopRange {
    std::int64_t startSamples = 0;
    std::int64_t endSamples = 0;
    bool hasStart = false;
    bool hasEnd = false;

    [[nodiscard]] bool isValid() const noexcept {
        return hasStart && hasEnd && startSamples >= 0 && endSamples > startSamples;
    }
};

class AbLoopEngine final {
public:
    void setStartSample(std::int64_t sample) noexcept {
        const auto writerVersion = lockForWrite();
        startSamples.store(std::max<std::int64_t>(sample, 0), std::memory_order_relaxed);
        hasStart.store(true, std::memory_order_relaxed);
        sequence.store(writerVersion + 1, std::memory_order_release);
    }

    void setEndSample(std::int64_t sample) noexcept {
        const auto writerVersion = lockForWrite();
        endSamples.store(std::max<std::int64_t>(sample, 0), std::memory_order_relaxed);
        hasEnd.store(true, std::memory_order_relaxed);
        sequence.store(writerVersion + 1, std::memory_order_release);
    }

    void clear() noexcept {
        const auto writerVersion = lockForWrite();
        startSamples.store(0, std::memory_order_relaxed);
        endSamples.store(0, std::memory_order_relaxed);
        hasStart.store(false, std::memory_order_relaxed);
        hasEnd.store(false, std::memory_order_relaxed);
        sequence.store(writerVersion + 1, std::memory_order_release);
    }

    [[nodiscard]] AbLoopRange getRange() const noexcept {
        const auto observedVersion = sequence.load(std::memory_order_acquire);
        if ((observedVersion & 1U) != 0U) {
            return {};
        }

        const AbLoopRange range { startSamples.load(std::memory_order_relaxed),
                                  endSamples.load(std::memory_order_relaxed), hasStart.load(std::memory_order_relaxed),
                                  hasEnd.load(std::memory_order_relaxed) };
        if (sequence.load(std::memory_order_acquire) != observedVersion) {
            return {};
        }

        return range;
    }

private:
    [[nodiscard]] std::uint32_t lockForWrite() noexcept {
        auto observedVersion = sequence.load(std::memory_order_relaxed);
        for (;;) {
            if ((observedVersion & 1U) != 0U) {
                observedVersion = sequence.load(std::memory_order_relaxed);
                continue;
            }
            if (sequence.compare_exchange_weak(observedVersion, observedVersion + 1, std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
                return observedVersion + 1;
            }
        }
    }

    std::atomic<std::uint32_t> sequence { 0 };
    std::atomic<std::int64_t> startSamples { 0 };
    std::atomic<std::int64_t> endSamples { 0 };
    std::atomic_bool hasStart { false };
    std::atomic_bool hasEnd { false };
};

} // namespace devpiano::recording
