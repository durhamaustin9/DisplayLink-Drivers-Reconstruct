# Activation exchange metadata envelope

Status: parser and fake replay implemented; no real activation capture has been
provided or documented yet.

This format records the externally observable *shape* of one activation trial
without recording USB payload bytes, control-request setup fields, keys,
firmware, screen content, or inferred command meanings. A valid black-box file
is evidence of ordering, timing, direction, endpoint, transfer type, and length
only. It is not evidence that a transfer means “initialize,” “authenticate,”
“set mode,” or any other semantic label.

Raw captures and completed `.dba` envelopes are private research artifacts and
must remain under ignored `observations-private/`. Only independently repeated,
reviewed aggregate facts should be committed to `observations/`.

## Version-one grammar

```text
dockbridge-activation-envelope-v1
origin black-box
device 17e9 4323 3156
action warm-start
event 0 0 marker capture-start
event 1 500000 marker action-issued
event 2 500100 transfer out control 00 64
event 3 500200 transfer out bulk 02 2048
event 4 500300 transfer in bulk 84 1024
event 5 900000 marker output-stable
event 6 5900000 marker capture-end
```

The example values are synthetic placeholders, not observations.

- Event sequence numbers start at zero and are contiguous.
- Timestamps are capture-relative, monotonic microseconds.
- Markers occur exactly once and in the order shown.
- `action` is `cold-connect` or `warm-start`.
- `origin` is `black-box` for real external observations or `synthetic` for
  tests. `public` is deliberately rejected for this hardware-specific claim.
- Control metadata uses endpoint `00`; its setup packet is not represented.
- Bulk metadata is restricted to OUT `02` or IN `84`.
- No payload field exists. An added field causes rejection.
- An envelope is incomplete unless at least one transfer occurs between
  `action-issued` and `output-stable`.

The parser stores at most 4,096 events, accepts at most 16 MiB for one transfer,
and caps the activation-window byte total at 64 MiB. These are parser safety
bounds, not claims about what the dock accepts.

## Validate and replay safely

Build the checker:

```sh
make activation-check
```

Validate a private envelope:

```sh
./clean-room/build/activation-check \
  observations-private/session-B/activation.dba
```

Exercise only its bulk transfer sizes/directions through zero-filled packets in
the bounded fake transport:

```sh
./clean-room/build/activation-check --replay-fake \
  observations-private/session-B/activation.dba
```

Fake replay never uses captured bytes. It partitions each bulk transfer into
at most 1024-byte zero-filled packets, writes/reads only the in-memory fake
queues, and reports the modeled byte totals. Control transfers are counted as
`control-not-replayed` because endpoint-zero setup metadata is absent.

This tool has no converter from a packet capture. That converter must be
written only after a real capture method and its submission/completion pairing
rules are known. Manual transcription should be independently checked against
the capture and repeated trials before any aggregate is published.

## Real-evidence completion criteria

The first real exchange is documented only when:

1. the observer follows `COLLECTION-GUIDE.md` on a supported external capture
   environment and records one action on HDMI 2 or HDMI 3;
2. three trials with the same starting state are captured and hashed;
3. timestamps and transfer metadata are transcribed without payloads;
4. the envelope checker accepts each trial;
5. transfer submission/completion pairing is reviewed for that capture tool;
6. stable ordering facts are separated from trial-specific timing/lengths; and
7. no command meaning is claimed without a controlled comparison that supports
   it.

Until then, the state-machine activation barrier remains unchanged and real
hardware writes remain absent.
