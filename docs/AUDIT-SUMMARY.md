# DisplayLink Manager 16.2.39 audit summary

Audit date: 2026-08-11

This document summarizes local findings that motivated the build profiles. It
is not a complete evidence archive, a malware verdict, or an exhaustive proof
of runtime behavior. The repository includes the hashes and verification logic
needed for its build boundary, not every intermediate forensic artifact.

## Audited artifact

| Property | Value |
| --- | --- |
| Package | DisplayLink Manager Graphics Connectivity 16.2 |
| Internal version | `16.2.39` |
| Package size | `10,398,712` bytes |
| SHA-256 | `fd9eafab9542e592baa39984ed4e87e64e89f3de6b9a4429ab13a2334a7538e6` |
| Plugable S3 ETag | `98cbc77131b98f31a80061397959e2ad` |
| Installer signer | `Developer ID Installer: DisplayLink Corp (73YQY62QM3)` |
| Application signer | `Developer ID Application: DisplayLink Corp (73YQY62QM3)` |

The local artifact's size and ETag matched Plugable's official hosted object,
and that ETag also matched the local MD5. Its internal version matched
Synaptics' release notes. Independent verification
of the XAR signature, nested Mach-O signatures, code-page hashes, signed special
slots, and sealed resources found no mismatches. This is strong evidence that
the audited package was authentic and unmodified before analysis.

Users can reproduce the repository's outer SHA-256 and complete 32-file input
check by running the documented `make integration-local` command; the package is
expanded but never installed. The deeper XAR/CMS and Mach-O code-page review used
independent local parsing plus OpenSSL and is reported here as methodology, not
as a one-command reproducibility claim. Extracted certificates, signature blobs,
and vendor payloads are intentionally not published.

## Package architecture

The current package installs a userspace application, not a new kernel extension
or DriverKit system extension. The application contains four executables:

- `DisplayLinkUserAgent`: virtual-display publication, screen capture, frame
  processing/encoding, and USB transport;
- `DisplayLinkXpcService`: local display enumeration and control broker;
- `CrashRestartHelper`: rate-limited restart helper; and
- `DisplayLinkLoginHelper`: login launcher.

The package also contains app-scoped LaunchAgent definitions, device firmware,
Metal shaders, UI resources, and setup media.

## Screen access

The main executable imports ScreenCaptureKit, legacy CGDisplayStream and
AVCapture screen-input paths, IOSurface, Metal, and IOKit/USB APIs. Its rendering
pipeline receives display frames, processes damaged regions, and sends encoded
video records to DisplayLink hardware over USB.

That behavior is expected for DisplayLink HDMI 2/3 on the tested Plugable dock.
It also explains why macOS reports that the screen is being observed even when
the application is not saving a conventional recording.

## Network and storage review

The official main executable had the App Sandbox client-network entitlement and
imports raw socket/DNS functions. Static inspection found no identifiable:

- telemetry or advertising SDK;
- hard-coded collector, crash-upload, or updater endpoint;
- pixel-upload destination;
- screenshot writer or image-writing call path; or
- literal IP address used as a destination.

The visible URLs found were limited to support, setup, forum, feedback, and
product pages.

The executable contains many opaque strings and is proprietary, so plaintext
endpoint searching cannot establish the absence of every dynamically resolved
destination. Local diagnostic logging exists; no upload path was identified.

## Tested containment result

The Local profile removed direct client/server network entitlements while
retaining App Sandbox and USB access. A separate sandbox probe confirmed that a
test process without `network.client` received `EPERM` at the socket connection
boundary.

The repository's source-only package preparation and Local build pipeline was
then run from the pinned installer. Its resulting app tree matched the app used
for the hardware observation byte-for-byte.

During one controlled warm-connected hardware test—meaning the dock and monitor
were attached before app launch—these observations were recorded:

- the Plugable UD-3900PDZ appeared as USB `17e9:4323`;
- one DisplayLink monitor came online at 1920×1080, 60 Hz;
- the main and app-scoped services stayed running;
- neither the main process nor XPC service owned a TCP or UDP socket during the
  observation period; and
- no crash occurred.

The session was brief, but its exact duration, monitor model, and whether HDMI 2
or HDMI 3 was used were not recorded. Those omissions narrow the claim: this was
a functional observation, not a compatibility or long-duration qualification.

The XPC service rejected the ad-hoc main process at its vendor Team-ID check.
That did not prevent the tested display from coming online, but XPC-backed
controls may not work.

## Contained-controller status

The repository also contains independently written source for a
Finder-launchable controller. A local build embeds the exact verified,
single-process Core profile under `Contents/Helpers/DisplayLink Core Engine.app`,
uses the nested engine's existing item as the sole menu-bar UI, and disables
automatic login and crash-restart launch. It binds the Core PID to its process
start time and exact path, and supervises normal Quit until repeated scans prove
that identity and every exact nested process are gone. It also checks the two
known DisplayLink helper registrations read-only and refuses completion if a
foreign DisplayLink main/helper process or registration remains. It never
unloads a global DisplayLink launchd job or terminates by name or bundle
identifier alone.

This outer controller is intentionally unsandboxed so it can enumerate and
signal its sibling process. It has no entitlements; source and build gates reject
reviewed direct networking APIs/classes and explicit networking/web framework
links. That is not an enforced no-network sandbox or an exhaustive semantic
proof. The nested Core process retains App Sandbox, and the nested engine—not
the controller—still requires Screen Recording permission and causes macOS's
observation disclosure.

The standalone Local app was used in the original limited hardware observation.
After the Core identity received Screen Recording authorization, the final
Contained build completed a second narrow run: one active, non-built-in
1920×1080 display at 144 Hz, USB identity `17e9:4323`, two clean
launch/reopen/quit cycles, and zero DisplayLink processes and legacy helper
labels after each quit. A stale Local registration was also introduced during
testing; the controller detected it, stopped Core, and refused to report
success until the foreign state was removed. These are brief functional
observations, not broad compatibility or long-duration qualification.

## Core-profile status

The Core profile statically verifies as a single-executable app with only App
Sandbox, USB, and the local Apple backlight-service exception. Its first A/B
test was blocked because macOS treated the ad-hoc signature as a new Screen
Recording identity. A later authorized run completed the narrow hardware and
lifecycle observations above. Core remains experimental outside that exact
configuration and untested scenarios.

## Conclusions

The audit found no evidence that the official package was modified or that it
uploaded screen pixels. It also cannot prove every behavior in opaque vendor
code. Removing direct network permission is a concrete defense-in-depth control;
it is not proof that the official software was spyware.

The screen-observation notice cannot be removed without removing the userspace
DisplayLink video path or bypassing a macOS privacy safeguard. The native HDMI 1
path on the tested dock avoids DisplayLink capture because it uses DisplayPort
Alt Mode hardware instead.
