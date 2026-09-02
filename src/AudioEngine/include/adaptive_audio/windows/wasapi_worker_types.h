#pragma once

// wasapi_worker_types.h — shared types for capture and render WASAPI workers.

#include <cstdint>

namespace adaptive_audio::windows {

// Why both workers are in separate threads: WASAPI capture and render endpoints
// run on independent hardware clocks. A single WaitForMultipleObjects thread
// risks one endpoint's delay stalling the other. Dual threads decouple the
// hardware clocks as specified in wasapi-bridge-design.md.

enum class WorkerExitReason : std::uint8_t {
    kShutdownRequested,  // Orchestration thread requested clean stop.
    kDeviceInvalidated,  // AUDCLNT_E_DEVICE_INVALIDATED received.
    kUnexpectedError,    // Any other HRESULT failure.
};

} // namespace adaptive_audio::windows
