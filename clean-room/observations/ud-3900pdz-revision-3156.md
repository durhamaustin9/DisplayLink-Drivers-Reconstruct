# Sanitized descriptor observation: UD-3900PDZ revision 3156

Observation date: 2026-08-13

Status: independently reproduced public descriptor facts. The registry probe
opened or claimed no USB interface and sent no transfer. The later opt-in
reader temporarily opened the exact device with no capture or seize option and
released it immediately. It claimed no interface and performed no configuration,
reset, vendor request, firmware operation, or endpoint transfer. Apple's API
may issue a standard USB `GET_DESCRIPTOR` request when the descriptor is not
cached.

## Environment

- Dock: Plugable UD-3900PDZ
- USB identity: `17e9:4323`
- Observed `bcdDevice`: `0x3156`
- Host architecture: Apple silicon, M3 Pro
- Host operating system: macOS 27 beta
- Probe: repository `clean-room/dock-probe`
- Standard reader: repository `clean-room/standard-descriptor-reader`

The observation was reproduced from the same attached hardware by the user and
the repository maintainer during the session. Device, monitor, and machine
serial identifiers were deliberately omitted.

## Observed public descriptor metadata

| Interface | Class/subclass/protocol | Alternate | Endpoints | Classification |
| --- | --- | --- | --- | --- |
| 0 | `ff/00/03` | 0 | 2 | candidate vendor display transport |
| 1 | `fe/01/01` | 0 | 0 | auxiliary interface |
| 2 | `01/01/20` | 0 | 1 | standard USB audio |
| 3 | `01/02/20` | 0 | 0 | standard USB audio |
| 4 | `01/02/20` | 0 | 0 | standard USB audio |
| 5 | `02/0d/00` | 0 | 1 | standard USB networking control |
| 6 | `0a/00/01` | 1 | 2 | standard USB networking data |

The probe reported USB speed code `4` and seven interfaces.

## Candidate endpoint descriptors

After DockBridge reached zero exact processes, the opt-in reader observed the
standard configuration descriptor and released the device. DockBridge then
relaunched successfully.

| Interface | Address | Direction | Type | Maximum packet | Interval | Burst | Streams |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `0x02` | host to device | bulk | 1024 bytes | 0 | one packet / 1024 bytes | 0 |
| 0 | `0x84` | device to host | bulk | 1024 bytes | 0 | one packet / 1024 bytes | 0 |

Interface 1 declared zero endpoints. The parsed endpoint count exactly matched
each interface's declared count.

## What this does not establish

The standard descriptors alone do not expose live transfers, activation
commands, EDID traffic, endpoint semantic roles beyond standard direction/type,
mode setting, frame records, compression, or recovery behavior. A later
[Windows ARM64 guest ETW observation](windows-arm64-parallels-etw-2026-08-14.md)
confirmed repeated traffic and visible output on the two candidate endpoints,
but it does not reveal payload format or command meaning and is not a physical
Mac-bus trace. A later
[native Windows USBPcap cold-start observation](windows-native-usbpcap-cold-2026-08-17.md)
captured one complete first data-bearing burst and provisional structural
patterns; later activation phases remain uninterpreted, and the trace does not
establish command semantics.
