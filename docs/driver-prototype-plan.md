# Virtual endpoint prototype plan

## Decision

Use Microsoft's SYSVAD virtual audio device sample as the learning and prototype base for a WaveRT virtual endpoint. Do not write a bespoke kernel audio stack from scratch and do not add adaptive DSP or ONNX Runtime to the driver.

## Preconditions

1. Install a supported Visual Studio + Windows SDK + WDK combination on a dedicated development machine or VM.
2. Clone the Windows Driver Samples repository with its required WIL submodule.
3. Build and test the unmodified SYSVAD sample with test signing and kernel debugging enabled on the target.
4. Record install, uninstall, reboot, rollback, crash, and audio-engine restart behaviour before changing data flow.

The driver package will ultimately need a production signing and servicing plan. Test signing is for the laboratory prototype only and must never be presented as a consumer installation route.

## Prototype questions that require evidence

| Question | Required experiment | Exit condition |
| --- | --- | --- |
| Can a virtual render endpoint accept standard shared-mode playback? | Make it default; play stereo and 5.1 fixtures through common applications. | Endpoint activity and format negotiation are recorded. |
| Can the driver expose a bounded, timestamped user-mode route for those frames? | Implement a minimal paired data path; verify exact impulse/frame ordering. | No unexplained loss, duplication, reordering, or channel-mask change. |
| How are two endpoint clocks reconciled? | Run virtual and physical endpoints for hours with occupancy tracing. | Explicit drift and under/overflow policy passes. |
| What is the usable latency? | Inject an impulse and measure rendered output with a hardware loopback interface. | Distribution reported by buffer configuration and output device type. |
| What happens on physical device loss? | Disconnect/change default endpoint during playback. | Engine recovers or passes safely without a stuck audio graph. |

## Safety constraints

- No pageable work, allocation, disk I/O, network access, logging burst, or wait on the driver streaming path.
- Driver-to-user transport must have fixed memory ownership and bounded queues.
- Driver failure handling is a product-quality concern: a kernel driver bug can destabilize the machine. Keep the kernel code minimal and isolate experimentation to test-signed environments.
- Do not attach a system APO to unrelated third-party physical audio drivers. APO registration is driver-specific and a faulty APO can crash the Windows audio engine.

## First acceptance test set

- 44.1 kHz and 48 kHz float32 stereo fixtures
- 48 kHz 5.1 channel-order impulse fixture
- 24-hour sine/noise continuity run with queue occupancy and under/overrun counters
- physical endpoint disconnect/reconnect
- application restart and Windows Audio service restart
- normal shared-mode player, exclusive-mode WASAPI player, and ASIO application compatibility classification
