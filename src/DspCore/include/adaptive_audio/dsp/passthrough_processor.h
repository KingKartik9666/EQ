#pragma once

#include "adaptive_audio/dsp/audio_format.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace adaptive_audio::dsp {

// The only DSP permitted in the foundation phase: a bounded byte-for-byte copy
// of complete PCM frames. It owns no buffers and never changes format metadata.
struct PassthroughMetrics final {
    std::atomic<std::uint64_t> process_calls{0U};
    std::atomic<std::uint64_t> processed_frames{0U};
    std::atomic<std::uint64_t> rejected_calls{0U};
};

class PcmPassthroughProcessor final {
public:
    [[nodiscard]] bool Configure(AudioFormat format,
                                 std::uint32_t maximum_frames_per_callback) noexcept;

    // Safe for in-place use. The caller owns preallocated input and output
    // storage. Frame counts above the configured bound fail without copying.
    [[nodiscard]] bool Process(const std::byte* input, std::byte* output,
                               std::uint32_t frame_count,
                               PassthroughMetrics& metrics) const noexcept;

    [[nodiscard]] bool IsConfigured() const noexcept;
    [[nodiscard]] const AudioFormat& Format() const noexcept;

private:
    AudioFormat format_{};
    std::uint32_t maximum_frames_per_callback_{0U};
    bool configured_{false};
};

} // namespace adaptive_audio::dsp
