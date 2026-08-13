# Plugable USB display clean-room research

Original, source-only macOS research for the USB interfaces in a **Plugable
UD-3900PDZ** dock, plus a retained audit of DisplayLink Manager 16.2.39.

> [!IMPORTANT]
> The independent code in `clean-room/` is not yet a display driver. It is a
> read-only hardware probe: it neither loads DisplayLink software nor sends
> protocol bytes to the dock. This repository is not an open-source DisplayLink driver,
> distributes no DisplayLink package, and does not currently drive the
> dock's USB-graphics HDMI 2/3 outputs.

## Current result

The first independent milestone is implemented and tested:

- recognizes the exact attached USB device, `17e9:4323`;
- enumerates its public USB interface descriptors through IOKit;
- identifies interface 0 as the candidate proprietary display transport;
- leaves standard audio and Ethernet interfaces to macOS;
- does not read or print the dock serial number;
- never opens, claims, resets, or writes to a USB interface; and
- uses no DisplayLink executable, library, firmware, resource, or protocol code.

Build and run it with the dock attached:

```sh
git clone https://github.com/durhamaustin9/DisplayLink-Drivers-Reconstruct.git
cd DisplayLink-Drivers-Reconstruct
make test
make probe
./clean-room/build/dock-probe
```

The observed descriptor topology is:

| Interface | Public descriptor | Current classification |
| --- | --- | --- |
| 0 | vendor class `ff/00/03`, two endpoints | candidate display transport; unopened |
| 1 | application-specific `fe/01/01`, no endpoints | auxiliary interface; unopened |
| 2–4 | USB audio classes | handled by macOS |
| 5–6 | USB networking classes | handled by macOS |

A descriptor is not the display protocol. The activation handshake, endpoint
directions, EDID/head selection, mode setting, frame records, compression,
damage updates, hotplug, sleep/wake, and error recovery for this exact
DL-3900/Ella revision still need independent documentation and tests.

## Hardware boundary and corrected test record

| Item | Observed configuration |
| --- | --- |
| Dock | [Plugable UD-3900PDZ](https://plugable.com/products/ud-3900pdz/) |
| USB identity | `17e9:4323` |
| DisplayLink family | DL-3900 / Ella |
| Mac | MacBook Pro with Apple M3 Pro |
| macOS | macOS 27 beta |
| Attached display | HP Z27H, reported at 1920×1080/144 Hz on 2026-08-13 |
| Independent probe | Device and seven interface descriptors found read-only |
| Independent USB video | Not implemented or demonstrated |

An earlier version of this README incorrectly treated the 144 Hz external
display as proof that the experimental Core derivative was driving HDMI 2/3.
On 2026-08-13 that same display remained online after every DisplayLink process
was stopped and the installed wrapper was removed. It was therefore on a native
display path—most likely HDMI 1/DisplayPort Alt Mode—and cannot qualify the
Core USB-graphics path. That claim is retracted.

A separate 2026-08-11 observation recorded a vendor-derived Local build, USB
`17e9:4323`, and a 1920×1080/60 Hz external display, but the physical HDMI port
was not recorded. It also cannot conclusively prove HDMI 2/3 transport. Treat it
as an audit/lifecycle observation only. This is a narrow functional observation, not a general compatibility or stability claim.

The UD-3900PDZ's **HDMI 1** port uses native DisplayPort Alt Mode. It works
without DisplayLink software and does not require an application to receive
screen pixels. HDMI 2 and HDMI 3 use the DisplayLink USB-graphics path.

## Why a complete replacement is not in this repository yet

There are two independent problems:

1. **Dock transport.** The DL-3900/Ella USB activation and compressed-display
   protocol for `17e9:4323` is not publicly documented. Guessing or replaying
   writes against real hardware would be unsafe and would not be publication-
   quality engineering.
2. **macOS display publication.** USBDriverKit can communicate with custom USB
   endpoints, but Apple's public DriverKit families do not include a third-party
   host display-output family. A userspace implementation therefore still needs
   a virtual-display mechanism and a source of composited pixels. The supported
   pixel source is ScreenCaptureKit, which requires Screen Recording approval
   and causes macOS to disclose that the screen is being captured/observed.

The deprecated IOFramebuffer kernel route is not intended for third-party
display drivers and would weaken security on Apple silicon. Private frameworks
or attempts to suppress the observation indicator are not acceptable project
foundations.

Accordingly, there is no honest supported way to both drive HDMI 2/3 in software
and promise that macOS will not report screen observation. Use HDMI 1, a direct
USB-C/DisplayPort/HDMI connection, or a native Thunderbolt/USB4 display dock if
that disclosure is unacceptable.

Apple references:

- [DriverKit](https://developer.apple.com/documentation/driverkit)
- [USBDriverKit](https://developer.apple.com/documentation/usbdriverkit)
- [ScreenCaptureKit capture sample](https://developer.apple.com/documentation/screencapturekit/capturing-screen-content-in-macos)
- [IOFramebuffer](https://developer.apple.com/documentation/kernel/ioframebuffer)

## Clean-room roadmap

The next stages are gated deliberately:

1. preserve public USB descriptors for this exact hardware without identifiers;
2. document legally obtained, independently observed protocol facts for Ella;
3. build bounded parsers, a fake dock, corpus tests, and fuzz targets;
4. prove cold/warm activation and mode selection without copying firmware,
   executable code, keys, or vendor resources;
5. implement damage encoding and USB transport behind an exact device allowlist;
6. choose a macOS publication/capture architecture with honest privacy UI; and
7. qualify both USB-graphics outputs, hotplug, sleep/wake, malformed responses,
   disconnects, and long-duration operation.

See [`clean-room/README.md`](clean-room/README.md) and the
[proposed design](docs/CLEAN-ROOM-DESIGN.md).

## Retired proprietary containment research

The repository retains source for reproducing the earlier audit and containment
experiments. Those scripts accept a user-supplied, exact DisplayLink Manager
16.2.39 package, verify pinned hashes, remove direct client/server network
entitlements, and build ad-hoc-signed Local/Core derivatives. They still contain
and execute proprietary DisplayLink code. They are **not** the clean-room
replacement requested now and are no longer the recommended path.

For local compatibility testing, the current wrapper is visibly named
**DockBridge** and its nested privacy identity is **DockBridge Engine**. Both use
an original source-generated icon and independent bundle identifiers. The
vendor menu-bar item is requested off; DockBridge supplies its own status menu,
status dialog, and supervised **Quit Completely** action. This is surface
rebranding, not a claim that the proprietary engine was rebuilt from source.
Internal vendor symbols, protocol behavior, firmware, and required legal notices
remain. macOS still requires and discloses Screen Recording access.

The verified application installs system-wide as `/Applications/DockBridge.app`.
Only that global copy should be kept installed; duplicate copies under a user's
`~/Applications` directory can confuse LaunchServices and privacy attribution.

No generated proprietary app is installed by the clean-room probe. On the test
Mac, the stale `com.displaylink.XpcService` registration that caused the
“foreign DisplayLink executable” alert was unregistered through its owning app
bundle, all DisplayLink processes were confirmed stopped, and the obsolete
generated apps were moved to Trash. The alert was a correct fail-closed safety
check: it refused to target an executable owned by a different build.

The historical and DockBridge compatibility tooling remains for reproducibility
and code review:

```sh
make help
```

Do not use it if your requirement is “no DisplayLink software.” Do not
redistribute a generated app.

## Security and privacy

The clean-room probe:

- is original C source using public IOKit/CoreFoundation APIs;
- performs registry reads only;
- has no networking code, updater, analytics, persistence, or login item;
- does not request Screen Recording, Accessibility, or administrator access;
- does not capture frames or create a virtual display; and
- records no serial number, monitor EDID serial, screen content, or local path.

These properties apply to the probe, not to a future functional driver. A future
userspace driver must necessarily receive frames if it drives HDMI 2/3, and the
dock firmware remains part of the trusted path.

Run the repository publication and build checks before every commit:

```sh
make test
make test-clean-room
```

See the [audit summary](docs/AUDIT-SUMMARY.md), [security policy](SECURITY.md),
and [third-party notices](THIRD_PARTY_NOTICES.md).

## License and status

The MIT License applies only to original repository material. It grants no
rights in DisplayLink/Synaptics software, firmware, patents, assets, or
trademarks. The historical transformation scripts may be restricted by the
vendor license or applicable law; review both before using them.

This project is experimental, unofficial, and not production-ready. It is not
affiliated with or endorsed by Synaptics, DisplayLink, Plugable, or Apple.
