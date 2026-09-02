#pragma once

// DynamicSpscRingBuffer — lock-free, byte-granularity, runtime-capacity ring buffer.
//
// Design constraints (from wasapi-bridge-design.md):
//   - Exactly one producer (capture worker) and one consumer (render worker).
//   - Capacity is fixed at construction time and never changes during streaming.
//   - No heap allocation in TryWriteFrames / TryReadFrames / TryReadSilence.
//   - Atomic indices are on separate 64-byte cache lines to prevent false sharing
//     between the two real-time threads.
//   - Capacity must be a power of two to allow bitmask wrapping (no division).
//   - Frame granularity: all reads and writes are multiples of bytes_per_frame.
//
// Overflow policy (newest-drop):
//   When the ring is full, TryWriteFrames drops the incoming frames and returns
//   false. The existing data (which the render thread has not yet consumed) is
//   never overwritten. This preserves the render timeline: the audio already
//   queued plays in order. The capture thread falls behind by the dropped
//   amount; this gap will manifest as later jitter, not corrupted ordering.
//
// Underflow policy:
//   TryReadFrames returns false when insufficient frames are available. The
//   render thread is responsible for writing silence to the endpoint buffer;
//   this keeps the render deadline and avoids corrupting the ring state.

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

namespace adaptive_audio::dsp {

class DynamicSpscRingBuffer final {
public:
    // Metrics updated atomically by the workers. Readers may snapshot at any
    // time from non-real-time threads without holding a lock.
    struct Metrics final {
        std::atomic<std::uint64_t> frames_written{0U};
        std::atomic<std::uint64_t> frames_read{0U};
        std::atomic<std::uint64_t> overflow_count{0U};
        std::atomic<std::uint64_t> underflow_count{0U};
    };

    DynamicSpscRingBuffer() = default;
    ~DynamicSpscRingBuffer() = default;

    DynamicSpscRingBuffer(const DynamicSpscRingBuffer&) = delete;
    DynamicSpscRingBuffer& operator=(const DynamicSpscRingBuffer&) = delete;
    DynamicSpscRingBuffer(DynamicSpscRingBuffer&&) = delete;
    DynamicSpscRingBuffer& operator=(DynamicSpscRingBuffer&&) = delete;

    // Allocates storage. Must be called exactly once before streaming, on a
    // non-real-time thread. capacity_frames must be a power of two >= 2.
    // bytes_per_frame must be > 0. Returns false if either constraint fails or
    // allocation fails.
    [[nodiscard]] bool Initialize(std::uint32_t capacity_frames,
                                  std::uint32_t bytes_per_frame) noexcept {
        if (capacity_frames < 2U ||
            (capacity_frames & (capacity_frames - 1U)) != 0U ||
            bytes_per_frame == 0U) {
            return false;
        }

        const std::size_t byte_count =
            static_cast<std::size_t>(capacity_frames) *
            static_cast<std::size_t>(bytes_per_frame);

        try {
            storage_ = std::make_unique<std::byte[]>(byte_count);
        } catch (const std::bad_alloc&) {
            return false;
        }

        capacity_frames_ = capacity_frames;
        bytes_per_frame_ = bytes_per_frame;
        // Indices start at zero; use relaxed stores since no thread is running yet.
        write_index_.store(0U, std::memory_order_relaxed);
        read_index_.store(0U, std::memory_order_relaxed);
        return true;
    }

    // Resets the ring to empty without re-allocating. Safe to call from the
    // orchestration thread when both workers are stopped.
    void Reset() noexcept {
        write_index_.store(0U, std::memory_order_relaxed);
        read_index_.store(0U, std::memory_order_relaxed);
    }

    // Returns true if Initialize has been successfully called.
    [[nodiscard]] bool IsInitialized() const noexcept {
        return storage_ != nullptr;
    }

    // --- Producer interface (capture worker only) ---

    // Copies frame_count frames from src into the ring.
    // src must point to frame_count * bytes_per_frame bytes.
    // If the ring cannot accommodate frame_count frames, drops all frames and
    // increments metrics.overflow_count. Partial writes are not performed.
    // Returns true on success, false on overflow.
    [[nodiscard]] bool TryWriteFrames(const std::byte* src,
                                      std::uint32_t frame_count,
                                      Metrics& metrics) noexcept {
        if (frame_count == 0U) {
            return true;
        }

        const std::uint32_t write = write_index_.load(std::memory_order_relaxed);
        const std::uint32_t read  = read_index_.load(std::memory_order_acquire);
        const std::uint32_t filled = write - read; // unsigned subtraction wraps correctly
        const std::uint32_t free_frames = capacity_frames_ - filled;

        if (frame_count > free_frames) {
            metrics.overflow_count.fetch_add(1U, std::memory_order_relaxed);
            return false;
        }

        // Write frames into the ring. May wrap around.
        const std::uint32_t write_pos = write & (capacity_frames_ - 1U);
        const std::uint32_t frames_to_end =
            (capacity_frames_ - write_pos < frame_count)
                ? capacity_frames_ - write_pos
                : frame_count;

        const std::size_t bpf = bytes_per_frame_;
        std::memcpy(storage_.get() + write_pos * bpf, src,
                    frames_to_end * bpf);

        if (frames_to_end < frame_count) {
            // Wrap: copy remaining frames to the start of the ring.
            std::memcpy(storage_.get(),
                        src + frames_to_end * bpf,
                        (frame_count - frames_to_end) * bpf);
        }

        write_index_.store(write + frame_count, std::memory_order_release);
        metrics.frames_written.fetch_add(frame_count, std::memory_order_relaxed);
        return true;
    }

    // Writes frame_count zeroed frames into the ring as if they were silent
    // captured audio. Overflow semantics are identical to TryWriteFrames.
    [[nodiscard]] bool TryWriteSilentFrames(std::uint32_t frame_count,
                                            Metrics& metrics) noexcept {
        if (frame_count == 0U) {
            return true;
        }

        const std::uint32_t write = write_index_.load(std::memory_order_relaxed);
        const std::uint32_t read  = read_index_.load(std::memory_order_acquire);
        const std::uint32_t filled = write - read;
        const std::uint32_t free_frames = capacity_frames_ - filled;

        if (frame_count > free_frames) {
            metrics.overflow_count.fetch_add(1U, std::memory_order_relaxed);
            return false;
        }

        const std::uint32_t write_pos = write & (capacity_frames_ - 1U);
        const std::uint32_t frames_to_end =
            (capacity_frames_ - write_pos < frame_count)
                ? capacity_frames_ - write_pos
                : frame_count;

        const std::size_t bpf = bytes_per_frame_;
        std::memset(storage_.get() + write_pos * bpf, 0,
                    frames_to_end * bpf);

        if (frames_to_end < frame_count) {
            std::memset(storage_.get(), 0,
                        (frame_count - frames_to_end) * bpf);
        }

        write_index_.store(write + frame_count, std::memory_order_release);
        metrics.frames_written.fetch_add(frame_count, std::memory_order_relaxed);
        return true;
    }

    // --- Consumer interface (render worker only) ---

    // Copies frame_count frames from the ring into dst.
    // dst must point to frame_count * bytes_per_frame bytes.
    // Returns false (without modifying dst) if fewer than frame_count frames
    // are available. The caller is responsible for rendering silence on
    // underflow and incrementing metrics.underflow_count.
    [[nodiscard]] bool TryReadFrames(std::byte* dst,
                                     std::uint32_t frame_count,
                                     Metrics& metrics) noexcept {
        if (frame_count == 0U) {
            return true;
        }

        const std::uint32_t read  = read_index_.load(std::memory_order_relaxed);
        const std::uint32_t write = write_index_.load(std::memory_order_acquire);
        const std::uint32_t filled = write - read;

        if (frame_count > filled) {
            return false;
        }

        const std::uint32_t read_pos = read & (capacity_frames_ - 1U);
        const std::uint32_t frames_to_end =
            (capacity_frames_ - read_pos < frame_count)
                ? capacity_frames_ - read_pos
                : frame_count;

        const std::size_t bpf = bytes_per_frame_;
        std::memcpy(dst, storage_.get() + read_pos * bpf,
                    frames_to_end * bpf);

        if (frames_to_end < frame_count) {
            std::memcpy(dst + frames_to_end * bpf,
                        storage_.get(),
                        (frame_count - frames_to_end) * bpf);
        }

        read_index_.store(read + frame_count, std::memory_order_release);
        metrics.frames_read.fetch_add(frame_count, std::memory_order_relaxed);
        return true;
    }

    // Returns the number of frames currently filled (snapshot; may be stale).
    [[nodiscard]] std::uint32_t FilledFramesSnapshot() const noexcept {
        const std::uint32_t w = write_index_.load(std::memory_order_relaxed);
        const std::uint32_t r = read_index_.load(std::memory_order_relaxed);
        return w - r;
    }

    [[nodiscard]] std::uint32_t capacity_frames() const noexcept {
        return capacity_frames_;
    }

    [[nodiscard]] std::uint32_t bytes_per_frame() const noexcept {
        return bytes_per_frame_;
    }

private:
    // Cache-line separated to prevent false sharing between the capture
    // (producer) and render (consumer) threads. Each cache line is 64 bytes
    // on all supported x86-64 targets.
    alignas(64) std::atomic<std::uint32_t> write_index_{0U};
    alignas(64) std::atomic<std::uint32_t> read_index_{0U};

    std::unique_ptr<std::byte[]> storage_{};
    std::uint32_t capacity_frames_{0U};
    std::uint32_t bytes_per_frame_{0U};

    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "DynamicSpscRingBuffer requires lock-free 32-bit index atomics.");
};

} // namespace adaptive_audio::dsp
