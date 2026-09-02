#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace adaptive_audio::windows {

struct RenderEndpointInfo final {
    std::wstring device_id;
    std::wstring friendly_name;
    std::uint32_t sample_rate_hz{};
    std::uint16_t channel_count{};
    std::uint16_t bits_per_sample{};
    std::uint32_t channel_mask{};
    bool float_pcm{false};
};

// Discovery/control-plane code only. It may allocate and use COM; do not call
// it from a real-time audio callback.
[[nodiscard]] long EnumerateActiveRenderEndpoints(
    std::vector<RenderEndpointInfo>& endpoints) noexcept;

} // namespace adaptive_audio::windows
