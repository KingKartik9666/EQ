#include "adaptive_audio/app/controller.h"

namespace adaptive_audio::app {

bool Controller::Initialize() noexcept {
    // No tray icon or engine IPC exists in Phase 1. Keeping this state machine
    // small makes its future replacement explicit rather than simulating UI.
    state_ = ControllerState::kReady;
    return true;
}

void Controller::Stop() noexcept {
    state_ = ControllerState::kStopped;
}

ControllerState Controller::state() const noexcept {
    return state_;
}

} // namespace adaptive_audio::app
