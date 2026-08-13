# Clean-room hardware probe

This directory contains original source code and uses public macOS IOKit APIs.
It does not include, load, modify, or call DisplayLink software, firmware, keys,
or protocol implementation code.

The current milestone is deliberately read-only. `dock-probe` finds the exact
USB identity observed for a Plugable UD-3900PDZ (`17e9:4323`) and reports its
public interface descriptors. It never opens or claims an interface and never
sends a USB control, bulk, interrupt, or isochronous transfer. It also avoids
reading or printing the dock serial number.

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

Run all hardware-independent classification and parser tests with:

```sh
make -C clean-room test
```

The vendor-specific interface is described only as a *candidate* display
transport. A descriptor does not document the proprietary activation, control,
mode-setting, or compressed-frame protocol. Consequently this probe does not
light a DisplayLink-connected monitor and is not a replacement display driver.

The next safe research gate is an independently documented protocol transcript
for this exact DL-3900/Ella device, followed by parsers and a simulator that can
be fuzzed before any code is allowed to send bytes to real hardware. A macOS
implementation would still need a supported way to publish a desktop display
and receive its pixels; current public DriverKit families do not provide a
third-party host display family.
