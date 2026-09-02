#pragma once

#include "adaptive_audio/dsp/dynamic_spsc_ring_buffer.h"
#include "adaptive_audio/foundation/realtime_event.h"
#include "adaptive_audio/windows/wasapi_worker_types.h"

#include <cstdint>

namespace adaptive_audio::windows {

struct CaptureWorkerConfig final {
    // Both handles are created by the orchestration thread and outlive Run().
    // HANDLE typed as void* to avoid including <windows.h> in this header.
    void* wasapi_event{nullptr};
    void* shutdown_event{nullptr};
    // Ring buffer shared with the render worker; owned by orchestration.
    adaptive_audio::dsp::DynamicSpscRingBuffer* ring{nullptr};
    adaptive_audio::dsp::DynamicSpscRingBuffer::Metrics* ring_metrics{nullptr};
    // RT event sink; may be null if diagnostics are disabled.
    adaptive_audio::foundation::RealtimeEventSink* event_sink{nullptr};
    std::uint16_t source_id{0U};
};

// Blocking. Call from a dedicated std::thread after IAudioClient::Start()
// and SetEventHandle() have been called by the orchestration thread.
// The worker's only job is to pull frames from IAudioCaptureClient and push
// them into the ring buffer. It does not call Start/Stop.
//
// audio_capture_client is IAudioCaptureClient*; typed as void* to keep this
// header COM-free. The implementation casts and uses it correctly.
[[nodiscard]] WorkerExitReason CaptureWorkerRun(
    void* audio_capture_client,
    const CaptureWorkerConfig& config) noexcept;

} // namespace adaptive_audio::windows
