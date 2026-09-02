# AdaptiveAudio engineering contract

**Status:** binding engineering constraints for the foundation and every later
milestone. This document specifies required behaviour; it does not assert that
an unimplemented component already has that behaviour.

## Product boundary

AdaptiveAudio is a Windows desktop audio utility whose intended V1 route is:

```text
normal shared-mode application playback
  -> AdaptiveAudio virtual render endpoint
  -> AdaptiveAudio per-user engine
  -> selected physical render endpoint
```

This is **not** a claim of universal system-audio interception. WASAPI
exclusive-mode streams, ASIO, hardware-offload/vendor paths, protected paths,
and future or unusual driver paths are separate compatibility cases. They must
be measured and classified before a support claim is made.

The first functional path is unchanged PCM passthrough. It is a prerequisite
for enhancement, not an opportunity to add hidden processing. Neural-network
inference, training, adaptive controls, and advanced DSP are out of scope until
that path is proven reliable and measured.

## Module and privilege boundaries

| Module | Owns | Must not own |
| --- | --- | --- |
| `AdaptiveAudio.exe` | Tray UI, user-visible configuration, status | Audio-period work, driver installation, model execution |
| `AudioService.exe` | Per-user audio session, endpoint selection, recovery state machine, real-time bridge | UI work, filesystem logging, model loading on a time-critical thread |
| `DSPCore` | Deterministic PCM processing and control application | Windows device management, persistence, inference scheduling |
| `AIInference` | Future model lifetime, feature/inference workers, bounded control publication | PCM waveform generation or the real-time callback |
| `tests` | Deterministic unit/integration tests | Production endpoint ownership |
| `tools/benchmarks` | Offline profiling, fixtures, reports | Real-time decisions in the product process |
| future driver package | Endpoint exposure and minimal bounded PCM transport | AI, policy, telemetry, general-purpose user configuration |

`AudioService.exe` is a product module name, not an instruction to run the live
audio engine in Windows Service Session 0. The active output device and user
audio session are user-scoped; the initial design is a per-user background
process. A separate privileged installer/helper may be introduced later, but it
must be outside the audio signal path.

## PCM and format contract

- PCM is the signal-path representation unless a conversion is explicitly
  negotiated and measured.
- Each stream carries its exact sample rate, channel count, sample type,
  interleaving, and Windows channel-mask/layout metadata through the pipeline.
- No component may silently downmix, upmix, reorder channels, crossfeed, or
  destroy timing/phase relationships.
- The initial bridge must test at least 44.1 kHz and 48 kHz stereo, then a
  multichannel fixture before declaring multichannel support. Other formats,
  including 96 kHz and non-float PCM, are compatibility tests rather than
  assumptions.
- Any resampler, format converter, or latency-bearing DSP stage must be an
  explicit stage with an owner, a bounded buffer policy, and a measurement.

## Real-time contract

Treat every thread or callback that must meet an audio period as real-time,
including a WASAPI event-driven render/capture bridge. The audio-period path
must have a statically bounded execution and memory profile.

It must never perform:

- heap allocation or deallocation;
- filesystem, registry, console, ETW, or network I/O;
- blocking locks, condition waits, sleeps, arbitrary waits, or synchronous IPC;
- model loading, model inference, dynamic format discovery, or device
  enumeration;
- unbounded queue traversal, exception-driven control flow, uncontrolled
  logging, or other unpredictable heavy work.

It must instead use:

- buffers and scratch space allocated before streaming starts;
- bounded single-purpose queues/ring buffers with documented ownership;
- lock-free or nonblocking handoff where threads cross the real-time boundary;
- immutable, validated control packets prepared outside the real-time path;
- atomic counters suitable for nonblocking telemetry; and
- a deterministic, bounded fallback that continues valid audio when an optional
  component is late or unavailable.

No real-time function may reach the logging or configuration implementation
indirectly. This includes error paths. The only permitted observation from that
path is a fixed-size counter, a bounded preallocated event record, or a
nonblocking best-effort queue write; dropped telemetry is preferable to a
missed audio deadline.

## Cross-thread ownership

```text
real-time PCM path
  consumes: preallocated PCM and already-validated control state
  produces: PCM plus atomic/bounded telemetry

control/configuration path
  validates new settings off the real-time path
  publishes: immutable versioned snapshot/control packet

logging/telemetry path
  consumes: bounded event records and counters
  performs: persistence, uploads, formatting, rate limiting

future feature/inference path
  consumes: bounded PCM snapshots/features; may drop analysis under load
  publishes: bounded, smoothed intent only
```

The audio path must not wait for the control, UI, logging, device-management,
telemetry, or future inference threads. A producer may drop nonessential work;
the PCM consumer must remain predictable.

## Configuration and logging

- Configuration is parsed, schema-validated, range-checked, and persisted only
  on non-real-time threads.
- A configuration update is all-or-nothing. The audio path sees either the last
  known-valid immutable state or a new validated state, never partially parsed
  data.
- Invalid or incompatible configuration falls back to safe defaults and emits a
  non-real-time diagnostic.
- Logging is asynchronous, bounded, rate-limited, and has a no-I/O failure
  mode. It may lose records under load; it must never destabilize audio.
- Logs must not include raw PCM by default. Any diagnostic capture requires an
  explicit privacy, retention, and consent design before it is enabled.

## Failure and recovery policy

Audio continuity is the priority. Failures in enhancement, inference,
configuration, telemetry, or optional DSP must resolve to neutral deterministic
processing or PCM passthrough when routing remains viable. They must not
silence, corrupt, reorder, or block audio.

The engine must have explicit states for startup, stream running, safe bypass,
endpoint loss, recovery, and terminal failure. State transitions belong to a
non-real-time coordinator; the real-time path consumes an already-safe state.
At minimum, every later endpoint implementation must exercise:

- device removal/default-device change and reconnection;
- format or period renegotiation;
- queue underflow/overflow;
- CPU overload and a late analysis/inference result;
- model/configuration load failure;
- engine restart and driver removal/update; and
- loss of the Windows audio services.

The exact behaviour for each transition must be documented and tested before it
is called supported.

## Latency and measurement discipline

The approximately 10 ms V1 goal is a budget hypothesis, not a claim. Do not
publish a latency number until an end-to-end measurement method, device class,
buffer configuration, and percentile/worst-case result are recorded.

Instrument, with nonblocking collection, at least:

- negotiated sample rate, channel count, sample format, channel mask, and
  buffer/period size;
- callback/period execution duration, maximum, percentile distribution, and
  deadline misses;
- queue occupancy, underruns, overruns, and recovery events;
- scheduling jitter, process CPU use, memory use, and physical-endpoint changes;
- per-stage DSP time, future feature time, inference time, and model-load time;
- explicitly accounted buffering/algorithmic latency; and
- output class, with Bluetooth reported separately from wired devices.

Averages are insufficient for a real-time claim. Retain maximum and high
percentile values as well as event counts. The measurement tool must state what
it cannot measure (for example, an unknown DAC or Bluetooth transport delay).

## DSP and future AI constraints

The current and future DSP chain is deterministic and serves PCM. Every stage
must have a documented purpose, explicit safe range, overload behaviour, and
test. A change that widens gain, introduces look-ahead, resampling, channel
coupling, harmonic generation, or dynamics processing requires latency,
overload, channel-integrity, and listening validation.

Future AI is a control-plane estimator only:

```text
PCM -> bounded feature window -> inference worker -> bounded/smoothed control
vector -> deterministic DSP -> PCM
```

It does not generate arbitrary output waveform samples. Model output must be
finite-checked, clamped, rate-limited, smoothed, versioned, and safe to ignore.
Inference is never allowed to block the audio path; unavailable inference means
last-known-safe controls or neutral passthrough.

## Windows and driver constraints

The virtual-endpoint driver is a separate, high-risk project boundary. It must
remain minimal and must not contain model inference, user policy, telemetry,
file/network I/O, or a dependency on a live general-purpose user process. Its
architecture, installation, signing, update, and rollback plan require a WDK
prototype and target-machine evidence. See
[docs/windows-audio-driver-research.md](docs/windows-audio-driver-research.md).

Driver and output-device behaviour must be verified on supported Windows builds
and actual hardware. Do not infer endpoint period, format support, clock
behaviour, exclusive-mode reach, or Bluetooth latency from a single machine.

## Change and verification gates

Before a material architectural change:

1. Inspect the affected modules and preserve unrelated work.
2. Document the required behaviour, ownership, real-time impact, failure mode,
   and measurement plan.
3. Make the smallest testable change.
4. Build and run relevant unit/integration tests where the toolchain is
   available; report exactly what ran and what could not run.
5. For signal-path changes, verify channel identity, frame ordering, no
   allocation/lock/I/O on the period path, and recovery behaviour.
6. For driver changes, use a dedicated test target with debugging/rollback
   available; never treat a development-signed driver as a consumer release.

No benchmark, compatibility, stability, or production-readiness result may be
fabricated or generalized beyond its measured conditions.
