#pragma once

#include <cstdint>

namespace adaptive_audio::audio_service {

// AudioService.exe is a per-user process, despite its historical-looking name.
// It is deliberately not registered as a Windows Session-0 service.
struct PerUserHostStatus final {
    std::uint32_t session_id{};
    bool current_session_resolved{false};
    bool is_session_zero{false};
    bool controlled_audio_path_active{false};
};

class PerUserAudioHost final {
public:
    // Setup/control-plane operation only. It does not create WASAPI clients or
    // start any PCM path in this scaffold.
    [[nodiscard]] bool InitializeForCurrentProcess() noexcept;

    [[nodiscard]] const PerUserHostStatus& status() const noexcept;

private:
    PerUserHostStatus status_{};
};

} // namespace adaptive_audio::audio_service
