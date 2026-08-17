# Sanitized Windows ARM64 USB ETW observation

Observation date: 2026-08-14

Status: three intentional black-box reconnect trials reproduced the same public
transport topology and sustained HDMI 2 output. The observer later bounded the
time to visible output as 5–15 seconds, closer to 5. Private activation
envelopes conservatively use 15 seconds; this is not an exact latency.

## Environment and provenance

- Host boundary: Windows 11 ARM64 guest in Parallels on Apple silicon
- Dock: Plugable UD-3900PDZ
- USB identity and revision: `17e9:4323`, `bcdDevice 0x3156`
- Display output: HDMI 2
- Display mode: 1920×1080 at 60 Hz
- Action: disconnect and reconnect the VM-visible USB device three times
- Visible result: a sustained picture in every trial until manual disconnect
- Capture: Microsoft USB ETW providers through the guest's virtual xHCI stack
- ETL SHA-256: `fbcf8450df004538c45ec6d207b92861002b01fe19b56d07f20b3154e3fe64db`
- Typed XML SHA-256: `8dfb5879a6150321e447846116313168763a2a4e75ce5c48ee40e566a34bb1ba`

Raw ETL/XML/CSV files remain ignored private evidence. No captured transfer
buffer, control setup packet, screen content, device serial, or monitor serial
was copied into this repository or used as implementation material.

## Repeated metadata facts

Microsoft UCX endpoint-create events independently mapped the candidate display
transport in every lifetime:

| Address | Direction | Type | Maximum packet |
| --- | --- | --- | --- |
| `0x02` | host to device | bulk | 1024 bytes |
| `0x84` | device to host | bulk | 1024 bytes |

The first bulk request on these endpoints began 275,571, 218,083, and 215,235
microseconds after endpoint-zero creation in trials 1–3.
Private envelopes normalize `action-issued` and `capture-start` to that
endpoint-zero creation event; they do not claim it is the instant of the
observer's physical handoff to the VM.

| Trial | OUT completions | OUT bytes | Successful IN completions | IN bytes |
| --- | ---: | ---: | ---: | ---: |
| 1 | 1,208 | 68,900,704 | 144 | 26,245 |
| 2 | 973 | 46,393,504 | 125 | 23,781 |
| 3 | 1,140 | 46,568,768 | 154 | 30,645 |

All observed OUT completions on `0x02` reported success. Each trial ended with
one cancelled pending `0x84` IN request while the device was being removed;
dispatch and completion counts still matched. OUT request lengths varied, with
65,536 bytes the most frequent length. Every IN request was submitted with a
4,096-byte buffer and completed with a variable actual length.

## Evidence boundary

These facts describe host requests observed inside a virtual Windows xHCI
stack. They are not a physical-bus capture and are not automatically macOS
facts. The observation establishes consistent endpoints, request direction,
length shape, successful sustained output, and orderly teardown under the
tested conditions. It does not establish payload format, message meaning,
authentication, mode-setting fields, frame encoding, or the exact transfer at
which the picture became visible.

Because visible-output time was not logged, no `output-stable` timestamp is
invented. The private records instead label 15 seconds as an observer-reported
upper bound. Their payload-free transfer metadata passes the bounded parser and
zero-filled fake replay. Real hardware writes remain blocked until message
structure and meaning are independently established and reviewed.

## Conservative 15-second window

| Trial | Transfers | Total bytes | Fake packets |
| --- | ---: | ---: | ---: |
| 1 | 1,316 | 68,129,013 | 66,782 |
| 2 | 1,035 | 45,125,477 | 44,374 |
| 3 | 1,071 | 40,761,029 | 40,158 |

Fake replay substituted zero-filled 1024-byte chunks and accessed no hardware.
The private envelopes contain no transfer buffers or setup packets.
