#pragma once

#include "adaptive_audio/dsp/dynamic_spsc_ring_buffer.h"
#include "adaptive_audio/foundation/realtime_event.h"
#include "adaptive_audio/windows/wasapi_worker_types.h"

#include <cstdint>

namespace adaptive_audio::windows {

struct RenderWorkerConfig final {
    void* wasapi_event{nullptr};
    void* shutdown_event{nullptr};
    adaptive_audio::dsp::DynamicSpscRingBuffer* ring{nullptr};
    adaptive_audio::dsp::DynamicSpscRingBuffer::Metrics* ring_metrics{nullptr};
    adaptive_audio::foundation::RealtimeEventSink* event_sink{nullptr};
    std::uint16_t source_id{0U};
};

// Blocking. Call from a dedicated std::thread after IAudioClient::Start()
// and SetEventHandle() have been called by the orchestration thread.
//
// audio_render_client is IAudioRenderClient*; typed as void* to keep this
// header COM-free.
//
// render_buffer_size_frames is the value returned by IAudioClient::GetBufferSize.
[[nodiscard]] WorkerExitReason RenderWorkerRun(
    void* audio_render_client,
    void* audio_client_for_padding,    // IAudioClient* for GetCurrentPadding
    std::uint32_t render_buffer_size_frames,
    const RenderWorkerConfig& config) noexcept;

} // namespace adaptive_audio::windows
