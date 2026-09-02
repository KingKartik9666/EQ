# Transparent PCM audio-path prototype

## Purpose

The first executable audio experiment must prove a narrow claim only:

```text
selected WASAPI source
  -> bounded PCM frame bridge
  -> transparent processing function
  -> selected WASAPI render endpoint
```

No EQ, loudness adjustment, resampling, mixing, channel remapping, model inference, or virtual driver behaviour is part of this experiment. The processing function copies complete PCM frames unchanged. A format mismatch is a setup failure, not a reason to introduce an implicit converter in this phase.

## Modes

| Mode | Source | Destination | What it proves | What it does not prove |
| --- | --- | --- | --- | --- |
| `capture-to-render` | A chosen Windows capture endpoint | A chosen render endpoint | Capture, fixed frame buffering, transparent copying, rendering, recovery counters | Application playback interception |
| `loopback-to-render` | A chosen shared-mode render endpoint opened with WASAPI loopback | A **different** chosen render endpoint | Receiving normal shared-mode playback PCM and rendering it elsewhere | Virtual endpoint correctness or same-device rerouting |

The program must refuse a loopback-to-render run when source and destination endpoint IDs are identical: that configuration reintroduces the program's rendered audio into its own loopback capture and can create a feedback/duplication loop.

Windows supports loopback capture only for shared-mode streams. On Windows 10 version 1703 and later, event-driven loopback capture is supported; older systems need a different event-driving workaround. The initial supported test baseline is Windows 10 1703+ / Windows 11. [Microsoft loopback documentation](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)

## Format and queue contract

The prototype initially accepts only an identical source and target `WAVEFORMATEX` / `WAVEFORMATEXTENSIBLE` representation, including:

- sample rate;
- channel count;
- block alignment and bit depth;
- PCM vs IEEE float subtype; and
- channel mask where extensible format is used.

The service allocates a fixed PCM ring buffer before `IAudioClient::Start`. Capture packets are copied into the ring before their required `IAudioCaptureClient::ReleaseBuffer` call. Render pulls complete frames from the ring; it writes silence only on underflow, increments a counter, and continues. On overflow it discards the newest captured frames after recording the event, preserving render continuity and bounded memory. This is a temporary policy to expose pressure, not a clock-drift solution.

Microsoft documents that capture packet size can vary and that `GetBuffer` / `ReleaseBuffer` calls must be paired on the same processing thread. It also documents that a renderer should calculate writable frames from endpoint-buffer size minus current padding. [Capturing a stream](https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream) and [rendering a stream](https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream)

## Callback / worker boundary

The service has no user-provided DSP callback in this phase. Its event-driven worker performs only bounded endpoint operations, a bounded copy, counters, and timing reads. It must not:

- open devices or load configuration;
- allocate memory;
- acquire a blocking mutex;
- write a log record to disk or console;
- call network, UI, model, or telemetry code; or
- wait on anything other than its shutdown and WASAPI event handles.

Setup, endpoint discovery, configuration reload, logging flush, metrics presentation, and device recovery occur outside that worker. MMCSS registration is attempted for the worker and failure is recorded as a setup observation; it is not treated as a guarantee of low latency. Microsoft documents MMCSS task registration via `AvSetMmThreadCharacteristics` and lists `Pro Audio` as a built-in task name. [MMCSS documentation](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service)

## Instrumentation semantics

The prototype reports measurements, not targets.

| Metric | Definition | Collection point |
| --- | --- | --- |
| Source/target sample rate, channels, mask, block alignment | Negotiated format after stream initialization | Setup |
| Endpoint buffer frames | `IAudioClient::GetBufferSize` | Setup/recovery |
| Stream latency | `IAudioClient::GetStreamLatency` | Setup/recovery |
| Callback duration | `QueryPerformanceCounter` around one bounded service iteration | Worker counter only |
| Worst callback duration | maximum observed worker iteration | Atomic metric |
| Scheduling jitter | interval between source event wakes minus expected source period | Worker counter only |
| Underflow / overflow | ring has insufficient / insufficient free complete frames | Worker counter only |
| Estimated software queue latency | queued frames / sample rate | Snapshot/report thread |

This estimate excludes driver, DAC, acoustic, Bluetooth transport, and opaque Windows buffering effects. End-to-end latency requires a physical loopback measurement and is explicitly unmeasured at this stage.

## Failure state

| Failure | Immediate behaviour | Next action |
| --- | --- | --- |
| Source capture packet flagged silent | Push zero frames; retain timing | Count silent packet |
| Ring underflow | Render silence for missing frames | Count and continue |
| Ring overflow | Drop newly captured complete frames | Count and continue |
| Device invalidated / Windows Audio service error | Stop clients, release safely, mark path unavailable | Control thread attempts explicit recovery later |
| Invalid control/configuration | Do not start path | Report validation error outside worker |
| Unknown exception in worker | Stop path, retain diagnostic code | Do not continue with corrupted buffers |

This is an investigation tool. It cannot claim transparent application routing until a virtual endpoint sends normal shared-mode playback into the same source contract.
