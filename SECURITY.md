# Security policy

## Project boundary

The active project is original, read-only clean-room hardware research. Its
current probe uses public IOKit registry APIs, does not load vendor code, does
not open a USB interface, and does not send bytes to the dock. It is not yet a
functional display driver.

The repository also retains a historical local containment/audit toolkit. That
toolkit does not replace or make the proprietary DisplayLink rendering
executable auditable. Its primary control was macOS App Sandbox enforcement
after removing direct client/server network entitlements from a pinned input.

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

## Contained controller boundary

The optional `DockBridge.app` is a Finder-launchable wrapper whose Objective-C
source is in this repository. It embeds the exact verified, single-process
compatibility profile under `Contents/Helpers/DockBridge Engine.app`; no
generated controller or nested vendor app is distributed here. Its visible
name, icon, executable name, and bundle identifiers are independently authored,
but the engine payload remains proprietary vendor code.

The controller intentionally runs without App Sandbox or other entitlements.
That is necessary for it to enumerate and signal sibling processes during
shutdown, but it also means macOS does not enforce a no-network boundary on the
controller. Its smaller assurance is source-level: gates reject reviewed direct
networking APIs/classes and explicit network/web framework links. This is not an
exhaustive semantic proof. The nested Core engine retains App Sandbox, USB
access, Apple's local backlight-service lookup, and no direct client/server
network entitlements.

Lifecycle control is path-scoped and fail-closed. The controller launches only
its nested engine, binds the PID to its process start time and canonical path,
and revalidates that identity immediately before each signal. It never selects
a termination target by process name or bundle identifier alone. It contains no
launchd helper and never unloads or mutates a global DisplayLink job. It reports
shutdown as complete only after read-only checks find no known DisplayLink XPC
or crash-restart registration and repeated scans prove the bound identity, all
exact nested processes, and all foreign DisplayLink main/helper processes are
gone. PID reuse, a path change, uncertain process or registration inspection,
or a foreign DisplayLink application/helper causes refusal. The owning official
or Local installation must unregister its own background items; the controller
does not cross that ownership boundary.

The controller supplies the DockBridge status item and **Quit Completely**
action. The vendor status item is requested off through launch preferences.
Automatic login and crash restart are disabled, and the nested profile contains
no XPC helper, crash helper, LaunchAgent, or login item. Launching the nested
engine directly bypasses this lifecycle boundary.

The controller does not request Screen Recording access. That permission and
macOS's observation indicator remain associated with the nested proprietary
engine. Normal cleanup cannot be guaranteed after `SIGKILL`, process-controller
corruption, a system crash, or power loss. Use exact executable paths when
investigating an interrupted shutdown; reboot rather than broadly terminating
ambiguous processes.

The final controller completed two launch/reopen/quit cycles and a fail-closed
foreign-helper test. Those tests establish lifecycle behavior only. The later
144 Hz external-display observation was reproduced with zero DisplayLink
processes and no installed wrapper, so it was a native display route and is not
evidence that Core drove USB graphics. No independent USB-display output has
been qualified.

## Non-goals

This project does not claim that DisplayLink Manager is spyware, that telemetry
was removed, that the result is fully secure, or that screen frames cannot leave
the process through every conceivable mechanism. The static audit found no
telemetry collector, pixel-upload endpoint, screenshot writer, analytics SDK,
or crash uploader, but opaque code prevents exhaustive proof.

The project does not suppress macOS's screen-observation disclosure. Attempting
to bypass that privacy control is out of scope. It also does not claim that the
controller makes opaque vendor code spyware-free or fully secure.

## Reporting a vulnerability

Use GitHub's private security-advisory feature for vulnerabilities in the
original scripts or containment checks. Do not attach DisplayLink packages,
firmware, generated apps, logs containing private data, or display captures.

For vulnerabilities in DisplayLink Manager itself, contact Synaptics through
its official security channel. For macOS vulnerabilities, use Apple's official
security-reporting process.
