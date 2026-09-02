# Windows virtual audio endpoint research

**Research date:** 2026-09-02  
**Scope:** current public Microsoft documentation only.  
**Implementation status:** no driver package, kernel code, INF, signing key, or
installation action has been created by this repository.

## Evidence labels

- **Verified** means the statement is directly supported by one of the official
  Microsoft sources listed at the end of this document.
- **Product decision** is a project constraint selected for safety or scope; it
  still needs code and target-machine validation.
- **Open question** means the cited documentation does not establish the answer
  for this product. It is not evidence that the capability is impossible.

## Decision at this phase

**Product decision:** do not implement a virtual audio driver in this
foundation milestone. First complete a minimal user-mode PCM path and the
measurement framework, then run a separately packaged driver spike on a
dedicated test machine.

**Verified:** SYSVAD is Microsoft's current sample of a virtual audio device;
its `TabletAudioSample` demonstrates a WDM driver with WaveRT/offload render
support. Microsoft describes sample code as a starting point for a custom
proprietary driver, not as a shipped product implementation. [M5]

**Verified:** ACX 1.1 is the current Microsoft-recommended framework for new
audio-driver development. ACX is KMDF-based, currently supports WaveRT
streaming, and coexists with legacy PortCls/KS drivers. [M7] [M8]

**Conclusion:** SYSVAD/PortCls is the correct learning/reference baseline
because it is the official virtual-device sample. It is not yet the selected
production implementation. The driver spike must compare a SYSVAD-derived
PortCls/WaveRT route with an ACX/WaveRT proof of endpoint enumeration and PCM
transport before selecting the production framework. Neither documented sample
or framework description proves the product's required route to a selected
physical output.

## What Windows actually documents

### Endpoint creation

**Verified:** Windows Audio Endpoint Builder monitors `KSCATEGORY_AUDIO`
device-interface arrivals. It discovers unconnected bridge pins and creates an
endpoint for them; an unconnected bridge pin categorized as
`KSNODETYPE_SPEAKER` creates a speaker endpoint. It considers the endpoint
active when it can trace a path to a host pin that supports PCM, AC-3, or WMV.
[M2]

**Verified:** Endpoint topology and pin data ranges affect how an audio device
appears to applications. The documented topology algorithm can hide endpoints
or make endpoints mutually exclusive if the topology is suboptimal. [M2] [M6]

**Consequence:** a virtual playback endpoint is not merely a user-mode device
name. Its kernel audio-device topology, exposed bridge pin, PCM-capable host
path, formats, and PnP/interface registration must be correct enough for the
Endpoint Builder to create a usable render endpoint.

### WaveRT / PortCls

**Verified:** a WaveRT filter combines the system WaveRT port driver (generic
functions) with the vendor WaveRT miniport (device-specific functions). For
rendering, the audio engine opens a KS pin, supplies the wave format, requests a
cyclic buffer, and periodically writes audio to that buffer. The driver reports
the stream's buffer/position/clock-related behaviour. [M3] [M4]

**Verified:** a WaveRT miniport implements `IMiniportWaveRT` and
`IMiniportWaveRTStream`; the documented stream interface includes buffer
allocation/free, format/state changes, position/clock information, and hardware
latency reporting. [M4]

**Verified:** legacy WDM audio drivers use kernel streaming. For a custom audio
adapter, Microsoft identifies PortCls as the correct choice for most adapters,
with AVStream as the other documented class-driver choice; a PortCls custom
adapter is made available to applications using WaveRT. [M6]

**Consequence:** a minimal PortCls/WaveRT virtual render prototype needs a real
stream implementation with bounded buffer ownership, valid format negotiation,
monotonic position/clock semantics, endpoint topology, and PnP packaging. A
stub that only creates a friendly name is not a functioning endpoint.

### ACX

**Verified:** ACX is a KMDF audio class extension. Its model represents an
endpoint audio path as one or more circuits and uses objects for streams,
formats, and other audio concepts. Current ACX supports only WaveRT-based
streaming and runs alongside legacy PortCls/KS. [M7]

**Verified:** Microsoft states that ACX 1.1 is the current version and is
recommended for all new driver development; it is supported from Windows 10,
version 2004 with the stated KMDF minimum. [M8]

**Open question:** the public ACX overview does not prove that its sample or
default circuit setup implements this product's software-only virtual render
endpoint plus user-mode PCM handoff. A focused ACX prototype is needed before
claiming that it is a lower-risk replacement for the SYSVAD-derived route.

### SYSVAD

**Verified:** the SYSVAD `TabletAudioSample` uses a virtual audio device rather
than a hardware adapter and demonstrates WaveRT and audio offloading for
rendering devices. The same solution also includes a `SwapAPO` sample. [M5]

**Not inferred:** SYSVAD does **not** automatically give AdaptiveAudio a
production render-to-physical-output implementation, an installer, a
cross-device clock policy, telemetry, update/rollback handling, or permission
to process third-party physical endpoints. Those are product work items.

**Product decision:** if SYSVAD is used in the spike, begin by building and
installing the unmodified sample on an isolated target. Record the endpoint(s),
formats, buffer behaviour, removal/reinstall behaviour, and debugger output
before changing its data flow.

## Minimum requirements for the product endpoint

| Requirement | Evidence level | What must be proven |
| --- | --- | --- |
| A PnP-installable, signed driver package exposes an audio device interface and a valid render endpoint topology. | Verified endpoint discovery/package concepts; product-specific implementation open. [M2] [M9] | Endpoint appears, is active, survives reboot, and can be selected as a normal render device. |
| The endpoint accepts negotiated shared-mode PCM formats without changing channel identity or frame order. | Product requirement. | Stereo and multichannel impulse fixtures retain frame/channel order and `WAVEFORMATEXTENSIBLE` metadata where supplied. |
| The stream makes bounded progress, correctly reports positions/latency, and has explicit underflow/overflow behaviour. | Verified WaveRT stream responsibilities; product policy open. [M3] [M4] | Long-duration occupancy/position trace has a documented drift and recovery policy. |
| PCM reaches a user-mode engine or an equivalent controlled route, then a selected physical render endpoint. | Open question. | A measured, bounded data transport proves exact PCM ordering and establishes its permission, lifetime, and failure semantics. |
| Physical output loss, virtual endpoint removal, Windows Audio restart, and engine crash recover to safe audio behaviour. | Product requirement. | Test matrix records outcome and requires passthrough/bypass where routing remains viable. |
| Updates and uninstalls have a reversible package/device procedure. | Verified package tools exist; product servicing policy open. [M9] | Versioned package can be installed, rolled back, and removed after streams are stopped. |

### The unresolved PCM bridge

**Verified:** WaveRT documentation describes the audio engine writing to a
cyclic buffer that the miniport exposes for the WaveRT stream. [M3]

**Open question:** the reviewed Microsoft documents do not define a generic
product-ready API that exports a virtual WaveRT render stream's PCM directly to
an arbitrary ordinary user-mode process for subsequent rendering on a different
physical endpoint. That is a documentation gap for this design, not a claim
that a solution cannot be implemented.

Possible designs—such as a paired virtual capture endpoint, a narrowly scoped
driver/user transport, or an all-kernel route—must not be chosen from analogy.
Each has different synchronization, security, clock, latency, install, and
failure implications. The first driver spike must explicitly select one,
document its ABI and buffer ownership, and prove the following before any DSP:

1. PCM frame ordering and channel metadata are preserved.
2. Producer/consumer clocks have a bounded drift policy.
3. The route never blocks the Windows audio engine on a user-mode process.
4. Engine loss leads to a documented fallback rather than a stalled kernel
   stream.
5. The virtual endpoint cannot cause unbounded kernel memory, waits, or logging
   on the streaming path.

## Candidate driver models and their current status

| Model | What is verified | Project status |
| --- | --- | --- |
| PortCls + WaveRT, SYSVAD-derived | Official virtual-device sample; mature documented port/miniport WaveRT model. [M3] [M5] [M6] | Reference/learning baseline; run unmodified sample first. |
| ACX + WaveRT | Current ACX 1.1 is recommended for new development and only supports WaveRT streaming. [M7] [M8] | Evaluate in the spike; no claim yet that it solves software-only virtual routing. |
| AVStream | Microsoft documents it as an alternative custom-audio class-driver model. [M6] | Not selected; no evidence yet that it reduces this product's risk. |
| APO on an existing endpoint | APOs are in-process user-mode COM plugins associated with a logical device. [M1] [M11] | Not an endpoint-interception substitute; do not attach an APO to third-party physical drivers for V1. |

The APO option is deliberately excluded from the V1 route. Microsoft requires
real-time APO methods to be nonblocking with nonpageable process-path code/data
and buffers. That is incompatible with treating an APO as a host for general
model loading, arbitrary inference, or telemetry. [M11]

## Practical development and testing workflow

### 1. Prepare an isolated driver environment

**Verified:** the WDK is the Microsoft kit for developing, testing, and
deploying Windows drivers. The SDK and WDK build numbers must match. At the time
of this research, Microsoft documents WDK 28000.2526 with Visual Studio 2026,
and WDK 26100.6584 for continued Visual Studio 2022 use. [M10]

**Product decision:** keep the application CMake build separate from the driver
solution/package. Do not make an ordinary application build install a kernel
driver.

**Product decision:** use a dedicated physical test computer or a carefully
configured test VM, with a kernel-debug path and a known rollback procedure. A
kernel audio-driver fault can destabilize the machine. Microsoft explicitly
limits Driver Verifier to test/debug computers and notes it can induce crashes.
[M12]

### 2. Establish an unmodified-sample baseline

1. Install a matching Visual Studio/SDK/WDK set and retain its exact build
   numbers in the test report.
2. Obtain the SYSVAD source from Microsoft's current Windows Driver Samples
   location cited by Microsoft. [M5]
3. Build/package the unmodified sample for the target architecture.
4. Install it only on the isolated target; enumerate endpoints and record the
   PnP device, package version, supported/mix formats, and uninstall path.
5. Reboot, start/stop audio clients, remove/reinstall the package, and preserve
   the resulting setup logs before modifying source.

### 3. Add one bounded PCM proof at a time

The first product delta may add only a fixed-size frame/sequence trace or an
impulse fixture. It must not add AI, DSP, an unbounded kernel/user queue, a
physical-device policy engine, or production installer logic. Required proof
sequence:

1. shared-mode applications render to the virtual endpoint;
2. selected input formats and channel layouts are negotiated and recorded;
3. frames reach the chosen controlled receiver with no unexplained loss,
   duplication, reordering, or channel swap;
4. a physical WASAPI render route proves unchanged PCM output;
5. clock-drift, underflow/overflow, endpoint removal, and process-crash cases
   have measured, bounded behaviour.

### 4. Test and debug as a driver, not as an application

**Verified:** Microsoft documents host/target provisioning for deployment,
testing, and debugging, and recommends a kernel debugger with Driver Verifier.
[M12] [M13]

**Product decision:** collect at least these test artifacts for each driver
candidate: package hash/version, Windows build, WDK/SDK build, Secure Boot/test
signing state, endpoint IDs/formats, buffer periods, stream-position trace,
queue occupancy, underruns/overruns, driver-verifier result, crash dumps, and
hardware impulse latency measurement. Do not infer end-to-end latency from the
driver's reported latency alone.

## Signing, installation, update, and rollback

### Development test signing

**Verified:** a PnP driver package uses an INF and a catalog; changes to files
represented in the catalog require the catalog to be regenerated and signed.
Microsoft documents test signing and installation/staging with elevated
PnPUtil, as well as device/package removal after the device stops using it.
[M9]

**Verified:** conventional `TESTSIGNING` testing requires a reboot. Current
Microsoft guidance states that TESTSIGNING requires Secure Boot to be disabled;
preproduction WHQL/WHCP signing is a separate development/validation path for
testing while Secure Boot remains enabled. [M9] [M14]

**Product decision:** test-signed packages are lab-only. They will never be
installed by the ordinary product build or represented as an end-user
installation method. Before changing test-signing or Secure Boot state, preserve
the target's recovery keys and current configuration; use a disposable target.

### Public release

**Verified:** for new kernel-mode drivers on Windows 10 version 1607 and later,
Microsoft's policy requires Dev Portal signing. Current documentation says an EV
code-signing certificate is required to establish the Hardware Dev Center
dashboard account. [M15]

**Verified:** attestation signing is available for Windows 10 Desktop and later
and requires an EV certificate for Partner Center submission, but it is not
Windows Certified and cannot be published to retail Windows Update audiences.
Windows Hardware Compatibility Program submission with HLK/HCK results is the
route documented for certification/retail Windows Update. [M14] [M15]

**Product decision:** production planning must include package versioning,
hardware/OS support policy, driver-store installation, elevation, staged
updates, a tested rollback package, and an uninstall path that first stops the
device/streams. Signing is a release gate, not an afterthought.

## Claims deliberately not made

- No driver is built, installed, test-signed, or signed for release.
- No virtual endpoint exists in this repository yet.
- No claim is made that SYSVAD, ACX, WaveRT, or an APO routes all Windows audio
  to a physical endpoint.
- No latency, CPU, power, compatibility, or stability number has been measured
  for a virtual route.
- No compatibility claim is made for exclusive-mode WASAPI, ASIO, vendor
  paths, protected media, Bluetooth, or multichannel output.
- No production signing or servicing plan has been approved.

## Driver-spike entry and exit criteria

Do not start kernel implementation until the application foundation can build,
unit tests run, logging/configuration are isolated from the real-time path, and
the PCM benchmark format/measurement schema is agreed.

The driver spike is complete only when all of the following are captured on an
isolated target:

- a normal shared-mode application can select and render to the virtual
  endpoint;
- a 44.1 kHz or 48 kHz stereo impulse and a 48 kHz 5.1 channel impulse preserve
  frame order, channel identity, and timestamps through the controlled route;
- virtual and physical clock/queue occupancy are recorded for a multi-hour
  continuity run with an explicit drift policy;
- device removal, Windows Audio restart, engine loss, install/update/rollback,
  and overload paths are exercised;
- Driver Verifier/kernel-debug test results are recorded; and
- latency is measured externally where possible and reported by buffer setting
  and output class, without treating Bluetooth and wired output as equivalent.

## Official Microsoft sources

All sources below were accessed on 2026-09-02. Version-specific signing and WDK
rules must be rechecked immediately before a driver build or release.

- **[M1]** [Windows Audio Architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/windows-audio-architecture)
- **[M2]** [Audio Endpoint Builder Algorithm](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-endpoint-builder-algorithm)
- **[M3]** [Understanding the WaveRT Port Driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/understanding-the-wavert-port-driver)
- **[M4]** [WaveRT Miniport Driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/wavert-miniport-driver)
- **[M5]** [Sample Audio Drivers / SYSVAD](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/sample-audio-drivers)
- **[M6]** [WDM Audio Drivers Overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/getting-started-with-wdm-audio-drivers)
- **[M7]** [ACX Audio Class Extensions Overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-audio-class-extensions-overview)
- **[M8]** [ACX Version Information](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-version-overview)
- **[M9]** [Test Signing](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing)
- **[M10]** [Download the Windows Driver Kit](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- **[M11]** [Implementing Audio Processing Objects](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects)
- **[M12]** [How to Use Driver Verifier for Driver Testing](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/driver-verifier)
- **[M13]** [Provision a Computer for Driver Deployment and Testing](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/provision-a-target-computer)
- **[M14]** [Driver Signing Options](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings) and [preproduction signing with Secure Boot](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/preproduction-driver-signing-and-install)
- **[M15]** [Driver Signing Policy](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-)
