# Sanitized native Windows USBPcap cold-connect observations

Observation date: 2026-08-17

Status: three controlled upstream-USB reconnect trials captured enumeration,
the first data-bearing bursts, and later sustained HDMI 2 traffic. The exact
15-transfer first-burst shape repeated in all three trials. This qualifies the
offline structural fixture for this tested setup; it does not identify a
command or authorize hardware writes.

These were USB reconnects while using the same dock and host. The dock was not
necessarily power-cycled, so this document does not call them independent
firmware cold boots.

## Environment and provenance

- Capture boundary: USBPcap on a dedicated Windows computer
- Capture interface: `USBPcap4`
- Dock: Plugable UD-3900PDZ
- USB identity and revision: `17e9:4323`, `bcdDevice 0x3156`
- Display output: HDMI 2
- Display mode: 1920x1080 at 60 Hz
- Test image: synthetic solid black
- Action: begin capture with the upstream cable disconnected, then connect once

| Trial | File size | Duration | SHA-256 |
| --- | ---: | ---: | --- |
| Cold-01 | 241,949,204 bytes | 27.004839 s | `5a0e756a4439a443a74f22fe4b8588d515db7b3c792cd5c348a82fd7f13e1df4` |
| Cold-02 | 246,657,256 bytes | 31.058362 s | `5b69f4271153626a3882161658e4f189ec4b76901c648e9a5a4a693f5f98f0cc` |
| Cold-03 | 283,930,660 bytes | 37.841430 s | `42f199facd0ebd9ba035dd3b3d0ed2438411f86e905304e2ad988c1c7ec3b53d` |

The raw pcapng files remain private. They contain device strings, kernel
request identifiers, proprietary messages, and screen-bearing traffic. No raw
payload, serial string, request address, or verbatim captured byte sequence is
in this repository.

## Repeated transport timeline

All three device descriptors confirmed the exact target and all three
configuration descriptors confirmed vendor interface `ff/00/03`, bulk OUT
`0x02`, bulk IN `0x84`, and 1024-byte maximum packets.

The table uses the first data-bearing burst as its relative origin. Timing is
observational and is not accepted as a parser rule.

| Measurement | Cold-01 | Cold-02 | Cold-03 |
| --- | ---: | ---: | ---: |
| Descriptor request to first burst | 0.200021 s | 0.210007 s | 0.509579 s |
| First-burst duration | 4.530 ms | 5.031 ms | 4.391 ms |
| Second burst start | +0.358931 s | +0.358232 s | +0.358893 s |
| Third burst start | +0.724473 s | +0.726510 s | +0.723770 s |
| Fourth burst start | +1.087312 s | +1.088285 s | +1.086050 s |
| First 65,536-byte OUT | +1.353884 s | +1.369736 s | +1.338693 s |

The descriptor-to-burst delay varies, while the approximately 359-millisecond
burst cadence is closely repeated. The first large write is only a transport-
phase boundary; the captures contain no physical monitor-visible marker.

## Qualified first-burst shape

The first data-bearing burst has the same 15-transfer direction/length sequence
in all three trials:

```text
O16 O32 O80 I39 O48 I38 O64 I38 O64 I38 I549 I31 O176 I38 I34
```

`O` means one host-to-device transfer on `0x02`; `I` means one successful
device-to-host completion on `0x84`. These are capture boundaries and lengths,
not command labels. Every first-burst transfer was complete, paired, and
successful.

The following 41-transfer burst also repeated exactly at the metadata level:

```text
I58 I42 O64 I38 I58 O80 I38 I51 O64 I38 I29 O64 I38 I58 O64 O64
I112 O64 I80 O64 I160 O64 I48 I64 O64 I64 O64 I64 O64 I64 O64 I64
O80 I64 O80 I64 I576 I48 O192 I64 I64
```

The third burst contained the same transfer set, but one adjacent IN/OUT pair
differed in ordering among trials. The fourth burst varied further. Those later
bursts are not modeled as exact ordered fixtures.

## Trial-stable and trial-variable positions

Offsets below are zero-based. They describe equality across these three
captures only; no byte values are published or embedded.

| Transfer | Direction/length | Trial-variable offsets | Trial-stable offsets |
| ---: | --- | --- | --- |
| 1–6, 8–12, 14 | as listed above | none observed | entire transfer |
| 7 | `O64` | 44–51 | 0–43 and 52–63 |
| 13 | `O176` | 44–171 | 0–43 and 172–175 |
| 15 | `I34` | 26–33 | 0–25 |

Twelve of the 15 payloads were byte-identical in this fixed setup. Across all
1,285 first-burst positions, 1,141 matched and 144 varied. “Trial-stable” does
not mean protocol constant: all trials used the same dock, host, driver,
monitor, mode, and closely spaced reconnects. No authentication, counter, key,
nonce, or field purpose is inferred from either class.

The source model records only the variable ranges. It deliberately does not
store or validate any matching captured value.

## Repeated structural patterns

Across the three captures:

- all 476 complete payload-bearing IN transfers had two zero-valued prefix
  bytes followed by a little-endian 16-bit value equal to the remaining body
  length;
- all 12,777 observed OUT declarations were positive and 16-byte aligned; and
- all first-burst endpoint completions reported success.

These are bounded structural correlations, not evidence of a command,
encryption scheme, key, counter, or field meaning.

## Capture limits

USBPcap retained the same 65,535-byte snapshot limit in every trial.

| Trial | Truncated later OUT records | Omitted payload bytes | Post-onset OUT pair imbalance |
| --- | ---: | ---: | ---: |
| Cold-01 | 3,219 | 90,132 | 83 |
| Cold-02 | 3,251 | 91,012 | 95 |
| Cold-03 | 3,697 | 103,516 | 64 |

The small startup bursts are complete. Truncation and missing high-rate submit
records begin only after large streaming starts, so these files qualify the
first-burst parser but are not lossless video corpora.

## Remaining gates

No captured OUT payload will be replayed and real hardware writes remain
disabled. The next discriminating experiments are an otherwise identical
reconnect with HDMI disconnected and a separately documented true dock power-
cycle trial. Those can determine which structural correlations depend on the
attached display or retained dock state without assigning unsupported message
meanings.
