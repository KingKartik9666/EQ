#pragma once

#include <cstdint>

namespace adaptive_audio::app {

// This is a lifecycle boundary only. Tray creation, IPC, configuration, and
// engine control intentionally remain outside this initial scaffold.
enum class ControllerState : std::uint8_t {
    kCreated,
    kReady,
    kStopped,
};

class Controller final {
public:
    [[nodiscard]] bool Initialize() noexcept;
    void Stop() noexcept;

    [[nodiscard]] ControllerState state() const noexcept;

private:
    ControllerState state_{ControllerState::kCreated};
};

} // namespace adaptive_audio::app
