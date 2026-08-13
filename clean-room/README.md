# Clean-room hardware probe

This directory contains original source code and uses public macOS IOKit APIs.
It does not include, load, modify, or call DisplayLink software, firmware, keys,
or protocol implementation code.

The current milestone is deliberately read-only. `dock-probe` finds the exact
USB identity observed for a Plugable UD-3900PDZ (`17e9:4323`) and reports its
public interface descriptors. It never opens or claims an interface and never
sends a USB control, bulk, interrupt, or isochronous transfer. It also avoids
reading or printing the dock serial number.

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
