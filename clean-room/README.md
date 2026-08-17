# Clean-room hardware probe

This directory contains original source code and uses public macOS IOKit APIs.
It does not include, load, modify, or call DisplayLink software, firmware, keys,
or copied vendor protocol implementation code.

The current milestone is deliberately read-only. `dock-probe` finds the exact
USB identity observed for a Plugable UD-3900PDZ (`17e9:4323`) and reports its
public interface descriptors. It never opens or claims an interface and never
sends a USB control, bulk, interrupt, or isochronous transfer. It also avoids
reading or printing the dock serial number.

The repository also contains a hardware-independent fake transport and device
state-machine skeleton. It models only the standard facts observed for this
exact revision: bulk OUT `0x02`, bulk IN `0x84`, 1024-byte packets, one packet
per burst, and no streams. Its queues are bounded and live only in memory. It
does not link IOKit or IOUSBHost, enumerate a device, or contain a real-hardware
transport kind.

The queue's 1024-byte storage unit is a bounded synthetic chunk, not a maximum
host-transfer claim. Metadata-only replay splits larger observed transfer
lengths across chunks and does not preserve or reconstruct their bodies.

Run the simulator with:

```sh
make fake-lab
```

The simulated state sequence is `offline` → `attached` →
`topology-verified` → `blocked-protocol-undocumented`. The activation request
is an intentional hard barrier: the state machine has no call to the transport
write function, and its test requires both attempted and successful write
counters to remain zero. Synthetic inbound packets can be injected into the
fake dock for future parser tests, but no protocol payload has been invented.

A separate parser now validates only the provisional structure and ordered
direction/length shape of the first observed data-bearing burst at the USB
boundary. Exercise it with:

```sh
make exchange-lab
```

The lab creates deterministic nonzero synthetic transfers, sends them only
through the in-memory fake transport, and requires the parser to reach
`complete` after 15 ordered transfers. It validates the provisional four-byte
IN length prefix and OUT length alignment without assigning any other field or
command meaning. It has no real-device transport. The fixture is provisional
pending two more matching native Windows cold-start captures. The reviewed
facts and capture limitations are recorded in
[`observations/windows-native-usbpcap-cold-2026-08-17.md`](observations/windows-native-usbpcap-cold-2026-08-17.md).

The parser accepts the endpoint address with every transfer and rejects
anything other than bulk OUT `0x02` or bulk IN `0x84`. Any future capture or
hardware integration must additionally bind it to the already verified
`17e9:4323`, revision `0x3156`, interface-0 topology before supplying data.

An additional **opt-in** tool reads the standard USB configuration descriptor
to report endpoint addresses and packet sizes. It allows only `17e9:4323`,
refuses to run while candidate interfaces or the device are owned, and opens
the device with neither capture nor seize options. It may issue the standard
USB `GET_DESCRIPTOR` request if the descriptor is not already cached. It never
configures or resets the device, claims an interface, sends a vendor request,
opens an endpoint, or transfers display data.

First choose **Quit Completely** from DockBridge. Then build and run the reader
with the explicit opt-in command:

```sh
make read-descriptors
```

If DockBridge is running, the command exits safely with an “interfaces are in
use” message and makes no ownership request.

On the observed revision `3156`, the opt-in reader returned:

```text
interface 0 alt=0 class=ff/00/03 endpoints=2
  endpoint 0x02 direction=out type=bulk max-packet=1024
  endpoint 0x84 direction=in type=bulk max-packet=1024
interface 1 alt=0 class=fe/01/01 endpoints=0
```

Both bulk endpoints reported interval zero, one packet per burst, 1024 burst
bytes, and no USB streams. These are standard descriptor facts, not meanings
for activation or frame records.

Build and run it with the dock attached:

```sh
make -C clean-room probe
./clean-room/build/dock-probe
```

The protocol lab also includes an independently authored, bounded parser for
metadata-only observations. It deliberately rejects payload bytes: public
source code and fixtures can describe transfer order, direction, kind,
endpoint, and length without accidentally publishing a vendor message, key, or
screen data. Build the parser with:

```sh
make -C clean-room checker
./clean-room/build/transcript-check /path/to/private-observation.dbobs
```

The next evidence layer is the stricter activation-envelope parser. It records
capture-relative markers around exactly one cold-connect or warm-start action,
then accepts only control endpoint-zero metadata and bulk `0x02`/`0x84`
metadata. It contains no payload field or semantic command labels. Build it
with:

```sh
make activation-check
./clean-room/build/activation-check /path/to/private-activation.dba
./clean-room/build/activation-check --replay-fake \
  /path/to/private-activation.dba
```

Fake replay uses only zero-filled synthetic chunks in the memory transport;
control transfers are not replayed. Read
[`ACTIVATION-ENVELOPE.md`](ACTIVATION-ENVELOPE.md) for the grammar, bounds, and
criteria for turning three private observations into a reviewed public fact.
The first repeated transport-lifetime facts are recorded in
[`observations/windows-arm64-parallels-etw-2026-08-14.md`](observations/windows-arm64-parallels-etw-2026-08-14.md).
Those trials produced sustained HDMI 2 output at 1920×1080/60 Hz, but no human
timestamp was recorded. Their private envelopes therefore label 15 seconds as
a conservative observer-reported `output-stable` upper bound rather than an
exact activation latency. All three pass the parser and zero-filled fake replay.

The version-one text format is:

```text
dockbridge-observation-v1
origin synthetic
device 17e9 4323 3156
interface 0 ff 00 03 0 2
transfer 0 out control 00 64
transfer 1 out bulk 02 4096
transfer 2 in bulk 84 512
```

This example is synthetic and is not a claim about Ella protocol messages.
Captured payloads, firmware, keys, serial numbers, and screen contents must not
be committed. Read [PROVENANCE.md](PROVENANCE.md) before adding an observation.
Follow [COLLECTION-GUIDE.md](COLLECTION-GUIDE.md) for the controlled experiment
matrix, safe capture options, private storage location, and notes template.
Sanitized, reproduced facts are kept under [`observations/`](observations/);
the current Mac observation records revision `3156` and seven interfaces.

Run all hardware-independent classification, parser, transport, and
state-machine tests with:

```sh
make -C clean-room test
```

The vendor-specific interface is described only as a *candidate* display
transport. A descriptor does not document the proprietary activation, control,
mode-setting, or compressed-frame protocol. Consequently this probe does not
light a DisplayLink-connected monitor and is not a replacement display driver.

The next safe research gate is an independently documented protocol transcript
for this exact DL-3900/Ella device. Its records can then be implemented as
bounded parsers and exercised through the existing fake transport before any
code is allowed to send bytes to real hardware. A macOS
implementation would still need a supported way to publish a desktop display
and receive its pixels; current public DriverKit families do not provide a
third-party host display family.
