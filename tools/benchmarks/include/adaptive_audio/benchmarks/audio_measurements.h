#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace adaptive_audio::benchmarks {

// Values are copied from the negotiated live stream by a device-management
// thread. They do not represent a requested or assumed device configuration.
struct StreamFormatObservation final {
    bool observed{false};
    std::uint32_t sample_rate_hz{};
    std::uint16_t channel_count{};
    std::uint32_t buffer_frames{};
};

// CPU utilization requires a CPU-time delta and wall-time delta from the same
// sampling interval. This structure intentionally retains both raw values.
struct CpuUsageObservation final {
    bool observed{false};
    std::uint64_t process_cpu_time_delta_ns{};
    std::uint64_t wall_time_delta_ns{};
};

struct AudioMeasurementSnapshot final {
    StreamFormatObservation stream_format{};
    CpuUsageObservation cpu_usage{};

    std::uint64_t callback_count{};
    std::uint64_t callback_duration_total_ns{};
    std::uint64_t worst_callback_duration_ns{};
    std::uint64_t underrun_count{};
    std::uint64_t overrun_count{};
    std::uint64_t worst_scheduling_jitter_ns{};

    bool estimated_processing_latency_observed{false};
    std::uint64_t estimated_processing_latency_frames{};
};

// The Record* methods below are for one callback writer and use only lock-free
// 64-bit atomics. They take durations measured by the caller; this type never
// queries a clock or emits a log message from the real-time path.
class AudioMeasurementAccumulator final {
public:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Audio callback metrics require lock-free 64-bit atomics.");

    // Device/control-plane methods. Never call these from an audio callback.
    void SetObservedStreamFormat(std::uint32_t sample_rate_hz,
                                 std::uint16_t channel_count,
                                 std::uint32_t buffer_frames) noexcept;
    void SetCpuUsageSample(std::uint64_t process_cpu_time_delta_ns,
                           std::uint64_t wall_time_delta_ns) noexcept;
    void SetEstimatedProcessingLatencyFrames(std::uint64_t frames) noexcept;

    // Real-time callback methods. They do not allocate, lock, log, or wait.
    void RecordCallbackDurationNanoseconds(std::uint64_t duration_ns) noexcept;
    void RecordUnderrun() noexcept;
    void RecordOverrun() noexcept;
    void RecordSchedulingJitterNanoseconds(std::uint64_t jitter_ns) noexcept;

    // Monitoring/logging only. This method may acquire a mutex to copy
    // control-plane observations and must not be called from the callback.
    [[nodiscard]] AudioMeasurementSnapshot Snapshot() const noexcept;

private:
    mutable std::mutex observation_mutex_;
    StreamFormatObservation stream_format_{};
    CpuUsageObservation cpu_usage_{};
    bool estimated_processing_latency_observed_{false};
    std::uint64_t estimated_processing_latency_frames_{};

    std::atomic<std::uint64_t> callback_count_{0U};
    std::atomic<std::uint64_t> callback_duration_total_ns_{0U};
    std::atomic<std::uint64_t> worst_callback_duration_ns_{0U};
    std::atomic<std::uint64_t> underrun_count_{0U};
    std::atomic<std::uint64_t> overrun_count_{0U};
    std::atomic<std::uint64_t> worst_scheduling_jitter_ns_{0U};
};

} // namespace adaptive_audio::benchmarks
