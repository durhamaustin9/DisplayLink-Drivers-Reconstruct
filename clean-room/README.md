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

Run the hardware-independent classification tests with:

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
