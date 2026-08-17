# Sanitized native Windows USBPcap HDMI transition observations

Observation date: 2026-08-17

Status: two user-reported HDMI 2 hot-plug trials and two user-reported HDMI 2
hot-unplug trials reproduced bounded USB metadata transitions on the tested
dock. The four trials qualify metadata-only transition fixtures for the tested
conditions. They do not qualify a payload parser, identify a command, authorize
a hardware write, or establish the time of the physical action or visible
output.

## Scope and provenance

- Capture boundary: USBPcap on a dedicated Windows computer
- Capture interface: `USBPcap4`
- Dock: Plugable UD-3900PDZ
- USB identity and revision: `17e9:4323`, `bcdDevice 0x3156`
- Candidate display interface: `ff/00/03`, bulk OUT `0x02`, bulk IN `0x84`
- Endpoint maximum packet size: 1024 bytes
- User-reported display connection: HDMI 2, 1920x1080 at 60 Hz
- Test image: synthetic solid black

| Observation | User-reported action | File size | Duration | SHA-256 |
| --- | --- | ---: | ---: | --- |
| HDMIHotPlug-01 | connect HDMI 2 without reconnecting USB | 188,495,312 bytes | 27.460817 s | `f9544169152bcb772dce12b5ac45969dfec96993a726a381e466c1deabe006cd` |
| HDMIHotPlug-02 | connect HDMI 2 without reconnecting USB | 224,927,912 bytes | 32.948759 s | `4f08b58759c2d375c9aca5a508e6339edfffc6f3786ce3316052bcc8b0b00a7f` |
| HDMIHotUnplug-01 | disconnect HDMI 2 while large writes were active | 89,030,004 bytes | 20.747156 s | `e3ba0fabe4acfb1318b9bbb8515e5586e7123195645cbe28c23ec94f2fae8e65` |
| HDMIHotUnplug-02 | disconnect HDMI 2 while large writes were active | 102,288,020 bytes | 10.655449 s | `59c2d03952a597f60b65ccb6ee193de5b36001504481b105a3efbf8743164668` |

The raw pcapng files remain private. They contain device strings, kernel
request identifiers, proprietary messages, and screen-bearing traffic. No raw
payload, serial string, request address, or verbatim captured byte sequence is
included here or in the source fixture.

## Device boundary

Each transition file begins with USBPcap's zero-time inventory records for the
already attached bus. The target inventory contains the exact device and
configuration descriptors and confirms interface 0 and endpoints `0x02` and
`0x84`. These records establish which existing device carried the observed
traffic; they are not evidence of a physical USB enumeration at capture start.

Neither HDMI transition caused a USB device re-enumeration. The hot-plug and
hot-unplug transitions occurred within the existing device session. Standard
`SET_INTERFACE` activity for interface 3 also occurred near every transition,
but that interface is part of the USB audio topology. Its timing is useful
corroboration and is not treated as a required display rule.

## HDMI hot-plug boundary

The physical insertion was not marked in either trace. The exact boundary used
below is the first data-bearing `0x02`/`0x84` reaction, not the time at which the
connector moved.

| Measurement | HDMIHotPlug-01 | HDMIHotPlug-02 |
| --- | ---: | ---: |
| First observed reaction | +8.623316 s | +7.619878 s |
| End of repeated 15-transfer reaction | +8.751247 s | +7.746951 s |
| Reaction duration | 127.931 ms | 127.073 ms |
| Start of the next metadata burst | +8.920841 s | +7.917414 s |
| Next burst relative to first reaction | +297.525 ms | +297.536 ms |
| First OUT declaration larger than 1024 bytes | +9.003292 s | +8.001821 s |
| Larger declaration relative to first reaction | +379.976 ms | +381.943 ms |
| First 65,536-byte OUT declaration | +9.045433 s | +8.057802 s |
| 65,536-byte declaration relative to first reaction | +422.117 ms | +437.924 ms |

HDMIHotPlug-02 contains one earlier no-monitor polling burst at
`+1.391182` to `+1.452402` seconds. HDMIHotPlug-01 contains no data-bearing
candidate-display transfer before the transition at `+8.623316` seconds. The
human action therefore cannot be localized more precisely than the unmarked
interval preceding the first reaction.

### Repeated direction/length shape

Both trials contain the same first 15 data-bearing transfers:

```text
I48 O64 I128 I48 O64 I112 O64 I64 O64 I64 I48 O64 I128 O64 I64
```

`O` means a host-to-device transfer on `0x02`; `I` means a successful
device-to-host completion on `0x84`. These symbols describe capture direction
and declared length only. This exact shape does not occur in the three
connected-start reconnect baselines or the three HDMI-absent reconnect
controls.

After an approximately 170-millisecond idle interval, both trials share this
exact nine-transfer continuation:

```text
I48 O64 I112 O64 I320 I48 O64 I64 O64
```

The remaining pre-stream traffic differs in response lengths and in the number
of repeated `O64`/`I64` exchanges. Both trials preserve only the following
safe partial order:

```text
repeated 15-transfer reaction
  -> repeated 9-transfer continuation
  -> variable small metadata exchanges
  -> O32 -> I992
  -> optional repeated small exchanges
  -> O1088
  -> response exchanges
  -> O576
  -> O65536
```

An ordered parser must not extend an exact fixture beyond the repeated
24-transfer prefix. The milestone order is observational and assigns no
meaning to any transfer.

### Observed-variable position mask

Offsets are zero-based half-open ranges. They compare only HDMIHotPlug-01 and
HDMIHotPlug-02 and contain no captured values.

| Transfers | Direction/length | Observed-variable ranges |
| --- | --- | --- |
| 1, 4, 11 | `I48` | `[12,14)`, `[16,48)` |
| 2, 5, 7, 9, 12, 14 | `O64` | `[12,14)`, `[16,64)` |
| 3 | `I128` | `[10,11)`, `[12,14)`, `[16,128)` |
| 6 | `I112` | `[10,11)`, `[12,14)`, `[16,112)` |
| 8, 15 | `I64` | `[12,14)`, `[16,64)` |
| 10 | `I64` | `[12,14)`, `[16,34)`, `[35,64)` |
| 13 | `I128` | `[10,11)`, `[12,14)`, `[16,39)`, `[40,85)`, `[86,128)` |

None of the 15 same-shaped payloads was byte-identical between the two trials.
Across 1,088 positions, 878 varied in 36 ranges and 210 happened to match. The
nine-transfer continuation likewise had 720 varying positions out of 848.
Observed matching positions are not protocol constants. No source parser
stores or checks their captured values.

### Standard control correlation

Both trials successfully selected interface 3 alternate settings in the order
`2`, `0`, `2` shortly before the first declaration larger than 1024 bytes. A
later successful selection of alternate setting `0` occurred about 1.06
seconds after the first reaction. Because interface 3 is an audio interface and
only two trials exist, these standard control operations are not part of the
candidate display-transition fixture.

## HDMI hot-unplug boundary

Both hot-unplug captures start while sustained large OUT traffic is already
active. The physical disconnect was not marked. The first safe boundary is the
end of the observed large-write region followed by the first small transition
completion.

| Measurement | HDMIHotUnplug-01 | HDMIHotUnplug-02 |
| --- | ---: | ---: |
| First member of active large-write region | +0.017675 s | +0.007051 s |
| Last member of active large-write region | +10.365144 s | +10.433489 s |
| First post-stream small transition | +10.437190 s | +10.471987 s |
| Quiet boundary between those observations | 72.046 ms | 38.498 ms |
| End of repeated 29-transfer transition | +10.606277 s | +10.654990 s |
| Repeated-transition duration | 169.087 ms | 183.003 ms |
| No-large continuation after the last stream write | 10.382012 s | 0.221960 s |

No large-write region resumes before either capture ends. HDMIHotUnplug-01
continues long enough to show one exact five-transfer no-monitor poll and a
later ten-transfer poll whose shape differs from the corresponding HDMI-absent
poll by one response length.

### Repeated partial order

The two trials have one adjacent ordering difference and four differing IN
lengths. Their shared 29-transfer partial order is:

```text
I48 -> O64 -> I112 -> O112 -> I64
 -> {I48 || O64} -> I64 -> O64 -> I112
 -> I48 -> O96 -> I128
 -> I48 -> O64 -> I*
 -> I48 -> O64 -> I*
 -> I48 -> O64 -> I*
 -> I48 -> O64 -> I*
 -> O64 -> I64 -> O64 -> I64
```

`{I48 || O64}` records that the adjacent capture order reverses between the
two trials. Each `I*` records a response whose observed length differs between
the trials. It is not a wildcard suitable for accepting arbitrary input. No
same-shaped hot-unplug payload was byte-identical between the two captures.

The standard interface-3 alternate-setting sequence near this boundary was
`2`, `0`, `2`, `0` in both trials. It remains corroborating USB audio metadata,
not an inferred display-disable sequence.

## Capture quality

| Observation | Payload-bearing OUT | Payload-bearing IN | Truncated later OUT | Omitted bytes | OUT pair imbalance |
| --- | ---: | ---: | ---: | ---: | ---: |
| HDMIHotPlug-01 | 3,092 | 68 | 2,487 | 69,636 | 78 |
| HDMIHotPlug-02 | 3,704 | 74 | 3,004 | 84,112 | 0 |
| HDMIHotUnplug-01 | 1,475 | 48 | 1,152 | 32,256 | 6 |
| HDMIHotUnplug-02 | 1,684 | 34 | 1,297 | 36,316 | 14 |

All complete payload-bearing IN transfers in these four observations use the
previously recorded four-byte length envelope. Every OUT declaration is
positive and 16-byte aligned. Every small transition record described above is
complete and successful. The hot-plug 15-transfer reactions are also fully
paired in both captures.

Combined with the 11 startup/control files, the complete 15-capture matrix has
1,655 qualifying payload-bearing IN transfers and 43,254 positive,
16-byte-aligned OUT declarations. These aggregate correlations still assign no
meaning to a message body.

USBPcap's snapshot limit truncates each listed later 65,536-byte OUT body by 28
bytes. Truncation begins only with large streaming traffic. It does not affect
the small transition metadata, but these files are not lossless screen-data
corpora.

## Reconnect and port controls

The separate
[reconnect and control observation](windows-native-usbpcap-cold-2026-08-17.md)
records the canonical startup path and its payload-free position mask. Two
additional HDMI-absent controls extend that comparison without changing the
startup shape or its established mask.

| Observation | Condition | File size | Duration | SHA-256 |
| --- | --- | ---: | ---: | --- |
| NoHDMI-01 | HDMI disconnected | 81,344 bytes | 7.482875 s | `37751e44e8140cbea7a0410e63450be3d43ed5ce9d436c798b95cee87d553a71` |
| NoHDMI-02 | HDMI disconnected | 79,028 bytes | 7.110250 s | `9837fe2d16c50118bfb108c7827331ac052b5067fc6e27e32eaba67c00e69360` |
| NoHDMI-03 | HDMI disconnected | 78,488 bytes | 7.448992 s | `0598409a8ca76e3771d7a9888d073ed10462a8084395b9b0dfacf1a1e82429f6` |
| HDMI3Only-01 | user-reported HDMI 3 only | 209,074,492 bytes | 24.049099 s | `60b21ad18c58b1652892da9cf311098bb0b49405fdd4553ed7c28d7475addaa4` |
| HDMI3Only-02 | user-reported HDMI 3 only | 224,757,252 bytes | 27.035168 s | `69e9cc850290afeb81d6362f6e1248131dd32ca24c4bde72e2c045b668f979ee` |

All three HDMI-absent controls retain the canonical first bursts and produce no
OUT declaration larger than 224 bytes. Both HDMI-3-only controls retain the
canonical startup burst and progress to sustained 65,536-byte OUT declarations.

One early four-slot response ordering correlates with the user-reported port:

| Reported condition | Four responses following the corresponding `O64` slots |
| --- | --- |
| HDMI 2 connected-start baseline | `I112`, `I112`, `I320`, `I112` |
| HDMI 3 only | `I112`, `I320`, `I112`, `I112` |
| HDMI absent | no `I320` in the corresponding early slots |

This is a bounded ordering correlation, not a port number, head identifier,
EDID field, or selection command. The physical port labels are user-reported
and are not independently encoded by USBPcap.

Across the full startup/control matrix, role-aligned comparison yields six
disjoint observed-variable ranges totaling 147 positions. The extra two
positions arise only when the adjacent pair in PowerCycle-03 is aligned to its
canonical direction/length roles. Neither HDMI-3-only trial nor either new
HDMI-absent control expands that mask. The exact table and alignment caveat are
kept in the reconnect/control observation rather than duplicated here.

## Safe conclusions and remaining gates

The three HDMI-absent controls, three HDMI-2 connected-start baselines, two
HDMI-3-only controls, two in-session hot-plug trials, and two in-session
hot-unplug trials consistently associate HDMI presence with sustained large
OUT traffic. The in-session transitions occur without USB re-enumeration and
have repeatable bounded direction/length structure.

These observations do not establish:

- the time of physical insertion or removal;
- the time at which a monitor became visible or went dark;
- a command, field, checksum, counter, nonce, key, or authentication exchange;
- EDID, mode-setting, head-selection, compression, or frame-record semantics;
- a captured byte sequence safe to send to hardware; or
- general behavior beyond this dock, host, driver installation, and hardware
  revision.

A metadata-only candidate parser may recognize the exact hot-plug 15-transfer
reaction and nine-transfer continuation and may represent the hot-unplug
29-transfer partial order. It must bind input to the already verified device,
revision, interface, and endpoints; validate only public framing and bounded
metadata; and retain a hard no-write boundary. No additional capture is needed
for the current partial-order milestone. A third independently marked trial in
each direction would be required only before promoting either transition to an
exact ordered parser fixture. Captured OUT payload replay and real hardware
writes remain prohibited.
