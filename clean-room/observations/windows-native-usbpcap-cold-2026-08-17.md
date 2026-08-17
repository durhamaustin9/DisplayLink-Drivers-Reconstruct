# Sanitized native Windows USBPcap reconnect and control observations

Observation date: 2026-08-17

Status: three controlled upstream-USB reconnect trials, one HDMI-absent
negative control, and one user-reported full-power-cycle control captured the
same 15-transfer first data-bearing burst. This qualifies the offline
structural fixture for the tested conditions; it does not identify a command or
authorize hardware writes.

The three `Cold` files were USB reconnects while using the same powered dock and
host. The power-cycle procedure removed both dock power and upstream USB for 15
seconds, but physical power removal cannot be proven from USB packets alone.

## Environment and provenance

- Capture boundary: USBPcap on a dedicated Windows computer
- Capture interface: `USBPcap4`
- Dock: Plugable UD-3900PDZ
- USB identity and revision: `17e9:4323`, `bcdDevice 0x3156`
- Connected-display trials: HDMI 2, 1920x1080 at 60 Hz
- Test image: synthetic solid black

| Observation | Condition | File size | Duration | SHA-256 |
| --- | --- | ---: | ---: | --- |
| Cold-01 | powered-dock USB reconnect, HDMI 2 connected | 241,949,204 bytes | 27.004839 s | `5a0e756a4439a443a74f22fe4b8588d515db7b3c792cd5c348a82fd7f13e1df4` |
| Cold-02 | powered-dock USB reconnect, HDMI 2 connected | 246,657,256 bytes | 31.058362 s | `5b69f4271153626a3882161658e4f189ec4b76901c648e9a5a4a693f5f98f0cc` |
| Cold-03 | powered-dock USB reconnect, HDMI 2 connected | 283,930,660 bytes | 37.841430 s | `42f199facd0ebd9ba035dd3b3d0ed2438411f86e905304e2ad988c1c7ec3b53d` |
| NoHDMI-01 | upstream USB reconnect, HDMI disconnected | 81,344 bytes | 7.482875 s | `37751e44e8140cbea7a0410e63450be3d43ed5ce9d436c798b95cee87d553a71` |
| PowerCycle-01 | reported full power cycle, HDMI 2 connected | 245,094,412 bytes | 30.917618 s | `57eb06d7d53aeeafa8f1f847e4f20fca4f81b188a08471bcdfa91a517dd7f14d` |

The raw pcapng files remain private. They contain device strings, kernel
request identifiers, proprietary messages, and—when HDMI was connected—screen-
bearing traffic. No raw payload, serial string, request address, or verbatim
captured byte sequence is in this repository.

## Topology and timing

Every device descriptor confirmed the exact target. Every configuration
descriptor confirmed vendor interface `ff/00/03`, bulk OUT `0x02`, bulk IN
`0x84`, and 1024-byte maximum packets.

The reconnect table uses the first data-bearing burst as its relative origin.
Timing is observational and is not accepted as a parser rule.

| Measurement | Cold-01 | Cold-02 | Cold-03 |
| --- | ---: | ---: | ---: |
| Descriptor request to first burst | 0.200021 s | 0.210007 s | 0.509579 s |
| First-burst duration | 4.530 ms | 5.031 ms | 4.391 ms |
| Second burst start | +0.358931 s | +0.358232 s | +0.358893 s |
| Third burst start | +0.724473 s | +0.726510 s | +0.723770 s |
| Fourth burst start | +1.087312 s | +1.088285 s | +1.086050 s |
| First 65,536-byte OUT | +1.353884 s | +1.369736 s | +1.338693 s |

The controls retained the same cadence:

| Measurement | NoHDMI-01 | PowerCycle-01 |
| --- | ---: | ---: |
| Descriptor request to first burst | 0.558187 s | 0.198101 s |
| First-burst duration | 4.761 ms | 5.714 ms |
| Second burst start | +0.358279 s | +0.358661 s |
| Third burst start | +0.724509 s | +0.732823 s |
| Fourth burst start | +1.087110 s | +1.094675 s |
| First 65,536-byte OUT | none in 7.482875 s | +1.386496 s |

The first large write is only a transport-phase boundary. The captures contain
no physical monitor-visible marker.

## Control-qualified first-burst shape

The first data-bearing burst has the same 15-transfer direction/length sequence
in all five observations:

```text
O16 O32 O80 I39 O48 I38 O64 I38 O64 I38 I549 I31 O176 I38 I34
```

`O` means one host-to-device transfer on `0x02`; `I` means one successful
device-to-host completion on `0x84`. These are capture boundaries and lengths,
not command labels. Every first-burst transfer was complete, paired, and
successful.

The following 41-transfer burst had one exact ordering in the three reconnect
trials and the HDMI-absent control:

```text
I58 I42 O64 I38 I58 O80 I38 I51 O64 I38 I29 O64 I38 I58 O64 O64
I112 O64 I80 O64 I160 O64 I48 I64 O64 I64 O64 I64 O64 I64 O64 I64
O80 I64 O80 I64 I576 I48 O192 I64 I64
```

The power-cycle control contained the same transfer multiset but reversed one
adjacent `I64`/`I576` pair. The third burst showed analogous ordering variation,
and the fourth burst varied further. No exact ordered parser fixture is assigned
beyond the first burst.

## Observed-stable and observed-variable positions

Offsets below are zero-based. They describe equality across these five
observations only; no byte values are published or embedded.

| Transfer | Direction/length | Observed-variable offsets | Observed-stable offsets |
| ---: | --- | --- | --- |
| 1–6, 8–10, 12, 14 | as listed above | none observed | entire transfer |
| 7 | `O64` | 44–51 | 0–43 and 52–63 |
| 11 | `I549` | 24 | 0–23 and 25–548 |
| 13 | `O176` | 44–171 | 0–43 and 172–175 |
| 15 | `I34` | 26–33 | 0–25 |

Eleven of the 15 payloads were byte-identical across all tested conditions.
Across all 1,285 first-burst positions, 1,140 matched and 145 varied. The
power-cycle control introduced the one-position range in transfer 11; no
meaning is assigned to it.

“Observed-stable” does not mean protocol constant. The sample still uses one
dock, host, driver installation, and hardware revision. No authentication,
counter, key, nonce, display flag, boot flag, or field purpose is inferred from
either class. The source model records only the variable ranges and deliberately
does not store or validate any captured value.

## Control results

The HDMI-absent control retained the qualified first burst and later small
polling bursts, but produced no OUT declaration larger than 224 bytes and no
video-sized transfer during 7.482875 seconds. A normal connected run reached a
65,536-byte OUT approximately 1.34–1.39 seconds after the first burst. This is
candidate evidence correlating the transition to large streaming with the
user-reported display state; it does not identify an EDID, mode-set, or enable
command. The negative control must be repeated before calling the later branch
HDMI-dependent.

The reported power-cycle control retained the first-burst shape, introduced one
additional observed-variable position, changed later-burst ordering, and still
transitioned into sustained large writes. These differences are compatible with
retained- or boot-state dependence, but one control cannot distinguish that
from ordinary run-to-run variation. The USB trace confirms one clean
enumeration after capture began, but cannot itself prove that external power
was physically absent beforehand.

## Repeated structural patterns

Across all five observations:

- all 710 complete payload-bearing IN transfers had two zero-valued prefix bytes
  followed by a little-endian 16-bit value equal to the remaining body length;
- all 16,875 observed OUT declarations were positive and 16-byte aligned; and
- all first-burst endpoint completions reported success.

These are bounded structural correlations, not evidence of a command,
encryption scheme, key, counter, or field meaning.

## Capture limits

USBPcap retained the same 65,535-byte snapshot limit in every observation.

| Observation | Truncated later OUT records | Omitted payload bytes | OUT pair imbalance |
| --- | ---: | ---: | ---: |
| Cold-01 | 3,219 | 90,132 | 83 |
| Cold-02 | 3,251 | 91,012 | 95 |
| Cold-03 | 3,697 | 103,516 | 64 |
| NoHDMI-01 | 0 | 0 | 0 |
| PowerCycle-01 | 3,203 | 89,684 | 19 |

The small startup bursts are complete. Truncation and missing high-rate submit
records begin only after large streaming starts, so the connected-display files
qualify startup parser work but are not lossless video corpora.

## Remaining gates

No captured OUT payload will be replayed and real hardware writes remain
disabled. The next discriminating experiment should repeat the identical
reported full-power-cycle procedure as `PowerCycle-02`. That can test whether
transfer 11 offset 24 and the later response reordering recur after power loss
or were ordinary run-to-run variation. A second HDMI-absent control should
follow. Only then should a single-session HDMI hotplug test isolate the
transition from small polling to display discovery and large streaming. Later
tests can vary mode and synthetic image one factor at a time.
