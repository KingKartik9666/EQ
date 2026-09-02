#pragma once

#include <cstdint>

namespace adaptive_audio::inference {

// A deliberately inert boundary for the future non-real-time inference owner.
// It has no model format, runtime dependency, feature contract, or DSP-control
// contract yet. Those decisions require separate benchmarks and validation.
enum class RuntimeState : std::uint8_t {
    kNotBuilt,
};

struct RuntimeCapabilities final {
    RuntimeState state{RuntimeState::kNotBuilt};
    bool model_loading_supported{false};
    bool inference_supported{false};
    bool callable_from_realtime_audio_thread{false};
};

class InferenceRuntime final {
public:
    [[nodiscard]] RuntimeCapabilities capabilities() const noexcept;
};

} // namespace adaptive_audio::inference
