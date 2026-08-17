# Offline protocol model and bounded fake-transport parsers

Status: implementation specification for the hardware-independent protocol-
model milestone. It turns the reviewed facts in [FACTS.md](FACTS.md) into
bounded recognizers and synthetic tests. It does not construct proprietary
requests, replay captures, access the dock, or replace the state machine's
`blocked-protocol-undocumented` barrier.

This is intentionally a model of *observed USB metadata*, not a model of what
the messages mean. A successful parse says only that an input has one qualified
direction/endpoint/length structure. It does not say “monitor connected,”
“activated,” “frame,” “mode set,” or “safe to transmit.”

## Evidence and provenance boundary

The implementation may consume only:

- public USB identity, interface, endpoint, and packet-size descriptors;
- the fact and rule identifiers in [FACTS.md](FACTS.md);
- transfer direction, endpoint, declared length, success status, and the
  sanitized four-octet IN length envelope;
- source-authored synthetic values used solely by tests; and
- project safety bounds that are labeled as design choices rather than device
  facts.

It must not consume a pcapng file at runtime, retain a proprietary message body,
copy a captured constant, infer a command name, or derive implementation
structure from vendor software. The project does not claim a formal two-team
clean-room process; [PROVENANCE.md](PROVENANCE.md) records that limitation.

## Component boundary

| Component | Responsibility | Explicit exclusion |
| --- | --- | --- |
| `protocol_transfer.h` | Define one payload-free, validated metadata record. | No pointer to or storage for the message body. |
| `partial_order_matcher.h/.c` | Match a bounded predecessor-mask graph using fixed storage. | No timing, payload-value, device-I/O, or semantic rules. |
| `qualified_sequences.h/.c` | Declare the reviewed startup, hot-plug, and hot-unplug role graphs. | No raw captures and no captured body constants. |
| `transition_parser.h/.c` | Expose named structural recognizers with sticky failure and explicit finish. | A match cannot alter the device state machine. |
| `traffic_regime.h/.c` | Classify a completed metadata window only by observed OUT lengths. | No HDMI, picture, frame, or stream-state inference. |
| `protocol_model_lab.c` | Exercise synthetic records and bodies against the matchers and in-memory fake transport. | No IOKit, USBDriverKit, device enumeration, or real transport. |
| `fuzz/transition_parser_fuzz.c` | Mutate source-authored metadata records within hard limits. | No capture-derived fuzz corpus or hardware execution. |

## Payload-free transfer record

One model input contains only the following logical fields:

```text
direction             out | in
transfer kind          bulk
endpoint               direction-compatible USB endpoint address
declared length        positive bounded integer
completion status      success | failure
inbound envelope       present only for in; four sanitized octets or decoded equivalent
```

The record owns no body buffer and has no body pointer. The shared syntax
validator deliberately separates representability from qualification. It
requires a known direction and bulk kind, a nonzero endpoint whose direction
bit agrees with the record, a Boolean success field, a positive length no
larger than 65,536, an all-zero unused IN-prefix field on OUT, and the qualified
four-octet envelope on IN. A failed completion and a direction-compatible but
non-target endpoint are therefore representable metadata, not qualified input.

Each named matcher or classifier applies the stricter policy before accepting
a record:

1. endpoint must be exactly `0x02` for OUT or `0x84` for IN under
   `FACT-TOPOLOGY-001`;
2. completion must report success;
3. an OUT declaration must satisfy `FACT-OUT-ALIGNMENT-001`; and
4. the exact role or classifier-specific length bound must also hold.

These checks validate the observation record, not the dock. They do not make
the remaining body safe or known.

## Fixed bounds

All storage is fixed-size and all bounds fail closed.

| Bound | Value | Basis |
| --- | ---: | --- |
| Roles in one partial-order model | 32 | Source-authored safety bound, not a device fact |
| Allowed lengths for one role | 4, sorted and unique | Source-authored safety bound, not a device fact |
| Concurrent candidate completion masks | 8 | Source-authored ambiguity bound, not a device fact |
| Startup roles | 15 | `FACT-STARTUP-ROLES-001` |
| Hot-plug prefix roles | 24 | `FACT-HOTPLUG-PREFIX-001` |
| Hot-unplug roles | 29 per profile | `FACT-HOTUNPLUG-PROFILES-001` |
| Traffic-classifier records per window | 65,536 | Source-authored denial-of-service bound, not a device fact |
| Maximum declared transfer length admitted by classifier | 65,536 bytes | Narrowly covers `FACT-LARGE-OUT-BOUNDARY-001`; not a device maximum |
| Maximum IN length admitted by classifier and transition models | 1024 bytes | Source-authored safety/model ceiling; not a device maximum |
| Fake-transport chunk | 1024 bytes | Existing bounded synthetic queue unit; `FACT-TOPOLOGY-001` packet size |
| Fake-transport queue capacity | 8 chunks per direction | Existing source-authored bound, not a device fact |

There is no heap allocation, recursion, unbounded search, unbounded queue, or
timestamp-dependent acceptance. Exceeding any bound is an error, never a
partial success.

Every transition parser is additionally bound to the exact verified fake-
machine object, its nonrepeating attachment-generation token, and the fake
transport's nonrepeating lifecycle epoch. Acceptance, finish, and positive
query results require both tokens to remain unchanged and the fake transport
to remain connected and open. The lifecycle epoch advances on each actual
open, close, disconnect, or reconnect state change. Detach, a direct fake-
transport disconnect/reconnect cycle, close/open cycle, or reinitialization
therefore invalidates the binding even when no parser call observed the down
state.

## Partial-order matcher

### Model

Each role contains:

- required direction and endpoint;
- one to four allowed declared lengths;
- a predecessor bit mask naming roles that must already be complete; and
- no payload value or semantic label.

The matcher uses an NFA-style fixed set of at most eight candidate completed-
role masks. When one transfer can legally match more than one ready role, it
branches the masks, deduplicates equal candidates, and rejects candidate
overflow. This permits the few explicitly observed ambiguities without
backtracking, heap allocation, or a permissive wildcard.

### States

```text
waiting -> in-progress -> complete
    \          \             \
     +----------+-------------+-> failed
```

- `waiting`: initialized, with no accepted role.
- `in-progress`: at least one role has been accepted and at least one complete
  candidate remains.
- `complete`: accepting the final role completed exactly one valid candidate.
  A later `finish` is an idempotent confirmation.
- `failed`: sticky terminal error. Further input cannot resurrect the matcher.

Calling `finish` before a complete candidate exists returns an incomplete
result and makes the failure sticky. Calling it on a complete matcher returns
complete without changing state. Input after completion returns a non-success
result without accepting another role. A duplicate role, insertion,
unauthorized order, impossible ambiguity, or corrupted internal state fails
closed.

### Qualified role graphs

The startup graph implements `Q-STARTUP-001`: roles `T1..T9` are ordered,
`T10` and `T11` may complete in either observed order, and `T12..T15` follow.
No other reorder is accepted.

The hot-plug graph implements `Q-HOTPLUG-001`: the observed 15-role reaction
precedes the observed nine-role continuation. The recorded time gap is not a
parser predicate. Because this structure has only two supporting trials, its
result is reported as a provisional metadata match, not a semantic event.

The hot-unplug recognizer implements `Q-HOTUNPLUG-001` as the closed union of
profiles U1 and U2. The matcher may share graph prefixes internally, but it
must preserve profile correlation: it cannot combine the adjacent order from
one profile with the four differing IN lengths from the other. There is no
general `I*` wildcard.

The exact payload-free direction/length sequences and their evidence hashes
are in [FACTS.md](FACTS.md). They must have a one-to-one fact identifier in
source comments or tests.

## Traffic-regime classifier

The classifier consumes valid transfer metadata in an explicitly bounded
observation window. Its public observational states are:

| State | Definition |
| --- | --- |
| `empty` | No valid transfer has been accepted. Explicit finish succeeds and returns `empty`, which carries no device, display, traffic-silence, or lifecycle meaning. |
| `small-only-observed` | At least one record was accepted, every admitted transfer was at most 1024 bytes, and no OUT declaration above 1024 bytes was observed. |
| `out-above-1024-observed` | At least one OUT declaration above 1024 bytes was observed, but none was exactly 65,536 bytes. |
| `out-65536-observed` | At least one 65,536-byte OUT declaration was observed. |
| `failed` | Sticky malformed-record, overflow, or lifecycle failure. |

Within one window the classification can only stay the same or move toward a
larger observed OUT category. `finish` closes a window; it does not
wait for traffic or use elapsed silence. A period containing no large OUT does
not downgrade a prior category and does not prove that a stream ended.

Finishing an empty window is useful for total, deterministic accounting; it is
not evidence that the device was idle, absent, disconnected, or finished.

The 1024 and 65,536 boundaries are descriptive bins for
`FACT-NO-HDMI-SMALL-001` and `FACT-LARGE-OUT-BOUNDARY-001`. Labels such as
`monitor-present`, `streaming`, and
`video-frame` are prohibited. Only an external test harness may define where
one window begins and ends.

## Synthetic fake-transport lab

The lab must be deterministic and hardware-independent:

1. construct a qualified sequence from direction/length metadata;
2. generate nonzero source-authored bodies, such as a repeating test pattern,
   without consulting a capture;
3. for IN records, generate only the four-octet envelope required by the
   structural validator and fill the uninterpreted remainder synthetically;
4. keep declarations larger than 1024 bytes as payload-free classifier
   metadata rather than materializing a body in the fake queue;
5. inject or take small synthetic transfers only through `DBFakeTransport`;
   and
6. assert that real-hardware write paths remain absent while reporting fake-
   transport operations separately.

The model parser itself needs no body, so body generation exists only to
exercise the already implemented fake queue and envelope adapter. A successful
fake replay demonstrates bounds and lifecycle behavior only. It does not
predict that the physical dock would accept the synthetic bytes.

## Required deterministic tests

Positive tests cover:

- the two startup orders;
- the exact 24-role hot-plug prefix;
- each of the two closed hot-unplug profiles;
- every permitted ambiguous-role branch and candidate deduplication;
- each traffic-regime category, including explicit empty-window finish; and
- idempotent finish on a complete partial-order matcher; and
- synthetic small-transfer fake-queue traversal, payload-free large-length
  classification, and absence of every real-device symbol.

Negative tests cover:

- wrong device, revision, interface, endpoint, direction, kind, or status;
- zero, misaligned, oversized, and impossible declared lengths;
- missing, duplicated, inserted, truncated, or unexpected roles;
- every unauthorized adjacent swap;
- a hot-unplug hybrid assembled from U1 and U2;
- malformed, absent, or direction-inappropriate IN envelope metadata;
- premature finish, input after completion, and sticky failure;
- more than 32 roles, more than four lengths per role, unsorted or duplicate
  lengths, invalid predecessor masks, more than eight live candidates, more
  than 65,536 window records, and declarations above 65,536;
- fake-queue exhaustion and undersized read buffers in the transport suite;
- fake-machine detach, direct fake-transport disconnect, reconnect/open, and
  close/open at every prefix of the 24-role profile and both 29-role profiles,
  plus generation- and lifecycle-changing reinitialization checks; and
- deliberately corrupted public parser state where the API permits state
  validation.

All failure cases must preserve memory safety and must not call a transport
writer as a side effect.

## Fuzzing boundary

The deterministic test suite includes a bounded mutation smoke test over
source-authored metadata seeds. An opt-in fuzz target may run longer with
sanitizers. Both obey these rules:

- seeds contain only synthetic `protocol_transfer` records;
- no pcap, private envelope, captured body, device string, or screen data is a
  corpus input;
- the fuzzer cannot link a real USB transport;
- record count and declared length are checked before allocation or iteration;
- success requires a complete named profile, and every malformed path must end
  in a documented non-success state; and
- crashes, hangs, candidate overflow, undefined behavior, and state
  resurrection are failures.

## Completion gates

### Gate A — offline model complete

This milestone is complete only when:

1. every implemented rule maps to a `Q-*` rule and cited `FACT-*` facts;
2. record and matcher types have no proprietary-body storage;
3. startup, insertion-correlated, and removal-correlated fixtures match only
   their documented closed structures;
4. malformed, truncation, disconnect, queue-exhaustion, and corrupted-state
   tests pass;
5. deterministic fuzz smoke, AddressSanitizer, and UndefinedBehaviorSanitizer
   pass;
6. the protocol-model lab links only the fake transport;
7. publication checks find no raw capture or captured payload values; and
8. requesting activation still reaches `blocked-protocol-undocumented` with
   zero real or fake write attempts from that state-machine action.

Gate A does not authorize the next gate.

### Gate B — independently validated framing (E4)

Not satisfied. Promotion requires independent evidence for a bounded message
frame rather than only a USB transfer envelope. Observed equality, correlation,
or parser acceptance is insufficient.

### Gate C — semantics and request grammar (E5–E6)

Not satisfied. Controlled independent evidence must first validate each claimed
message or field meaning at E5. Every outbound bit, length, checksum, state
precondition, timeout, retry, and recovery path must then be generated from an
independently authored and reviewed E6 grammar. No captured body, vendor binary,
firmware, key, or paraphrased vendor routine may supply a constant or template.

### Gate D — fake-dock-qualified builder and decoder (E7)

Not satisfied. Future E6 builders and decoders must pass property, fuzz,
malformed-input, disconnect, retry, recovery, and safety tests against an
independently implemented fake dock. The metadata recognizers in Gate A are not
message builders and do not satisfy E7.

### Gate E — real hardware write candidate (E8)

Not satisfied. In addition to Gates B–D, this requires an isolated real-
transport design, the exact device/revision/topology allowlist, a no-firmware-
flashing invariant, a reviewer who did not derive the implementation from
proprietary code, independent/legal/safety review appropriate to the intended
distribution, and explicit project approval. Parser completion must never
automatically enable this gate.

## Build and verification commands

The implemented integration targets for this milestone are:

```sh
make protocol-model-lab
make -C clean-room test
make -C clean-room test-sanitized
make -C clean-room fuzz-transition
```

The top-level `protocol-model-lab` target builds and runs the deterministic
synthetic demonstration. The normal
`test` target includes the bounded fuzz smoke. `test-sanitized` runs the
hardware-independent model under AddressSanitizer and UndefinedBehaviorSanitizer.
`fuzz-transition` is an opt-in run of 100,000 deterministic metadata mutations;
it still has no real transport and no private corpus. `fuzz-libfuzzer` is an
additional optional target when the selected Clang installation supplies the
libFuzzer runtime; it may run until interrupted. The deterministic target is
the portable required gate.

## Forbidden scope

The offline model must not:

- read raw captures at runtime or publish captured bodies;
- construct, replay, translate, or mutate a proprietary OUT message;
- name an inferred command, field, or cryptographic purpose;
- accept a wildcard merely because two trials differed;
- use timing or silence as proof of a device or display state;
- open, claim, configure, reset, signal, or write to a USB device;
- load a vendor driver, application, firmware, framework, or resource;
- capture the desktop, suppress macOS privacy disclosure, or publish a virtual
  display; or
- claim general compatibility, formal two-team provenance, or readiness as a
  display driver.

The output of this milestone is a safer research instrument: it can say “this
synthetic or sanitized metadata record matches one bounded observed shape” and
nothing more.
