# AdaptiveAudio

AdaptiveAudio is a Windows real-time adaptive audio enhancer under construction. Its intended V1 route is normal Windows shared-mode playback through a virtual render endpoint, then through a per-user audio engine to the selected physical output. It is not intended to claim interception of exclusive-mode WASAPI, ASIO, or vendor-specific bypass paths.

The initial repository deliberately builds the measured, deterministic pieces before the driver or ML stack:

- `adaptive_audio_dsp`: allocation-free, interleaved float PCM EQ core; a non-real-time coordinator turns bounded AI intent into precomputed DSP control packets.
- `AudioEngine.exe --list-render-endpoints`: Windows endpoint discovery, separate from the real-time path.
- `dsp_core_tests`: deterministic tests for control bounds, lock-free SPSC delivery, channel-independent processing, and malformed-control rejection.

There is no virtual audio driver or ONNX inference integration yet. Both require hardware-specific validation; the driver also requires the WDK, test signing during development, and an installation/rollback plan. See [architecture.md](docs/architecture.md) and [driver-prototype-plan.md](docs/driver-prototype-plan.md).

## Build prerequisites

- Windows 10/11 with a current Windows SDK
- Visual Studio 2022 Build Tools or Visual Studio with the C++ desktop workload
- CMake 3.24 or later

The checked environment has no CMake or MSVC toolchain installed, so this repository has not been compiled here yet.

## Build

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64
cmake --build out/build --config RelWithDebInfo
ctest --test-dir out/build -C RelWithDebInfo --output-on-failure
.\out\build\src\AudioEngine\RelWithDebInfo\AudioEngine.exe --list-render-endpoints
```

Endpoint discovery runs outside the real-time audio path. It is a discovery tool only; it does not start capture, render, routing, or audio processing.

## Current safety boundaries

- `ProcessInterleaved` does not allocate, lock, log, perform I/O, call the model, or query Windows.
- The process path accepts only precomputed control packets from a fixed-capacity single-producer/single-consumer queue.
- EQ is identical and independent per channel; it does not downmix, reorder, or crossfeed channels.
- Model-originated gain intents are finite-checked, clamped, rate-limited, and converted to biquad coefficients before reaching the real-time thread.
- If no valid control packet arrives, the processor remains in its last valid state; startup is neutral EQ.

These are code-level boundaries, not a latency or production-readiness claim. Latency, device behavior, and driver viability must be measured on actual target machines.
