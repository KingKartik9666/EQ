#pragma once

#include "adaptive_audio/foundation/log_severity.h"
#include "adaptive_audio/foundation/spsc_ring_buffer.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace adaptive_audio::foundation {

// A timestamp is supplied by the audio/measurement subsystem. The event sink
// intentionally does not call a clock API from the real-time callback.
enum class RealtimeEventType : std::uint16_t {
    CallbackDeadlineMissed,
    InputUnderrun,
    OutputUnderrun,
    OutputOverrun,
    ProcessingFallbackEnabled,
    ProcessingFault,
    DeviceRecoveryRequested,
};

// Fixed-size event data. Do not add strings, containers, virtual functions, or
// ownership here: this packet is the only logging/telemetry handoff intended
// for the real-time audio callback.
struct RealtimeEvent final {
    RealtimeEventType type{RealtimeEventType::ProcessingFault};
    LogSeverity severity{LogSeverity::Warning};
    std::uint16_t source_id{0U};
    std::uint32_t code{0U};
    std::uint64_t monotonic_timestamp_ticks{0U};
    std::int64_t value_a{0};
    std::int64_t value_b{0};
};

static_assert(std::is_trivially_copyable_v<RealtimeEvent>);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "Realtime event accounting requires lock-free 64-bit atomics.");

// 1,023 usable events is deliberately bounded. A full queue drops an event
// instead of blocking the audio callback. Capacity should be revisited using
// measured burst behavior, not expanded speculatively.
inline constexpr std::size_t kRealtimeEventQueueSlots{1024U};
using RealtimeEventQueue = SpscRingBuffer<RealtimeEvent, kRealtimeEventQueueSlots>;

struct RealtimeEventMetrics final {
    std::atomic<std::uint64_t> published{0U};
    std::atomic<std::uint64_t> dropped{0U};
};

// This is the real-time-facing boundary. TryPublish performs only fixed-size
// copies and lock-free atomics; it never owns or references an AsyncLogger,
// filesystem object, mutex, condition variable, or callback.
class RealtimeEventSink final {
public:
    RealtimeEventSink(RealtimeEventQueue& queue, RealtimeEventMetrics& metrics) noexcept
        : queue_(queue), metrics_(metrics) {}

    [[nodiscard]] bool TryPublish(const RealtimeEvent& event) noexcept {
        if (!queue_.TryPush(event)) {
            metrics_.dropped.fetch_add(1U, std::memory_order_relaxed);
            return false;
        }

        metrics_.published.fetch_add(1U, std::memory_order_relaxed);
        return true;
    }

private:
    RealtimeEventQueue& queue_;
    RealtimeEventMetrics& metrics_;
};

} // namespace adaptive_audio::foundation
