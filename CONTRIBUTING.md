# Contributing

Contributions are welcome when they preserve the source-only and fail-closed
design of the project.

## Before opening a pull request

1. Run `make test` on macOS.
2. Do not add a vendor installer, expanded package, application bundle,
   firmware, Metal library, movie, certificate, signature blob, packet capture,
   crash report, or generated binary.
3. Do not include display EDID serials, Mac serial numbers, usernames, absolute
   home/workspace paths, tokens, keys, or logs containing private window data.
4. Describe exactly which dock, USB VID:PID, chip family, Mac architecture,
   macOS version, ports, modes, and lifecycle scenarios you tested.
5. Never describe an untested device as supported.

## Supporting a new DisplayLink release

A version update requires a fresh provenance review, package hash, complete
source-file manifest, signature audit, static capability audit, entitlement
review, build verification, and hardware qualification. Do not weaken or skip a
hash check merely to accept a newer package.

## Code style

- Shell scripts target the macOS system `zsh` and use `set -euo pipefail`.
- Destructive operations must be limited to validated temporary directories.
- Builders must refuse to overwrite an existing output.
- Verifiers must fail closed on unexpected files, entitlements, versions, or
  bundle identifiers.
- Hardware tests must be explicitly invoked and must not broadly terminate
  unrelated processes.
