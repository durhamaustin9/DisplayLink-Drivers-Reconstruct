# Collecting external USB-display observations

This guide describes black-box interoperability research for a lawfully owned
dock. It does not authorize activity prohibited by an agreement or local law,
and it is not legal advice. Read [PROVENANCE.md](PROVENANCE.md) first.

The goal is to learn what the device does at its USB boundary. Do not inspect,
translate, or paraphrase a vendor routine. Do not collect protected video,
HDCP sessions, firmware updates, passwords, personal windows, audio, Ethernet
traffic, serial numbers, or monitor-identifying EDID fields.

## Recommended capture environment

In order of fidelity:

1. Use a USB 3/SuperSpeed hardware protocol analyzer between the Mac and dock.
   Confirm that it supports the negotiated speed; a USB 2-only analyzer changes
   the hardware path and cannot qualify SuperSpeed behavior.
2. Use a separate Linux test machine with the official driver and Linux
   `usbmon`. This captures host-controller requests rather than physical bus
   transactions, but it is suitable for discovering transfer structure.
3. Use a separate Windows test machine with Wireshark and USBPcap.

Do not disable System Integrity Protection on the primary Mac just to obtain a
software capture. A trace from another operating system is not automatically a
macOS fact, so record the host and driver exactly and later validate important
findings against a hardware trace from the Mac.

Use a dedicated test account and disconnect unrelated USB devices where
practical. Connect exactly one monitor to **HDMI 2 or HDMI 3** and record which
port. HDMI 1 is the native DisplayPort Alt Mode path and will not expose the
USB-display protocol being studied. Do not connect dock Ethernet, audio,
storage, keyboard, mouse, or charging-only accessories during a trial.

## What to record for every session

Copy [observation-notes-template.md](observation-notes-template.md) into the
ignored `observations-private/` directory and fill it out. Required facts are:

- session identifier and UTC timestamps;
- observer name or stable pseudonym;
- capture method and tool versions;
- host OS/kernel and official driver version;
- dock model, USB `17e9:4323`, and `bcdDevice` revision;
- connection speed and direct/hub topology;
- exact HDMI port, monitor model, cable, mode, and refresh rate;
- initial power/connection/application state;
- a timestamped action log; and
- whether each trial was independently repeated.

Run the repository's read-only probe before the capture and preserve its output
in the private session notes:

```sh
make -C clean-room probe
./clean-room/build/dock-probe
```

The probe intentionally omits device and monitor serial numbers.

## Minimum experiment matrix

Perform one action per capture. Start recording before the action and leave at
least five seconds of idle traffic on each side. Repeat each trial three times.

| Trial | Initial state | One action | Expected observation to record |
| --- | --- | --- | --- |
| A | Dock unpowered, official app ready | Power/connect dock | Cold enumeration and activation |
| B | Dock connected, app stopped | Start official app | Warm activation |
| C | App active, HDMI 2/3 empty | Connect monitor | Head discovery and EDID-related traffic |
| D | Static synthetic black screen | Change to synthetic white | Full-frame/damage transfer shape |
| E | Static synthetic black screen | Change one small rectangle | Damage granularity and addressing |
| F | One supported mode active | Select another documented mode | Mode-setting sequence |
| G | Static screen active | Disconnect monitor | Head removal and recovery |
| H | Static screen active | Unplug and reconnect dock | Hotplug recovery |

Use only source-authored, synthetic full-screen patterns. Never display the
desktop, a browser account, copyrighted video, DRM content, notifications, or
personal data during a capture. Disable notification previews before testing.

Begin with low-risk modes such as 1280×720 at 60 Hz and 1920×1080 at 60 Hz.
Do not test firmware updates, protected playback, high refresh rates, sleep/wake,
or malformed USB writes until parsers and a fake dock exist.

## Linux `usbmon` collection

Linux documents `usbmon` as its USB I/O tracing facility. On a separate test
machine:

```sh
sudo modprobe usbmon
lsusb -d 17e9:4323
sudo dumpcap -D
```

Find the `usbmonN` interface for the bus containing `17e9:4323`, then capture a
single short trial. Replace `N` with that bus number:

```sh
sudo dumpcap -i usbmonN -s 0 -a duration:30 -w trial-A-01.pcapng
sha256sum trial-A-01.pcapng
```

Do not capture `usbmon0` unless the dock cannot be isolated to one bus; it
collects all USB buses and creates unnecessary privacy exposure. If the dock's
device address changes after reconnecting, record the new address in the notes.

`usbmon` commonly emits separate submission and completion events for one host
request. Preserve both and their shared request identifier. Do not deduplicate
or reinterpret them manually.

## Private storage and handoff

Raw `.pcap`, `.pcapng`, analyzer exports, and derived `.dbobs` files must never
be committed or attached to a public issue. Create a local ignored session:

```sh
mkdir -p observations-private/session-A
cp /path/to/trial-A-01.pcapng observations-private/session-A/
cp clean-room/observation-notes-template.md \
  observations-private/session-A/notes.md
shasum -a 256 observations-private/session-A/trial-A-01.pcapng
```

If the capture was made on another computer, copy it locally using removable
storage. Then tell the project maintainer only the relative session directory,
for example `observations-private/session-A/`. The maintainer can inspect it
locally, build a sanitized metadata transcript, and commit only independently
reviewed protocol facts. Do not paste payload bytes into chat.

The current checker validates metadata-only `.dbobs` files:

```sh
make -C clean-room checker
./clean-room/build/transcript-check \
  observations-private/session-A/metadata.dbobs
```

It intentionally does not convert packet captures yet. A converter will be
implemented against the first reviewed capture so its request pairing and
endpoint semantics are based on observed data rather than guesses.

## Evidence quality

A useful protocol fact identifies its source trial and uncertainty. For
example: “In all three Linux trial B captures on revision `3156`, application
start was followed by control-transfer metadata X and bulk-transfer metadata Y.”
It must not claim that a byte means “set mode” until an isolated action changes
that byte consistently, controls do not, and another observer reproduces it.

Hash every raw capture, never edit it in place, and keep interpretation in a
separate document. Ultimately, independent implementation code should consume
a reviewed behavioral specification—not a packet capture or vendor binary.

References:

- [Linux kernel usbmon documentation](https://docs.kernel.org/usb/usbmon.html)
- [Wireshark USB capture setup](https://wiki.wireshark.org/CaptureSetup/USB)
- [Wireshark dumpcap manual](https://www.wireshark.org/docs/man-pages/dumpcap.html)
