// wasapi_capture_worker.cpp
//
// Real-time contract (ENGINEERING.md §"Real-time contract"):
//   The inner loop MUST NOT: allocate, deallocate, lock, perform I/O, call
//   any logging API, or wait on anything other than the two event handles.
//   All failure paths use bounded lock-free atomics only.
//
// AUDCLNT_BUFFERFLAGS_SILENT (wasapi-bridge-design.md §3):
//   If the Windows audio engine reports a silent packet, we must not read the
//   uninitialized data pointer. We push zeroed frames instead.

#include "adaptive_audio/windows/wasapi_capture_worker.h"

// <windows.h> must come first on MSVC.
#include <windows.h>

#include <audioclient.h>
#include <avrt.h>

#include <cstring>

namespace adaptive_audio::windows {

namespace {

// Maximum frames we will pull in one GetBuffer call. We preallocate a scratch
// block on the stack; 48000 Hz × 0.1 s × 8 channels × 4 bytes = 153 600 bytes.
// Keep this bounded and stack-friendly: cap at 48 kHz × 100 ms × 8ch × 4B.
// The worker processes available frames in one shot; WASAPI will not give us
// more than the buffer size which is typically 10–100 ms.
//
// NOTE: We do NOT preallocate a heap scratch buffer here. The WASAPI capture
// API supplies a pointer directly into its internal buffer; we copy from that
// pointer into the ring. No intermediate staging buffer is needed.

// Publishes a realtime event if the sink is available. Drops silently if the
// queue is full — dropping telemetry is preferred to blocking audio.
void TryPublishEvent(adaptive_audio::foundation::RealtimeEventSink* sink,
                     adaptive_audio::foundation::RealtimeEventType type,
                     adaptive_audio::foundation::LogSeverity severity,
                     std::uint16_t source_id,
                     std::uint64_t timestamp_ticks,
                     std::int64_t value_a,
                     std::int64_t value_b) noexcept {
    if (sink == nullptr) {
        return;
    }
    adaptive_audio::foundation::RealtimeEvent ev{};
    ev.type                     = type;
    ev.severity                 = severity;
    ev.source_id                = source_id;
    ev.code                     = 0U;
    ev.monotonic_timestamp_ticks = timestamp_ticks;
    ev.value_a                  = value_a;
    ev.value_b                  = value_b;
    sink->TryPublish(ev); // nonblocking; drop on full queue
}

[[nodiscard]] std::uint64_t QueryTicks() noexcept {
    LARGE_INTEGER li{};
    ::QueryPerformanceCounter(&li);
    return static_cast<std::uint64_t>(li.QuadPart);
}

} // namespace

WorkerExitReason CaptureWorkerRun(void* audio_capture_client_ptr,
                                   const CaptureWorkerConfig& config) noexcept {
    auto* const capture_client =
        static_cast<IAudioCaptureClient*>(audio_capture_client_ptr);

    // Attempt MMCSS registration. Failure is recorded but does not abort.
    DWORD task_index{0U};
    HANDLE mmcss_handle =
        ::AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
    // mmcss_handle may be null; that is an observation, not a fatal error.

    const HANDLE handles[2] = {
        static_cast<HANDLE>(config.shutdown_event),
        static_cast<HANDLE>(config.wasapi_event),
    };

    WorkerExitReason exit_reason = WorkerExitReason::kShutdownRequested;

    for (;;) {
        const DWORD wait_result =
            ::WaitForMultipleObjects(2U, handles, FALSE, INFINITE);

        if (wait_result == WAIT_OBJECT_0) {
            // Shutdown event: clean exit.
            exit_reason = WorkerExitReason::kShutdownRequested;
            break;
        }

        if (wait_result != WAIT_OBJECT_0 + 1U) {
            // Unexpected wait failure.
            exit_reason = WorkerExitReason::kUnexpectedError;
            break;
        }

        // --- Capture event fired: drain all available packets. ---
        // WASAPI may have accumulated more than one buffer period worth of
        // frames. We drain completely to avoid falling behind.
        for (;;) {
            UINT32 frames_available{0U};
            HRESULT hr = capture_client->GetNextPacketSize(&frames_available);

            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                TryPublishEvent(config.event_sink,
                                adaptive_audio::foundation::RealtimeEventType::DeviceRecoveryRequested,
                                adaptive_audio::foundation::LogSeverity::Warning,
                                config.source_id, QueryTicks(), 0, 0);
                exit_reason = WorkerExitReason::kDeviceInvalidated;
                goto exit_loop; // NOLINT(cppcoreguidelines-avoid-goto)
            }
            if (FAILED(hr) || frames_available == 0U) {
                break; // No more packets this event.
            }

            BYTE* data_ptr{nullptr};
            UINT32 frames_to_read{0U};
            DWORD flags{0U};
            UINT64 device_position{0U};
            UINT64 qpc_position{0U};

            hr = capture_client->GetBuffer(&data_ptr, &frames_to_read,
                                           &flags, &device_position,
                                           &qpc_position);

            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                TryPublishEvent(config.event_sink,
                                adaptive_audio::foundation::RealtimeEventType::DeviceRecoveryRequested,
                                adaptive_audio::foundation::LogSeverity::Warning,
                                config.source_id, QueryTicks(), 0, 0);
                exit_reason = WorkerExitReason::kDeviceInvalidated;
                goto exit_loop;
            }
            if (FAILED(hr)) {
                exit_reason = WorkerExitReason::kUnexpectedError;
                goto exit_loop;
            }

            // Push frames into the ring. If AUDCLNT_BUFFERFLAGS_SILENT, push
            // zeroed frames without reading data_ptr (which may be null or
            // contain undefined data per the WASAPI specification).
            const bool is_silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0U;
            bool wrote_ok;
            if (is_silent) {
                wrote_ok = config.ring->TryWriteSilentFrames(
                    frames_to_read, *config.ring_metrics);
            } else {
                wrote_ok = config.ring->TryWriteFrames(
                    reinterpret_cast<const std::byte*>(data_ptr),
                    frames_to_read, *config.ring_metrics);
            }

            if (!wrote_ok) {
                // Ring overflow: dropped above in TryWriteFrames/TryWriteSilentFrames.
                TryPublishEvent(config.event_sink,
                                adaptive_audio::foundation::RealtimeEventType::OutputOverrun,
                                adaptive_audio::foundation::LogSeverity::Warning,
                                config.source_id, qpc_position,
                                static_cast<std::int64_t>(frames_to_read), 0);
            }

            // ALWAYS release the buffer, even on overflow. The GetBuffer/
            // ReleaseBuffer pair must be called on the same thread and must
            // be balanced.
            hr = capture_client->ReleaseBuffer(frames_to_read);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                exit_reason = WorkerExitReason::kDeviceInvalidated;
                goto exit_loop;
            }
            if (FAILED(hr)) {
                exit_reason = WorkerExitReason::kUnexpectedError;
                goto exit_loop;
            }
        }
    }

exit_loop:
    if (mmcss_handle != nullptr) {
        ::AvRevertMmThreadCharacteristics(mmcss_handle);
    }
    return exit_reason;
}

} // namespace adaptive_audio::windows
