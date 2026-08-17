# Sanitized native Windows USBPcap cold-start observation

Observation date: 2026-08-17

Status: one controlled cold-connect trial captured enumeration, the first
observed data-bearing burst, and later sustained HDMI 2 traffic. The burst
shape is implemented as a provisional offline parser fixture. Two additional
matching cold trials are still required before any field is classified as
stable or variable.

## Environment and provenance

- Capture boundary: USBPcap on a dedicated Windows computer
- Capture interface: `USBPcap4`
- Dock: Plugable UD-3900PDZ
- USB identity and revision: `17e9:4323`, `bcdDevice 0x3156`
- Display output: HDMI 2
- Display mode: 1920x1080 at 60 Hz
- Test image: synthetic solid black
- Action: begin capture with the upstream cable disconnected, then connect once
- Capture duration: 27.004839 seconds
- Capture SHA-256: `5a0e756a4439a443a74f22fe4b8588d515db7b3c792cd5c348a82fd7f13e1df4`

The raw pcapng remains private. It contains device strings, kernel request
identifiers, proprietary messages, and screen-bearing traffic. No raw payload,
serial string, request address, or verbatim captured byte sequence is in this
repository.

## Transport timeline

All timestamps below are relative to the first record in the capture.

| Event | Time |
| --- | ---: |
| First target descriptor request | 0.171393 s |
| Exact device descriptor returned | 0.171420 s |
| Configuration 1 selected | 0.172990 s |
| Configuration selection completed | 0.182765 s |
| First display OUT payload | 0.371414 s |
| First display IN payload completed | 0.372682 s |
| First OUT transfer larger than 1 KiB | 1.649275 s |
| First 65,536-byte OUT transfer | 1.725298 s |

The configuration descriptor independently confirms vendor interface
`ff/00/03`, bulk OUT `0x02`, bulk IN `0x84`, and 1024-byte maximum packets.
All corresponding display-endpoint completions in the initial burst reported
success.

## First observed burst shape

The first data-bearing burst spans 4.530 milliseconds. Its externally
observable direction/length sequence is:

```text
O16 O32 O80 I39 O48 I38 O64 I38 O64 I38 I549 I31 O176 I38 I34
```

`O` means one host-to-device transfer on `0x02`; `I` means one successful
device-to-host completion on `0x84`. These are transfer boundaries and lengths,
not command labels. The sequence closely aligns with the earlier payload-free
Windows ETW metadata, but the ETW completion-length and capture-boundary
semantics differ and do not independently qualify this exact fixture.

Across this capture, all 181 complete payload-bearing IN transfers had a
four-byte envelope: two zero-valued bytes followed by a little-endian 16-bit
value equal to the remaining body length. All 4,042 observed OUT declarations
were positive and 16-byte aligned. These are provisional structural patterns,
not evidence of a command, encryption scheme, key, counter, or field meaning.

## Capture limits

USBPcap recorded a 65,535-byte snapshot limit. The small first-burst transfers are
complete, but 3,219 later 65,536-byte OUT transfers retained only 65,508 bytes
after the 27-byte USBPcap pseudoheader. High-volume pair counts also indicate
that later streaming records were dropped. This trace therefore qualifies the
initial parser work but is not a lossless video corpus.

The first 65,536-byte write is a conservative transport-phase boundary. It does
not prove the physical instant when the monitor became visible. No further
field or command meaning will be assigned until repeated cold traces and
controlled negative/changed-input experiments support it.
