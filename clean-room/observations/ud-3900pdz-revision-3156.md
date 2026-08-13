# Sanitized descriptor observation: UD-3900PDZ revision 3156

Observation date: 2026-08-13

Status: independently reproduced read-only descriptor fact; no USB interface
was opened or claimed by the probe and no transfer was sent.

## Environment

- Dock: Plugable UD-3900PDZ
- USB identity: `17e9:4323`
- Observed `bcdDevice`: `0x3156`
- Host architecture: Apple silicon, M3 Pro
- Host operating system: macOS 27 beta
- Probe: repository `clean-room/dock-probe`

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

## What this does not establish

The I/O Registry metadata did not expose endpoint addresses, live transfers,
activation commands, EDID traffic, mode setting, frame records, compression,
or recovery behavior. It does not show that the USB-display output produced a
picture. Those facts require standard descriptor access after the current
interface owner exits, a host trace on another operating system, or a compatible
external USB analyzer on the Mac.
