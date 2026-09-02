#pragma once

#include <cstdint>

namespace adaptive_audio::dsp {

// Metadata travels with the PCM stream. The DSP core never changes this layout.
struct AudioFormat final {
    std::uint32_t sample_rate_hz{};
    std::uint16_t channel_count{};
    std::uint32_t channel_mask{};
    std::uint16_t bytes_per_frame{};

    [[nodiscard]] constexpr bool IsSupported() const noexcept {
        return sample_rate_hz >= 32'000U && sample_rate_hz <= 192'000U &&
               channel_count >= 1U && channel_count <= 8U && bytes_per_frame >= channel_count;
    }
};

} // namespace adaptive_audio::dsp
