# Proposed independent successor research design

Status reviewed: 2026-08-11

This document proposes a defensible long-term architecture for an independently
implemented DL-3900/Ella userspace driver. It does not claim that a documented
two-team clean-room process has occurred, and no such macOS driver is implemented
in this repository.

Even a clean implementation would not remove macOS's observation state for
DisplayLink outputs: a userspace process would still need composited desktop
pixels.

## Proposed trust boundaries

1. A minimal ScreenCaptureKit producer receives frames and damage metadata from
   the macOS compositor and intentionally performs no application-level frame
   persistence. macOS may still swap memory, and the sandbox retains a writable
   private container.
2. A bounded Metal or memory-safe CPU codec converts only damaged tiles to an
   independently implemented DL3 record stream.
3. A USB-only transport validates every length, endpoint, display index, mode,
   and device response before sending records to the dock.
4. No network entitlement, user-file permission, analytics, updater, login item,
   crash uploader, or remote-control surface exists.
5. Protocol parsers and the tile codec have corpus, fuzz, malformed-device, and
   disconnect tests. Builds are reproducible from source.

## macOS platform boundary

USBDriverKit can provide a strong USB process boundary if Apple grants the
required transport entitlement, but DriverKit exposes no public third-party
display-output family for the host desktop. A virtual display currently relies
on private publication machinery, while the supported userspace pixel source is
ScreenCaptureKit.

A shipping design must not use private frameworks without an explicit supported
contract from Apple. A deprecated IOFramebuffer kernel extension would require
weaker security settings and would not be a security improvement.

## Protocol research gap

As of 2026-08-11, the active open DL3 research project
[Vino](https://github.com/FireBurn/vino-scripts) targets Linux DRM/KMS and a Dell
D6000/Ridge device (`17e9:6006`). The tested Plugable dock is DL-3900/Ella
(`17e9:4323`). Vino does not bind this device or integrate with macOS, and its
[current handover notes](https://github.com/FireBurn/vino-scripts/blob/main/docs/HANDOVER.md)
document rendering, performance, hotplug, and power-state limitations. This is
mutable upstream research, not evidence of support for the Plugable device.

Before implementation, an independent team would need documented Ella cold and
warm activation, endpoint topology, EDID/head selection, control protocol,
frame records, damage encoding, mode changes, sleep/wake, and hotplug behavior.
Do not copy proprietary firmware, keys, or implementation code. Review the
vendor license, HDCP obligations, third-party licenses, and applicable law.

## Release gates

- memory-safe implementation or comprehensive overflow/bounds checks;
- exact USB-device allowlist and least-privilege entitlements;
- no dynamic code loading or writable executable resources;
- fuzzed USB descriptor, control, and video parsers;
- bounded pixel-buffer pools with no intentional application-level frame
  persistence;
- automated runtime evidence of zero process-owned IP sockets;
- exact-hardware cold boot, both outputs, supported modes, hotplug storms,
  logout, and repeated sleep/wake tests; and
- clear user disclosure that the macOS Screen Recording grant and observation
  indicator remain required.
