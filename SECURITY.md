# Security policy

## Project boundary

This repository is a local containment and audit toolkit. It does not replace
or make the proprietary DisplayLink rendering executable auditable. Its primary
control is macOS App Sandbox enforcement after removing direct client/server
network entitlements from an exact, pinned vendor input.

The source-only repository intentionally contains no vendor package, executable,
firmware, generated application, signing certificate, or captured display data.
The repository hygiene check uses a plain-text source/document allowlist and
fails on private build directories, archives, binaries, links, oversized files,
and known sensitive artifact types.

## What the tested Local profile establishes

- Every input file must match the pinned DisplayLink Manager 16.2.39 manifest.
- App Sandbox remains enabled for all four executables.
- Direct client and server network entitlements are absent.
- The main process retains USB access required for the dock.
- The vendor notarization ticket is removed because it no longer authenticates
  the ad-hoc-signed derivative.
- One limited hardware observation found no TCP or UDP socket owned by the main
  process or XPC service.

This does not prove that every possible IPC or user-mediated side channel is
absent. The app can ask macOS to open a help URL in a separate browser after a
user action. The proprietary executable also remains capable of reading screen
pixels after the user grants macOS Screen Recording permission. App Sandbox
also provides each process a writable private container; removing user-file
entitlements is not a zero-disk-write guarantee.

The dock firmware and packaged vendor firmware resources were not audited. The
DisplayLink design necessarily transmits display frames over USB to that
firmware, so the dock remains inside the trusted path.

## Non-goals

This project does not claim that DisplayLink Manager is spyware, that telemetry
was removed, that the result is fully secure, or that screen frames cannot leave
the process through every conceivable mechanism. The static audit found no
telemetry collector, pixel-upload endpoint, screenshot writer, analytics SDK,
or crash uploader, but opaque code prevents exhaustive proof.

The project does not suppress macOS's screen-observation disclosure. Attempting
to bypass that privacy control is out of scope.

## Reporting a vulnerability

Use GitHub's private security-advisory feature for vulnerabilities in the
original scripts or containment checks. Do not attach DisplayLink packages,
firmware, generated apps, logs containing private data, or display captures.

For vulnerabilities in DisplayLink Manager itself, contact Synaptics through
its official security channel. For macOS vulnerabilities, use Apple's official
security-reporting process.
