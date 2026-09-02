#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace adaptive_audio::foundation {

// Fixed-capacity queue for exactly one producer and one distinct consumer.
// It uses one sentinel slot, so Capacity() is SlotCount - 1. Both operations
// are bounded and allocation-free. The queue is appropriate for a real-time
// producer only when T is a fixed-size, trivially copyable packet.
template <typename T, std::size_t SlotCount>
class SpscRingBuffer final {
    static_assert(SlotCount >= 2U, "A ring buffer needs an empty sentinel slot.");
    static_assert((SlotCount & (SlotCount - 1U)) == 0U,
                  "The real-time queue uses a power-of-two slot count.");
    static_assert(std::is_trivially_copyable_v<T>,
                  "The real-time queue carries trivially copyable packets only.");
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "The real-time queue requires lock-free index atomics.");

public:
    [[nodiscard]] bool TryPush(const T& value) noexcept {
        const std::size_t write = write_index_.load(std::memory_order_relaxed);
        const std::size_t next_write = Next(write);
        if (next_write == read_index_.load(std::memory_order_acquire)) {
            return false;
        }

        slots_[write] = value;
        write_index_.store(next_write, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool TryPop(T& value) noexcept {
        const std::size_t read = read_index_.load(std::memory_order_relaxed);
        if (read == write_index_.load(std::memory_order_acquire)) {
            return false;
        }

        value = slots_[read];
        read_index_.store(Next(read), std::memory_order_release);
        return true;
    }

    [[nodiscard]] static constexpr std::size_t Capacity() noexcept {
        return SlotCount - 1U;
    }

private:
    [[nodiscard]] static constexpr std::size_t Next(const std::size_t index) noexcept {
        return (index + 1U) & (SlotCount - 1U);
    }

    std::array<T, SlotCount> slots_{};
    alignas(64) std::atomic<std::size_t> write_index_{0U};
    alignas(64) std::atomic<std::size_t> read_index_{0U};
};

} // namespace adaptive_audio::foundation
