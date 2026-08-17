# Sanitized native Windows USBPcap reconnect and control observations

Observation date: 2026-08-17

Status: 11 controlled startup observations cover three powered-dock upstream-
USB reconnects with HDMI 2 connected, three HDMI-absent negative controls,
three user-reported full-power-cycle controls with HDMI 2 connected, and two
powered-dock reconnects with HDMI 3 connected. All contain the same 15
first-burst transfer roles, with two observed orders for one adjacent pair.
This qualifies a role-based partial-order fixture for the tested conditions; it
does not identify a command or authorize hardware writes.

The three `Cold` files were USB reconnects while using the same powered dock and
host. For each `PowerCycle` control, the user reported removing both dock power
and upstream USB for 15 seconds. Physical power removal cannot be proven from
USB packets alone.

## Environment and provenance

- Capture boundary: USBPcap on a dedicated Windows computer
- Capture interface: `USBPcap4`
- Dock: Plugable UD-3900PDZ
- USB identity and revision: `17e9:4323`, `bcdDevice 0x3156`
- Connected-display trials: HDMI 2 or HDMI 3, 1920x1080 at 60 Hz
- Test image: synthetic solid black

| Observation | Condition | File size | Duration | SHA-256 |
| --- | --- | ---: | ---: | --- |
| Cold-01 | powered-dock USB reconnect, HDMI 2 connected | 241,949,204 bytes | 27.004839 s | `5a0e756a4439a443a74f22fe4b8588d515db7b3c792cd5c348a82fd7f13e1df4` |
| Cold-02 | powered-dock USB reconnect, HDMI 2 connected | 246,657,256 bytes | 31.058362 s | `5b69f4271153626a3882161658e4f189ec4b76901c648e9a5a4a693f5f98f0cc` |
| Cold-03 | powered-dock USB reconnect, HDMI 2 connected | 283,930,660 bytes | 37.841430 s | `42f199facd0ebd9ba035dd3b3d0ed2438411f86e905304e2ad988c1c7ec3b53d` |
| NoHDMI-01 | upstream USB reconnect, HDMI disconnected | 81,344 bytes | 7.482875 s | `37751e44e8140cbea7a0410e63450be3d43ed5ce9d436c798b95cee87d553a71` |
| NoHDMI-02 | upstream USB reconnect, HDMI disconnected | 79,028 bytes | 7.110250 s | `9837fe2d16c50118bfb108c7827331ac052b5067fc6e27e32eaba67c00e69360` |
| NoHDMI-03 | upstream USB reconnect, HDMI disconnected | 78,488 bytes | 7.448992 s | `0598409a8ca76e3771d7a9888d073ed10462a8084395b9b0dfacf1a1e82429f6` |
| PowerCycle-01 | reported full power cycle, HDMI 2 connected | 245,094,412 bytes | 30.917618 s | `57eb06d7d53aeeafa8f1f847e4f20fca4f81b188a08471bcdfa91a517dd7f14d` |
| PowerCycle-02 | reported full power cycle, HDMI 2 connected | 199,880,412 bytes | 23.372456 s | `600d94b8733a19abbe5c1ca27a5fe9476bc5e35408365052a58c2adaa3130147` |
| PowerCycle-03 | reported full power cycle, HDMI 2 connected | 350,370,876 bytes | 31.853049 s | `099e11cea6419e28d442a01ce34f3102a87f3e91736d76dbd679d2bbfc6be949` |
| HDMI3Only-01 | powered-dock USB reconnect, HDMI 3 connected | 209,074,492 bytes | 24.049099 s | `60b21ad18c58b1652892da9cf311098bb0b49405fdd4553ed7c28d7475addaa4` |
| HDMI3Only-02 | powered-dock USB reconnect, HDMI 3 connected | 224,757,252 bytes | 27.035168 s | `69e9cc850290afeb81d6362f6e1248131dd32ca24c4bde72e2c045b668f979ee` |

The raw pcapng files remain private. They contain device strings, kernel
request identifiers, proprietary messages, and, when HDMI was connected,
screen-bearing traffic. No raw payload, serial string, request address, or
verbatim captured byte sequence is in this repository.

## Topology and timing

Every device descriptor confirmed the exact target. Every configuration
descriptor confirmed vendor interface `ff/00/03`, bulk OUT `0x02`, bulk IN
`0x84`, and 1024-byte maximum packets.

The tables use the first data-bearing burst as their relative origin. Timing is
observational and is not accepted as a parser rule.

| Measurement | Cold-01 | Cold-02 | Cold-03 |
| --- | ---: | ---: | ---: |
| Descriptor request to first burst | 0.200021 s | 0.210007 s | 0.509579 s |
| First-burst duration | 4.530 ms | 5.031 ms | 4.391 ms |
| Second burst start | +0.358931 s | +0.358232 s | +0.358893 s |
| Third burst start | +0.724473 s | +0.726510 s | +0.723770 s |
| Fourth burst start | +1.087312 s | +1.088285 s | +1.086050 s |
| First 65,536-byte OUT | +1.353884 s | +1.369736 s | +1.338693 s |

| Measurement | NoHDMI-01 | NoHDMI-02 | NoHDMI-03 |
| --- | ---: | ---: | ---: |
| Descriptor request to first burst | 0.558187 s | 0.186156 s | 0.522888 s |
| First-burst duration | 4.761 ms | 4.468 ms | 4.509 ms |
| Second burst start | +0.358279 s | +0.361219 s | +0.358110 s |
| Third burst start | +0.724509 s | +0.725880 s | +0.725214 s |
| Fourth burst start | +1.087110 s | +1.087741 s | +1.086762 s |
| First 65,536-byte OUT | none | none | none |

| Measurement | PowerCycle-01 | PowerCycle-02 | PowerCycle-03 |
| --- | ---: | ---: | ---: |
| Descriptor request to first burst | 0.198101 s | 0.219589 s | 0.566013 s |
| First-burst duration | 5.714 ms | 5.146 ms | 4.093 ms |
| Second burst start | +0.358661 s | +0.358001 s | +0.357417 s |
| Third burst start | +0.732823 s | +0.723513 s | +0.724096 s |
| Fourth burst start | +1.094675 s | +1.084891 s | +1.086994 s |
| First 65,536-byte OUT | +1.386496 s | +1.376142 s | +1.338083 s |

| Measurement | HDMI3Only-01 | HDMI3Only-02 |
| --- | ---: | ---: |
| Descriptor request to first burst | 0.212539 s | 0.526043 s |
| First-burst duration | 4.286 ms | 4.661 ms |
| Second burst start | +0.358640 s | +0.358804 s |
| Third burst start | +0.723762 s | +0.723492 s |
| Fourth burst start | +1.085892 s | +1.085894 s |
| First 65,536-byte OUT | +1.332660 s | +1.345763 s |

The first large write is only a transport-phase boundary. The captures contain
no physical monitor-visible marker.

## Control-qualified first-burst shape

All 11 observations have the same 15-role direction/length multiset. Ten use
the following observed order, designated Order A:

```text
O16 O32 O80 I39 O48 I38 O64 I38 O64 I38 I549 I31 O176 I38 I34
```

`PowerCycle-03` uses Order B, in which the adjacent tenth and eleventh roles
appear in the opposite order:

```text
O16 O32 O80 I39 O48 I38 O64 I38 O64 I549 I38 I31 O176 I38 I34
```

`O` means one host-to-device transfer on `0x02`; `I` means one successful
device-to-host completion on `0x84`. These are capture boundaries and lengths,
not command labels. Every first-burst transfer was complete, paired, and
successful.

The qualified model therefore does not require one total order. It names the
Order A roles `T1` through `T15` and accepts the observed partial order
`T1..T9`, then `T10` and `T11` in either order, then `T12..T15`. Role alignment,
rather than physical transfer ordinal, is required before comparing payload
positions.

All 11 second bursts retained one 41-transfer direction/length multiset but
used three observed total orders. Later bursts varied further. No exact ordered
parser fixture is assigned beyond the qualified first-burst partial order.

## Observed-stable and observed-variable positions

Offsets below are zero-based and roles are aligned to Order A. They describe
equality across these 11 observations only; no byte values are published or
embedded.

| Role | Direction/length | Observed-variable offsets | Observed-stable offsets |
| ---: | --- | --- | --- |
| T1–T6, T8–T9, T12, T14 | as listed above | none observed | entire transfer |
| T7 | `O64` | 44–51 | 0–43 and 52–63 |
| T10 | `I38` | 12 | 0–11 and 13–37 |
| T11 | `I549` | 12 and 24 | 0–11, 13–23, and 25–548 |
| T13 | `O176` | 44–171 | 0–43 and 172–175 |
| T15 | `I34` | 26–33 | 0–25 |

Ten of the 15 role payloads were byte-identical across all tested conditions.
Across all 1,285 role-aligned first-burst positions, 1,138 matched and 147
varied in six disjoint ranges. All three power-cycle observations differed from
the other eight at `T11` offset 24. `PowerCycle-03` also introduced the
one-position ranges at `T10` offset 12 and `T11` offset 12. No meaning is
assigned to any of these correlations.

“Observed-stable” does not mean protocol constant. The sample still uses one
dock, host, driver installation, and hardware revision. No authentication,
counter, key, nonce, display flag, boot flag, port identifier, or field purpose
is inferred from either class. The source model records only the variable
ranges and deliberately does not store or validate any captured value.

## Control results

All three HDMI-absent controls retained the qualified first burst and later
small polling bursts. None produced an OUT declaration larger than 224 bytes or
a video-sized transfer during its 7.11–7.48-second capture. The eight connected
observations reached a 65,536-byte OUT 1.333–1.386 seconds after the first burst
and continued with large writes. This repeatable contrast correlates the large-
write transition with the user-reported display connection state; it does not
identify an EDID, mode-set, or enable command.

Within the fourth burst, four corresponding `O64`/response slots had response-
length pattern `I112, I112, I320, I112` in all six HDMI 2 observations and
`I112, I320, I112, I112` in both HDMI 3 observations. The three HDMI-absent
observations had no `I320` in those slots. This is a repeatable port-correlated
placement difference, not evidence that any payload field or response is a
head identifier.

All three reported power-cycle controls retained the role multiset and reached
sustained large writes. They shared the `T11` offset-24 correlation, while
`PowerCycle-03` also used Order B and added two observed-variable positions.
Later-burst ordering varied among these trials. These are repeatable structural
observations, but the USB traces cannot independently establish prior physical
power loss or assign boot-state semantics.

## Repeated structural patterns

Across all 11 observations:

- all 1,431 complete payload-bearing IN transfers had two zero-valued prefix
  bytes followed by a little-endian 16-bit value equal to the remaining body
  length;
- all 33,299 observed OUT declarations were positive and 16-byte aligned; and
- all first-burst endpoint completions reported success.

These are bounded structural correlations, not evidence of a command,
encryption scheme, key, counter, or field meaning.

## Capture limits

USBPcap retained the same 65,535-byte snapshot limit in every observation. OUT
pair imbalance is the absolute difference between observed endpoint-`0x02`
submit and completion records.

| Observation | Truncated later OUT records | Omitted payload bytes | OUT pair imbalance |
| --- | ---: | ---: | ---: |
| Cold-01 | 3,219 | 90,132 | 83 |
| Cold-02 | 3,251 | 91,012 | 95 |
| Cold-03 | 3,697 | 103,516 | 64 |
| NoHDMI-01 | 0 | 0 | 0 |
| NoHDMI-02 | 0 | 0 | 0 |
| NoHDMI-03 | 0 | 0 | 0 |
| PowerCycle-01 | 3,203 | 89,684 | 19 |
| PowerCycle-02 | 2,627 | 73,556 | 10 |
| PowerCycle-03 | 4,458 | 124,824 | 50 |
| HDMI3Only-01 | 2,725 | 76,300 | 66 |
| HDMI3Only-02 | 2,973 | 83,244 | 103 |

The small startup bursts are complete. Truncation and missing high-rate submit
records begin only after large streaming starts, so the connected-display files
qualify startup parser and transition work but are not lossless video corpora.

## Remaining gates

The requested startup/control trace matrix is complete. No additional capture
is needed for the current first-burst and connected/absent/port-transition
milestone. The qualified implementation boundary is the two observed orders,
their role-based partial order, and the six role-aligned variable ranges; timing
and payload values are not parser rules.

No captured OUT payload will be replayed, and real hardware writes remain
disabled. Assigning message semantics or enabling a hardware transfer remains
a separate milestone requiring its own independently justified evidence and
safety review. Future mode, image, hotplug, or long-run experiments may support
later questions, but they are not a gate for this completed structural
milestone.
