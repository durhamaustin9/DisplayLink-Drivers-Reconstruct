# Proposed independent macOS USB-display design

Status reviewed: 2026-08-13

This is a proposed architecture, not a finished driver and not a claim that a
formal two-team clean-room process has already occurred. The only implemented
independent components are the read-only descriptor probe and the bounded,
metadata-only observation parser in `clean-room/`. The parser is protocol
agnostic and carries no captured vendor payload.

## Verified starting point

The attached Plugable UD-3900PDZ exposes USB `17e9:4323`. Public IOKit registry
properties show a vendor-specific interface 0 (`ff/00/03`, two endpoints), an
auxiliary interface 1, standard USB audio interfaces, and USB networking
interfaces. The probe reads those descriptors without opening or claiming an
interface and deliberately omits device and monitor serials.

An independently authored opt-in standard descriptor reader subsequently
confirmed interface 0 uses bulk OUT `0x02` and bulk IN `0x84`, both with a
1024-byte maximum packet size, and interface 1 has no endpoints. This establishes
USB topology, not the meaning or format of messages carried by those endpoints.

This topology is not enough to send pixels. The Ella activation, control,
head/EDID, mode-setting, frame-record, compression, and recovery protocols need
independent documentation for this exact hardware revision.

## Proposed trust boundaries

1. A minimal ScreenCaptureKit producer receives frames and damage metadata and
   intentionally persists no frame at application level. macOS may still swap
   memory and a sandbox retains a writable private container.
2. A bounded, memory-safe codec converts only damaged tiles to independently
   implemented records.
3. A USB transport accepts only exact allowlisted devices and validates every
   length, endpoint, display index, mode, and response.
4. Network access, analytics, updater, login item, crash uploader, user-file
   permission, remote control, and dynamic code loading are absent.
5. Parsers and codec logic are tested first against a fake dock and fuzzed
   corpora. Real-hardware writes are disabled until those gates pass.

## macOS platform boundary

USBDriverKit can own custom USB endpoints if Apple grants the appropriate
entitlement. Public DriverKit documentation does not provide a third-party host
display-output family, however. USB transport alone cannot publish a desktop
display to WindowServer.

A userspace architecture would need virtual-display publication plus a desktop
pixel source. ScreenCaptureKit is the public supported pixel source and requires
Screen Recording approval. The deprecated IOFramebuffer route is not intended
for third-party drivers and would require weaker system security on Apple
silicon. Private display frameworks are not a stable or acceptable foundation.

Therefore a clean implementation can improve transparency and least privilege,
but it cannot honestly promise USB-graphics HDMI 2/3 without macOS classifying
and disclosing screen capture. Native HDMI 1/DisplayPort Alt Mode avoids that
because no application receives the pixels.

## State of related open work

The previously identified Vino research targeted Linux DRM/KMS and a Dell D6000
Ridge device (`17e9:6006`), not this Ella device or macOS. Cached July 2026
material described visible artifacts, low frame rate, and incomplete power/
hotplug behavior. On 2026-08-13 its former GitHub repository URL returned 404,
so it is not an available dependency or a support claim for this project.

Linux EVDI is only a virtual-display component and expects a separate userspace
transport; it is not a DL-3900 USB protocol implementation for macOS. Old
libdlo/udl work targets earlier DisplayLink generations.

## Research and release gates

- record only protocol facts legally obtained for interoperability;
- follow the repository's clean-room provenance rules and keep observer facts
  separate from implementation decisions;
- copy no proprietary code, firmware, resource, key, or protected-media secret;
- preserve provenance for every protocol fact and independently authored file;
- use an exact VID:PID/revision allowlist and never flash firmware;
- fuzz descriptors, control replies, frame records, and disconnect paths;
- bound memory, dimensions, tile counts, transfer lengths, and timeouts;
- verify cold boot, both USB outputs, supported modes, hotplug storms, malformed
  responses, logout, and repeated sleep/wake;
- demonstrate reproducible builds and zero process-owned IP sockets; and
- disclose Screen Recording permission and observation UI accurately.

Apple documentation:

- [DriverKit](https://developer.apple.com/documentation/driverkit)
- [USBDriverKit](https://developer.apple.com/documentation/usbdriverkit)
- [ScreenCaptureKit capture sample](https://developer.apple.com/documentation/screencapturekit/capturing-screen-content-in-macos)
- [IOFramebuffer](https://developer.apple.com/documentation/kernel/ioframebuffer)
