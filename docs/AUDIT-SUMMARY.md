# DisplayLink Manager 16.2.39 audit summary

Audit dates: 2026-08-11 through 2026-08-13

This is an evidence summary, not a malware verdict. The current independent
work is in `clean-room/`; the artifact below was examined only to understand the
previous vendor-dependent path.

## Artifact provenance

| Property | Value |
| --- | --- |
| Package | DisplayLink Manager Graphics Connectivity 16.2 |
| Internal version | `16.2.39` |
| Size | `10,398,712` bytes |
| SHA-256 | `fd9eafab9542e592baa39984ed4e87e64e89f3de6b9a4429ab13a2334a7538e6` |
| Plugable-hosted ETag | `98cbc77131b98f31a80061397959e2ad` |
| Installer signer | `Developer ID Installer: DisplayLink Corp (73YQY62QM3)` |
| App signer | `Developer ID Application: DisplayLink Corp (73YQY62QM3)` |

The size, hash, hosted-object ETag, internal version, XAR signature, nested
Mach-O signatures, code-page hashes, signed special slots, and sealed resources
were checked. No mismatch was found. Extracted vendor payloads and signature
blobs are intentionally not published.

## Static behavior

The package is a userspace application, not a DriverKit extension or kext. Its
main executable imports ScreenCaptureKit, CGDisplayStream and AVCapture screen
input, IOSurface, Metal, and IOKit/USB APIs. That is consistent with receiving
desktop frames, processing damage, and sending a display stream over USB.

The official main executable had an App Sandbox client-network entitlement and
raw socket/DNS imports. Static inspection found no identifiable telemetry SDK,
collector, pixel-upload endpoint, screenshot writer, analytics SDK, crash
uploader, or updater. Opaque code and strings prevent exhaustive proof. The
audit therefore does not call the official software spyware and does not claim
that every runtime path was proven safe.

## Corrected hardware evidence

The Plugable UD-3900PDZ appeared as USB `17e9:4323`. A 2026-08-11 session with a
vendor-derived Local app recorded a 1920×1080/60 Hz external display and no
owned TCP/UDP sockets during a brief observation. The physical HDMI port was not
recorded, so this does not conclusively prove that the DisplayLink HDMI 2/3 path
was carrying the display.

An earlier version of this document also claimed that a later 1920×1080/144 Hz
display proved the single-process Core profile worked. On 2026-08-13, the same
external display remained online after all DisplayLink processes were stopped
and the installed wrapper was removed. It was on a native display route, most
likely HDMI 1/DisplayPort Alt Mode. The Core functional-output claim is
retracted. Two successful controller start/reopen/quit cycles established only
the lifecycle behavior of that experiment, not USB display output.

## Stale-service incident

The “foreign DisplayLink executable” alert was caused by a registered
`com.displaylink.XpcService` from an older generated Local bundle. The
controller correctly refused to start rather than terminate a process outside
its owned path. The service was unregistered using `SMAppService` from a
temporary copy of its exact owning bundle. Both known DisplayLink helper labels
then reported absent, no DisplayLink processes remained, and obsolete installed
or generated apps were moved to Trash.

## Conclusion

Containment could remove direct network entitlements, but the renderer remained
opaque proprietary code. It did not satisfy the goal of an independent driver.
The active project has therefore moved to an original read-only IOKit probe and
a staged clean-room design. No independent USB video transport exists yet.
