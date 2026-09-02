# WASAPI Bridge Technical Design

This document specifies the concrete architecture for the "Transparent PCM audio-path prototype" described in `audio-path-prototype.md`. 

## 1. Threading Architecture: Dual RT Threads
While `audio-path-prototype.md` refers to an "event-driven worker", we explicitly split this into **two independent real-time threads**:
*   **Capture Worker Thread:** Waits exclusively on the capture WASAPI event.
*   **Render Worker Thread:** Waits exclusively on the render WASAPI event.

**Why:** WASAPI capture and render endpoints rely on physically separate hardware clocks (especially in loopback-to-render scenarios across different devices). A single thread using `WaitForMultipleObjects` risks one endpoint's delay stalling the other's real-time deadline, or missing events if clocks drift aggressively. Decoupling them into two threads accurately models the hardware reality.

Both threads must attempt MMCSS registration (`"Pro Audio"`).

## 2. Cross-Thread Communication: DynamicSpscRingBuffer
The threads are bridged by a dynamically sized, lock-free Single-Producer-Single-Consumer (SPSC) ring buffer.

*   **Existing Limitation:** `SpscRingBuffer<T, SlotCount>` in `DspCore` is compile-time fixed, which cannot accommodate runtime-negotiated PCM frame sizes.
*   **New Component:** A `DynamicSpscRingBuffer` must be created. It operates on byte-granularity to hold arbitrary interleaved PCM frames. Capacity is fixed at allocation (prior to `IAudioClient::Start`).
*   **Cache Safety:** The atomic `read_index` and `write_index` MUST be forced to separate cache lines using `alignas(64)` to prevent false sharing cache thrashing between the Capture and Render threads.

## 3. Failure & Drift Policy
*   **Clock Drift Overflow:** When Capture attempts to write to a full ring buffer, it must drop the *newest* captured frames. It must not overwrite the unread tail, preserving the timeline of existing data. Increment `overflow_count`.
*   **Clock Drift Underflow:** When Render attempts to read from an empty/insufficient ring buffer, it must write silence (zeros) for the missing frames. Increment `underflow_count`.
*   **Silent Flags:** If `IAudioCaptureClient::GetBuffer` returns `AUDCLNT_BUFFERFLAGS_SILENT`, the Capture thread must explicitly push zeroed frames into the ring buffer instead of reading the uninitialized pointer.
*   **Device Loss:** If either thread encounters `AUDCLNT_E_DEVICE_INVALIDATED`, it must instantly exit its loop and signal the orchestration thread without crashing.

## 4. Initialization & Safety
*   The orchestration thread (non-RT) must verify that `source_id != destination_id` before starting, strictly preventing recursive loopback feedback.
*   Both endpoints must exactly match `WAVEFORMATEX` (rate, channels, bit depth, float/int).
*   `PcmPassthroughProcessor` must be used for the strictly bounded byte-for-byte copies into and out of the ring buffer.

## Handoff to Implementation Agent
The implementation agent should sequentially implement:
1.  `DynamicSpscRingBuffer` (in `DspCore` or a new utility namespace) with `alignas(64)` atomics and tests.
2.  `WasapiCaptureWorker` and `WasapiRenderWorker` classes that wrap `IAudioClient` and their respective events.
3.  The orchestration logic in `AudioService` that wires the two workers and the ring buffer together, enforces format matching, and blocks recursive loopback.
