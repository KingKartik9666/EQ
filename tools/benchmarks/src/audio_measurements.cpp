#include "adaptive_audio/benchmarks/audio_measurements.h"

#include <algorithm>

namespace adaptive_audio::benchmarks {

void AudioMeasurementAccumulator::SetObservedStreamFormat(const std::uint32_t sample_rate_hz,
                                                           const std::uint16_t channel_count,
                                                           const std::uint32_t buffer_frames) noexcept {
    const std::lock_guard lock(observation_mutex_);
    stream_format_ = {
        .observed = true,
        .sample_rate_hz = sample_rate_hz,
        .channel_count = channel_count,
        .buffer_frames = buffer_frames,
    };
}

void AudioMeasurementAccumulator::SetCpuUsageSample(
    const std::uint64_t process_cpu_time_delta_ns,
    const std::uint64_t wall_time_delta_ns) noexcept {
    const std::lock_guard lock(observation_mutex_);
    cpu_usage_ = {
        .observed = wall_time_delta_ns != 0U,
        .process_cpu_time_delta_ns = process_cpu_time_delta_ns,
        .wall_time_delta_ns = wall_time_delta_ns,
    };
}

void AudioMeasurementAccumulator::SetEstimatedProcessingLatencyFrames(
    const std::uint64_t frames) noexcept {
    const std::lock_guard lock(observation_mutex_);
    estimated_processing_latency_observed_ = true;
    estimated_processing_latency_frames_ = frames;
}

void AudioMeasurementAccumulator::RecordCallbackDurationNanoseconds(
    const std::uint64_t duration_ns) noexcept {
    callback_count_.fetch_add(1U, std::memory_order_relaxed);
    callback_duration_total_ns_.fetch_add(duration_ns, std::memory_order_relaxed);

    // The audio callback is the sole writer of this accumulator. Readers only
    // load it, so a conditional store records the true maximum without a CAS
    // retry loop in the real-time path.
    const std::uint64_t prior_max = worst_callback_duration_ns_.load(std::memory_order_relaxed);
    if (duration_ns > prior_max) {
        worst_callback_duration_ns_.store(duration_ns, std::memory_order_relaxed);
    }
}

void AudioMeasurementAccumulator::RecordUnderrun() noexcept {
    underrun_count_.fetch_add(1U, std::memory_order_relaxed);
}

void AudioMeasurementAccumulator::RecordOverrun() noexcept {
    overrun_count_.fetch_add(1U, std::memory_order_relaxed);
}

void AudioMeasurementAccumulator::RecordSchedulingJitterNanoseconds(
    const std::uint64_t jitter_ns) noexcept {
    const std::uint64_t prior_max =
        worst_scheduling_jitter_ns_.load(std::memory_order_relaxed);
    if (jitter_ns > prior_max) {
        worst_scheduling_jitter_ns_.store(jitter_ns, std::memory_order_relaxed);
    }
}

AudioMeasurementSnapshot AudioMeasurementAccumulator::Snapshot() const noexcept {
    AudioMeasurementSnapshot snapshot{};
    {
        const std::lock_guard lock(observation_mutex_);
        snapshot.stream_format = stream_format_;
        snapshot.cpu_usage = cpu_usage_;
        snapshot.estimated_processing_latency_observed =
            estimated_processing_latency_observed_;
        snapshot.estimated_processing_latency_frames = estimated_processing_latency_frames_;
    }

    snapshot.callback_count = callback_count_.load(std::memory_order_relaxed);
    snapshot.callback_duration_total_ns =
        callback_duration_total_ns_.load(std::memory_order_relaxed);
    snapshot.worst_callback_duration_ns =
        worst_callback_duration_ns_.load(std::memory_order_relaxed);
    snapshot.underrun_count = underrun_count_.load(std::memory_order_relaxed);
    snapshot.overrun_count = overrun_count_.load(std::memory_order_relaxed);
    snapshot.worst_scheduling_jitter_ns =
        worst_scheduling_jitter_ns_.load(std::memory_order_relaxed);
    return snapshot;
}

} // namespace adaptive_audio::benchmarks
