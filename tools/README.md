# Diagnostic source tools

These small programs support audit and verification work. They are not drivers.

Build them locally with:

```sh
make tools
```

Generated binaries go under the ignored `tools/build/` directory.

## `display-status.c`

Enumerates online CoreGraphics displays and prints public display IDs, product
names when available, vendor/model numbers, layout, active state, resolution,
and refresh rate. It intentionally does not print EDID serial numbers.

Reading WindowServer display state may not work from a restricted shell or CI
runner; use it from an ordinary logged-in macOS Terminal session.

## `network-probe.c`

Attempts one nonblocking TCP connection to RFC 5737 TEST-NET-2 address
`198.51.100.1`. That address is reserved for documentation. The probe reports
whether the kernel denied the connection with `EPERM`.

The historical audit wrapped this binary in two otherwise identical sandboxed
test apps, one with `com.apple.security.network.client` and one without it. The
probe is included as reproducibility source, but the repository does not run it
automatically or make any external connection during CI.

## `macho-payload.c`

Emits a canonical byte stream for one thin, signed 64-bit Mach-O file. It omits
only the final embedded code-signature blob and normalizes the three length
fields that `codesign` necessarily changes. Every other header, load-command,
executable, data, and pre-signature `__LINKEDIT` byte remains covered.

The Local and Core verifiers compile this small helper into a temporary
directory and compare each `x86_64` and `arm64` payload with the pinned manifest.

## `rename-excl.c`

Moves a completed staging tree into place with macOS's exclusive atomic rename
flag. Preparation and build scripts compile it inside their temporary staging
directory so a raced file, directory, or dangling symlink can never be
overwritten or populated as the final output.
