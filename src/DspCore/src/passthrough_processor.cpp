#include "adaptive_audio/dsp/passthrough_processor.h"

#include <cstring>
#include <limits>

namespace adaptive_audio::dsp {

bool PcmPassthroughProcessor::Configure(const AudioFormat format,
                                        const std::uint32_t maximum_frames_per_callback) noexcept {
    if (!format.IsSupported() || maximum_frames_per_callback == 0U) {
        return false;
    }

    const std::size_t maximum_bytes = static_cast<std::size_t>(format.bytes_per_frame) *
                                      static_cast<std::size_t>(maximum_frames_per_callback);
    if (maximum_bytes > std::numeric_limits<std::size_t>::max() / 2U) {
        return false;
    }

    format_ = format;
    maximum_frames_per_callback_ = maximum_frames_per_callback;
    configured_ = true;
    return true;
}

bool PcmPassthroughProcessor::Process(const std::byte* const input, std::byte* const output,
                                      const std::uint32_t frame_count,
                                      PassthroughMetrics& metrics) const noexcept {
    metrics.process_calls.fetch_add(1U, std::memory_order_relaxed);
    if (!configured_ || frame_count > maximum_frames_per_callback_ ||
        (frame_count != 0U && (input == nullptr || output == nullptr))) {
        metrics.rejected_calls.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }

    const std::size_t byte_count = static_cast<std::size_t>(frame_count) *
                                   static_cast<std::size_t>(format_.bytes_per_frame);
    std::memmove(output, input, byte_count);
    metrics.processed_frames.fetch_add(frame_count, std::memory_order_relaxed);
    return true;
}

bool PcmPassthroughProcessor::IsConfigured() const noexcept {
    return configured_;
}

const AudioFormat& PcmPassthroughProcessor::Format() const noexcept {
    return format_;
}

} // namespace adaptive_audio::dsp
