# Architecture baseline

## Scope and evidence level

This is the target architecture for the first end-to-end prototype. Only the DSP core and endpoint discovery are implemented today. The virtual endpoint, physical-device routing, timing behaviour, and latency budget are hypotheses to be validated with a WDK test-signed prototype and measured hardware; they are not shipped capabilities.

Microsoft's SYSVAD sample is the starting reference for a virtual WaveRT audio device, not production code to ship unchanged. The sample itself does not provide the application-to-physical-device routing required by this product. A custom data path and clock/underflow policy must be designed and tested. Microsoft also documents APOs as driver-associated, in-process real-time components; that makes an APO an unsuitable host for model loading or general ONNX inference. See the [SYSVAD sample documentation](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/) and [APO implementation requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects).

## V1 data plane

```text
Shared-mode application render stream
  -> Windows audio engine
  -> AdaptiveAudio virtual render endpoint (WaveRT/SYSVAD-derived driver)
  -> paired virtual capture stream or equivalent driver-to-user data path
  -> AdaptiveAudioEngine.exe (per-user background process)
       event-driven WASAPI capture
       -> bounded frame/clock bridge
       -> real-time PCM/DSP callback
       -> event-driven WASAPI render
  -> selected physical render endpoint
```

The virtual driver provides the routable endpoint; it must not contain ML inference, file access, telemetry, device switching logic, or an unbounded user-mode dependency. The per-user engine owns physical endpoint selection and recovery.

The exact virtual render-to-capture relationship is a driver prototype decision. It must preserve sample rate, frame order, and `WAVEFORMATEXTENSIBLE` channel-mask metadata, and it must have a bounded policy for mismatched virtual/physical clocks. The SYSVAD sample's simulated capture/loopback behaviour is not evidence that this requirement is already solved.

## Process roles

| Component | Role | Real-time responsibility |
| --- | --- | --- |
| `AdaptiveAudio.exe` | Tray UI and user configuration | None; talks to engine through a versioned control boundary. |
| `AdaptiveAudioEngine.exe` | Per-user background audio host | Owns WASAPI clients, recovery state machine, and the real-time processing callback. |
| `adaptive_audio_dsp` | Deterministic DSP library | No allocation, locks, I/O, model calls, or logging in `ProcessInterleaved`. |
| `AIInference` (future) | Model/session owner | Receives features on a bounded queue; publishes bounded controls only. |
| Virtual audio driver (future) | Endpoint exposure and PCM transport | Minimal kernel-mode WaveRT path; no policy or AI. |
| Optional privileged helper (future) | Install, update, rollback | Not part of the audio signal path. |

`AudioService.exe` is intentionally not implemented as a Windows service for V1. A Windows service runs in Session 0, whereas default output selection and user audio endpoints are session-scoped. Starting with a per-user background engine avoids making endpoint access, device notification, and user switching less reliable. A separate privileged helper may be justified later for driver installation and recovery, but it must not be in the live signal path.

## Thread and ownership boundaries

```text
WASAPI callback thread
  consumes: preallocated PCM buffer + at most 4 precomputed DspControlPacket values
  produces: processed PCM + atomic counters

feature thread (future)
  consumes: bounded PCM snapshots; drops analysis work under load
  produces: feature batches

inference thread (future)
  consumes: bounded feature batches
  produces: bounded AI target, never PCM

control coordinator thread
  consumes: AI target/user configuration
  produces: precomputed DspControlPacket into fixed SPSC queue
```

The audio callback continues with its last valid control state when feature extraction, inference, telemetry, or the UI is delayed or unavailable. A future recovery state machine must prefer bypass/passthrough over silence when routing is still viable.

## Audio representation and channel policy

The current DSP core takes interleaved float32 PCM and an immutable `AudioFormat` containing sample rate, channel count, and the Windows channel mask. It supports 1–8 channels from 32–192 kHz. It preserves order and applies an identical three-band EQ cascade independently to every channel. It does not downmix, upmix, reorder, or use stereo-width processing.

Identical independent filters preserve inter-channel phase and timing relationships for normal operation. The current final clip guard acts per channel only when a sample exceeds its knee; this is intentional short-term overload protection, not a transparent linked look-ahead limiter. A linked limiter must be designed and measured before it replaces the guard.

## Initial DSP control contract

The model-facing contract is intentionally small:

| Intent | Initial hard range | Maximum movement |
| --- | ---: | ---: |
| Low shelf gain | -4 to +4 dB | 1 dB/s |
| Presence gain | -3 to +3 dB | 1 dB/s |
| High shelf gain | -3 to +3 dB | 1 dB/s |

The control coordinator checks finiteness, clamps to those ranges, rate-limits changes, and computes RBJ biquad coefficients outside the callback. The real-time processor accepts only valid precomputed packets. These values are conservative initial safety limits, not a perceptual tuning result. Any wider range or added DSP stage needs a listening test, an overload test, and a documented reason.

## Latency and compatibility claims

No latency result exists yet. The approximately 10 ms goal is an engineering budget, not an advertised capability. The measurement harness must separately record virtual endpoint period, bridge occupancy, physical render period, DSP duration, scheduling jitter, and device/driver contribution. Bluetooth must be reported as a separate output class because its transport/device latency can dominate the software path.

The V1 compatibility statement must be limited to applications using the selected AdaptiveAudio endpoint through normal Windows shared-mode playback. WASAPI exclusive mode, ASIO, vendor paths, and DRM/protected or hardware-offloaded paths require explicit compatibility testing and may bypass the route.
