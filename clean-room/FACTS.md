# Qualified fact registry

Status: reviewed, metadata-only explanation of the machine-readable
[fact registry](facts.tsv) for the offline protocol-model milestone. The TSV is
authoritative for fact identifier, maturity, disposition, trial count,
contradiction, boundary, and source document. This file supplies the evidence
hashes, exact bounded statements, permitted implementation uses, and explicit
non-claims. Neither file is a protocol specification or evidence of a formal
two-team clean-room process.

The repository maintainers previously inspected a compiled vendor package for
a security audit. The current code is therefore described narrowly as original
interoperability research based on public USB descriptors, controlled
black-box observations, and synthetic tests. A stronger commercial clean-room
claim would require organizational separation, access controls, independent
observers and implementers, written specifications, and legal review. See
[PROVENANCE.md](PROVENANCE.md).

No captured message body, screen content, serial identifier, control setup
packet, firmware, key, vendor implementation detail, or decompiled expression
is recorded here. In the direction/length notation below, `O64` means only a
64-byte host-to-device bulk transfer and `I48` means only a successful 48-byte
device-to-host bulk completion. The numerals are transfer lengths, not payload
values.

## Maturity ladder

The maturity level describes the support available for one narrowly stated
fact. It is not a claim that the protocol is understood, lawful in every
jurisdiction, safe to transmit, or portable to another device.

| Level | Guardrail | Meaning |
| --- | --- | --- |
| E0 | private immutable evidence | A hashed raw capture, private experimental record, or private observer log. It establishes provenance but cannot be committed, embedded, or used as a source-code template. |
| E1 | sanitized observation or limitation | One reviewed public observation, a capture-method limit, or a rejected claim with its contradiction recorded. It may constrain research but does not qualify a protocol rule. |
| E2 | repeated structural candidate | At least two bounded observations support the same metadata structure. It may enter a provisional offline recognizer whose name and output remain observational. |
| E3 | control-qualified structural rule | Repetitions and relevant contrasts qualify a narrow metadata rule for the exact tested device/revision and conditions. It still has no message meaning. |
| E4 | independently validated framing rule | Independent evidence validates message framing rather than only transfer metadata. No current fact has reached E4. |
| E5 | independently validated semantic rule | Controlled independent evidence supports the meaning of a framed message or field. No current fact has reached E5. |
| E6 | independently authored request grammar | Every emitted bit and state precondition is explained by an independent written grammar. No current fact has reached E6. |
| E7 | fake-dock-qualified builder/decoder | Independently authored builders and decoders pass property, fuzz, malformed-input, recovery, and safety gates against a fake dock. No current fact has reached E7. |
| E8 | real-write candidate | E0–E7 provenance plus independent, legal, and safety review make one explicitly scoped operation a candidate for separate real-hardware approval. No current fact has reached E8. |

E2 and E3 permit offline recognition only. E4 is not request-construction
authority, and even E8 requires explicit approval; no maturity level
automatically enables a hardware transport. The current registry contains only
E1 through E3 facts.

## Evidence-source ledger

The descriptor source is the committed, sanitized
[revision-3156 observation](observations/ud-3900pdz-revision-3156.md). The
remaining sources are private native-Windows USBPcap files whose public hashes
and sanitized analyses are recorded in the
[startup/control observation](observations/windows-native-usbpcap-cold-2026-08-17.md)
and the
[HDMI-transition observation](observations/windows-native-usbpcap-hdmi-transitions-2026-08-17.md).
The raw files are intentionally not present in this repository.
Each hashed private file is an E0 evidence object. Only the E1–E3 aggregate
facts below cross the public boundary.

| Source ID | Controlled condition | SHA-256 |
| --- | --- | --- |
| `SRC-COLD-01` | powered-dock USB reconnect, HDMI 2 connected | `5a0e756a4439a443a74f22fe4b8588d515db7b3c792cd5c348a82fd7f13e1df4` |
| `SRC-COLD-02` | powered-dock USB reconnect, HDMI 2 connected | `5b69f4271153626a3882161658e4f189ec4b76901c648e9a5a4a693f5f98f0cc` |
| `SRC-COLD-03` | powered-dock USB reconnect, HDMI 2 connected | `42f199facd0ebd9ba035dd3b3d0ed2438411f86e905304e2ad988c1c7ec3b53d` |
| `SRC-NOHDMI-01` | upstream USB reconnect, HDMI disconnected | `37751e44e8140cbea7a0410e63450be3d43ed5ce9d436c798b95cee87d553a71` |
| `SRC-NOHDMI-02` | upstream USB reconnect, HDMI disconnected | `9837fe2d16c50118bfb108c7827331ac052b5067fc6e27e32eaba67c00e69360` |
| `SRC-NOHDMI-03` | upstream USB reconnect, HDMI disconnected | `0598409a8ca76e3771d7a9888d073ed10462a8084395b9b0dfacf1a1e82429f6` |
| `SRC-POWER-01` | observer-reported full dock power cycle, HDMI 2 connected | `57eb06d7d53aeeafa8f1f847e4f20fca4f81b188a08471bcdfa91a517dd7f14d` |
| `SRC-POWER-02` | observer-reported full dock power cycle, HDMI 2 connected | `600d94b8733a19abbe5c1ca27a5fe9476bc5e35408365052a58c2adaa3130147` |
| `SRC-POWER-03` | observer-reported full dock power cycle, HDMI 2 connected | `099e11cea6419e28d442a01ce34f3102a87f3e91736d76dbd679d2bbfc6be949` |
| `SRC-HDMI3-01` | powered-dock USB reconnect, HDMI 3 connected | `60b21ad18c58b1652892da9cf311098bb0b49405fdd4553ed7c28d7475addaa4` |
| `SRC-HDMI3-02` | powered-dock USB reconnect, HDMI 3 connected | `69e9cc850290afeb81d6362f6e1248131dd32ca24c4bde72e2c045b668f979ee` |
| `SRC-PLUG-01` | observer-reported HDMI 2 insertion without USB reconnect | `f9544169152bcb772dce12b5ac45969dfec96993a726a381e466c1deabe006cd` |
| `SRC-PLUG-02` | observer-reported HDMI 2 insertion without USB reconnect | `4f08b58759c2d375c9aca5a508e6339edfffc6f3786ce3316052bcc8b0b00a7f` |
| `SRC-UNPLUG-01` | observer-reported HDMI 2 removal during large OUT traffic | `e3ba0fabe4acfb1318b9bbb8515e5586e7123195645cbe28c23ec94f2fae8e65` |
| `SRC-UNPLUG-02` | observer-reported HDMI 2 removal during large OUT traffic | `59c2d03952a597f60b65ccb6ee193de5b36001504481b105a3efbf8743164668` |

`SRC-POWER-*`, `SRC-HDMI3-*`, `SRC-PLUG-*`, and `SRC-UNPLUG-*` include physical
conditions reported by the observer. USB packets alone cannot prove when a
power cable or HDMI connector moved, or when a picture became visible.

## Qualified facts

### `FACT-TOPOLOGY-001` — exact public USB boundary

- **Statement:** The tested device identifies as `17e9:4323`, revision
  `0x3156`. Candidate interface 0 is `ff/00/03` with bulk OUT `0x02` and bulk
  IN `0x84`; both endpoints declare a 1024-byte maximum packet, one packet per
  burst, and no streams. Auxiliary interface 1 is `fe/01/01` with no endpoint.
- **Maturity/disposition:** E3, active, for standard descriptor structure on
  this exact unit and revision.
- **Evidence:** the read-only and opt-in standard-descriptor observations in
  the revision-3156 record, corroborated by the device inventories in all 15
  native captures.
- **Allowed use:** exact allowlisting and topology validation before an offline
  record is admitted.
- **Not established:** display semantics, activation, a maximum host-transfer
  size, or support for another revision.

### `FACT-STARTUP-ROLES-001` and `FACT-STARTUP-ORDER-001`

- **Statement:** All 11 reconnect/control observations contain the same first-
  burst direction/length role multiset. Ten use Order A:

  ```text
  O16 O32 O80 I39 O48 I38 O64 I38 O64 I38 I549 I31 O176 I38 I34
  ```

  `SRC-POWER-03` uses Order B:

  ```text
  O16 O32 O80 I39 O48 I38 O64 I38 O64 I549 I38 I31 O176 I38 I34
  ```

  The safe role order is `T1..T9`, then `T10` and `T11` in either order, then
  `T12..T15`.
- **Maturity/disposition:** E3, active, for both the role multiset and bounded
  partial order.
- **Evidence:** `SRC-COLD-01..03`, `SRC-NOHDMI-01..03`,
  `SRC-POWER-01..03`, and `SRC-HDMI3-01..02`; see “Control-qualified first-
  burst shape” in the startup/control observation.
- **Allowed use:** a closed partial-order matcher for these 15 roles and these
  two orders.
- **Not established:** a command boundary, activation semantics, or a right to
  validate or construct role payload values.

#### Associated payload-value exclusion mask

- **Statement:** Role-aligned comparison of the 11 startup/control
  observations found six disjoint observed-variable ranges covering 147 of
  1,285 positions. The ranges are recorded in the startup/control observation;
  no corresponding captured values are public or eligible for validation.
- **Registry association:** this narrows `FACT-STARTUP-ROLES-001`; it is not a
  separate semantic field fact.
- **Evidence:** the same 11 sources as `FACT-STARTUP-ROLES-001`.
- **Allowed use:** documenting that a position was observed to vary so future
  tests do not accidentally treat it as a constant.
- **Not established:** that any observed-matching position is constant, or that
  any varying position is a counter, nonce, key, port, state, or checksum.

### `FACT-STARTUP-TOTAL-ORDER-000` — rejected total-order claim

- **Statement:** The claim that the 15 startup transfers have one exact total
  order is rejected. `SRC-POWER-03` reverses the otherwise adjacent tenth and
  eleventh roles.
- **Maturity/disposition:** E1, rejected, with `powercycle-03` recorded as the
  contradiction in the TSV registry.
- **Allowed use:** a regression test must prove that the old total-order model
  fails and the bounded two-order model succeeds.
- **Not established:** permission for any additional reorder.

### `FACT-IN-ENVELOPE-001` — bounded inbound length envelope

- **Statement:** Every one of the 1,655 complete qualifying payload-bearing IN
  transfers across the 15 native captures had a four-octet envelope: its first
  two octets were zero and its following little-endian 16-bit value equaled the
  number of remaining octets in that transfer.
- **Maturity/disposition:** E3, active, as an observed structural correlation.
- **Evidence:** all source IDs in the ledger; see “Repeated structural
  patterns” and “Capture quality” in the two native observation documents.
- **Allowed use:** validate the four-octet envelope supplied as sanitized
  metadata for a bounded offline record.
- **Not established:** message framing across transfers, command type,
  checksum, encoding, or the meaning of the remaining body.

### `FACT-OUT-ALIGNMENT-001` — positive 16-byte-aligned OUT declarations

- **Statement:** All 43,254 qualifying OUT declarations in the 15-capture
  matrix were positive and divisible by 16.
- **Maturity/disposition:** E3, active, as a tested correlation.
- **Evidence:** all source IDs in the ledger.
- **Allowed use:** reject a malformed offline OUT record before sequence
  matching.
- **Not established:** that the device specification requires this alignment,
  that aligned data is safe, or that padding has any particular meaning.

### `FACT-NO-HDMI-SMALL-001` — three bounded small-only controls

- **Statement:** `SRC-NOHDMI-01..03` retained the qualified startup roles and
  later small polling shapes. In their 7.11–7.48-second observation windows,
  none declared an OUT transfer larger than 224 bytes.
- **Maturity/disposition:** E3, active, for those completed windows.
- **Evidence:** `SRC-NOHDMI-01..03`, contrasted with the eight connected-start
  captures in the startup/control observation.
- **Allowed use:** label a completed, nonempty metadata window whose admitted
  transfers are at most 1024 bytes and which contains no OUT above that source-
  authored ceiling as `small-only-observed`.
- **Not established:** that HDMI is absent, that polling has a known meaning,
  or that future traffic after the window would remain small.

### `FACT-HDMI3-SLOT-001` — early response placement by reported port

- **Statement:** Four corresponding early response slots had length pattern
  `I112, I112, I320, I112` in all six HDMI-2 connected-start observations and
  `I112, I320, I112, I112` in both HDMI-3-only observations. The three HDMI-
  absent controls contained no `I320` in the corresponding slots.
- **Maturity/disposition:** E2, provisional. The cross-condition correlation is
  strong, but only two HDMI-3 trials exist and the physical port labels are
  observer-reported.
- **Evidence:** `SRC-COLD-01..03`, `SRC-POWER-01..03`,
  `SRC-HDMI3-01..02`, and `SRC-NOHDMI-01..03`.
- **Allowed use:** an offline regression fixture documenting the contrasting
  response-length placement.
- **Not established:** a port number, head identifier, selection request,
  display-presence bit, EDID, or mode-setting field.

### `FACT-HOTPLUG-PREFIX-001` — repeated 15-plus-9 prefix

- **Statement:** Both insertion trials contain the same 15-transfer reaction:

  ```text
  I48 O64 I128 I48 O64 I112 O64 I64 O64 I64 I48 O64 I128 O64 I64
  ```

  followed, after an unparsed interval, by the same nine-transfer
  continuation:

  ```text
  I48 O64 I112 O64 I320 I48 O64 I64 O64
  ```

  The safe exact fixture is the 24 roles only; timing is not a matching rule.
- **Maturity/disposition:** E2, provisional. Two repeated trials qualify a
  provisional recognizer, not a general or semantic protocol rule.
- **Evidence:** `SRC-PLUG-01` and `SRC-PLUG-02`; see “HDMI hot-plug boundary” in
  the transition observation.
- **Allowed use:** recognize this exact direction/length prefix in an already
  bounded offline observation.
- **Not established:** the physical insertion instant, monitor discovery,
  EDID, enablement, or visible output.

### `FACT-HOTUNPLUG-PROFILES-001` — closed two-profile union

- **Statement:** The first 29 small records after the last large-write region
  form two observed profiles. Profile U1 is:

  ```text
  I48 O64 I112 O112 I64 I48 O64 I64 O64 I112 I48 O96 I128
  I48 O64 I560 I48 O64 I768 I48 O64 I384 I48 O64 I672 O64 I64 O64 I64
  ```

  Profile U2 is:

  ```text
  I48 O64 I112 O112 I64 O64 I48 I64 O64 I112 I48 O96 I128
  I48 O64 I496 I48 O64 I784 I48 O64 I752 I48 O64 I368 O64 I64 O64 I64
  ```

  The public model is the closed union `{U1, U2}`. It does not accept arbitrary
  `I*` lengths, arbitrary adjacent swaps, or a hybrid assembled from the two
  profiles.
- **Maturity/disposition:** E2, provisional.
- **Evidence:** U1 is from `SRC-UNPLUG-01`; U2 is from `SRC-UNPLUG-02`; see
  “HDMI hot-unplug boundary” in the transition observation.
- **Allowed use:** represent the two exact metadata profiles or their shared
  partial order in an offline recognizer while preserving the closed union.
- **Not established:** a disable request, a disconnect notification, the
  physical removal instant, or a guarantee that large traffic has ended.

### `FACT-LARGE-OUT-BOUNDARY-001` — observational size boundary

- **Statement:** All eight connected-start observations reached sustained
  65,536-byte OUT declarations; all three HDMI-absent controls stayed at or
  below 224 bytes during their bounded windows. Both insertion trials moved
  from small metadata exchanges to OUT declarations above 1024 and then to
  65,536. Both removal trials began with 65,536-byte OUT declarations and were
  followed by their 29-record small profiles; no later large region occurred
  before either capture ended.
- **Maturity/disposition:** E2, provisional. The registry deliberately limits
  this fact to the four in-session transition windows; connected/absent
  controls are corroborating contrasts, not a semantic promotion.
- **Evidence:** all native source IDs, grouped by the two observation
  documents.
- **Allowed use:** classify a bounded observation window as empty,
  `small-only-observed`, `out-above-1024-observed`, or
  `out-65536-observed`.
- **Not established:** HDMI state, video frames, stream start/end, screen
  visibility, pixel encoding, or a reason for any transfer size. Silence does
  not downgrade a classification or prove a transition.

### `FACT-CAPTURE-LIMIT-001` — USBPcap truncation boundary

- **Statement:** USBPcap's 65,535-byte packet snapshot cap included its capture
  header. Consequently each affected 65,536-byte OUT body retained 65,508 body
  bytes and omitted 28. Small startup and transition records used by the
  qualified fixtures were complete and successful. Some high-rate OUT
  submission/completion pairs were imbalanced, as quantified in the
  observation documents.
- **Maturity/disposition:** E1, active, for this capture method and corpus.
- **Evidence:** all connected-start, insertion, and removal source IDs; the
  per-file counts are in the two observation documents.
- **Allowed use:** reject the corpus as a lossless message-body or screen-data
  source while retaining its reviewed small-record metadata.
- **Not established:** the content of omitted bytes or a device-side error.

## Code-rule traceability

The `Q-*` identifiers are implementation rules, not additional hardware facts.
They must remain narrower than their cited evidence.

| Rule ID | Proposed implementation rule | Supporting facts | Intended code boundary |
| --- | --- | --- | --- |
| `Q-TRANSFER-001` | The shared record validator checks bounded syntax and the sanitized IN envelope; named matchers and classifiers then require success, exact `0x02`/`0x84` mapping, and aligned OUT metadata as qualification policy. | `FACT-TOPOLOGY-001`, `FACT-IN-ENVELOPE-001`, `FACT-OUT-ALIGNMENT-001` | `protocol_transfer.h`, its implementation in `partial_order_matcher.c`, plus each consumer policy |
| `Q-MATCHER-001` | Use fixed role arrays and predecessor masks; fail closed on overflow, malformed state, an unexpected role, or premature finish. | Source-authored safety design; not a device fact | `partial_order_matcher.h/.c` |
| `Q-STARTUP-001` | Accept exactly the 15 startup roles and the one observed adjacent-order alternative. | `FACT-STARTUP-ROLES-001`, `FACT-STARTUP-ORDER-001`, rejected `FACT-STARTUP-TOTAL-ORDER-000` | `qualified_sequences.h/.c`, the generic matcher, and first-burst parser tests |
| `Q-MASK-001` | Record only variable-position ranges; never embed matching or varying capture values. | Associated limitation under `FACT-STARTUP-ROLES-001` | `exchange_parser.h/.c` and tests |
| `Q-HOTPLUG-001` | Recognize only the exact repeated 15-plus-9 metadata prefix, with no timing predicate and no semantic output label. | `FACT-HOTPLUG-PREFIX-001` | `qualified_sequences.h/.c` and `transition_parser.h/.c` |
| `Q-HOTUNPLUG-001` | Accept U1 or U2 as complete closed profiles; reject arbitrary `I*`, unauthorized swaps, and cross-profile hybrids. | `FACT-HOTUNPLUG-PROFILES-001` | `qualified_sequences.h/.c` and `transition_parser.h/.c` |
| `Q-REGIME-001` | Classify only an explicitly finished metadata window by observed OUT lengths; never infer a downgrade or HDMI state from silence. | `FACT-NO-HDMI-SMALL-001`, `FACT-LARGE-OUT-BOUNDARY-001` | `traffic_regime.h/.c` |
| `Q-BOUNDS-001` | Cap matcher roles, alternatives, record counts, and declared lengths; allocate no heap memory. | Source-authored safety design; not a device fact | all offline model components |
| `Q-FAKE-001` | Generate bodies synthetically and exercise only the bounded in-memory fake transport. | Source-authored safety design; `FACT-CAPTURE-LIMIT-001` excludes capture replay | protocol-model lab and tests |
| `Q-NOWRITE-001` | Parser completion never calls a transport writer or changes `blocked-protocol-undocumented`. | Project policy; no fact has E4 or greater maturity | state machine and integration tests |

## Claims that remain forbidden

No fact in this registry supports any of the following:

- naming a proprietary message, field, command, authentication step, checksum,
  counter, nonce, key, port selector, EDID exchange, mode set, damage record, or
  frame encoding;
- constructing an OUT request from a captured body or replaying any captured
  byte sequence;
- treating observed-matching payload positions as constants;
- treating a recognized sequence as proof of HDMI presence, visible output,
  stream activation, or stream termination;
- enabling a real USB transport, vendor request, reset, reconfiguration,
  firmware operation, or malformed-input experiment; or
- claiming formal two-team separation, protocol ownership, or compatibility
  beyond the tested Plugable UD-3900PDZ revision `0x3156`.

Promotion of a fact requires a new controlled experiment, its private immutable
source hash, a sanitized written result, review against this registry, and a
separate implementation change. A parser match alone never promotes evidence.
