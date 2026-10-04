#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace devpiano::audio {

template <typename T, std::size_t Capacity> class RealtimeQueue {
    static_assert(Capacity > 1);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);

public:
    bool push(const T& value) noexcept {
        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto next = (write + 1) % Capacity;
        if (next == readIndex.load(std::memory_order_acquire)) {
            return false;
        }
        entries[write] = value;
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& value) noexcept {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire)) {
            return false;
        }
        value = entries[read];
        readIndex.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }

    std::size_t snapshot(T& first, T& last) const noexcept {
        const auto read = readIndex.load(std::memory_order_relaxed);
        const auto write = writeIndex.load(std::memory_order_acquire);
        if (read == write) {
            return 0;
        }
        first = entries[read];
        last = entries[(write + Capacity - 1) % Capacity];
        return (write + Capacity - read) % Capacity;
    }

    void discardPublished() noexcept {
        readIndex.store(writeIndex.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    std::array<T, Capacity> entries {};
    std::atomic<std::size_t> writeIndex { 0 };
    std::atomic<std::size_t> readIndex { 0 };
};

} // namespace devpiano::audio
