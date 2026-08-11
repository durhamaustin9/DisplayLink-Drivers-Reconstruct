# Third-party software and trademarks

This repository contains independently written audit and build tooling only.
It does not distribute DisplayLink Manager, DisplayLink firmware, vendor
resources, or a prebuilt or modified application.

You must obtain DisplayLink Manager from Synaptics and review the license
included with that download. The repository's MIT License applies only to the
original files in this repository. It grants no rights in DisplayLink or
Synaptics software, firmware, assets, patents, certificates, or trademarks.

The scripts create a local, ad-hoc-signed derivative from a package supplied by
the user. They can also embed that exact local derivative inside an independently
written controller application. The resulting outer `.app` still contains
DisplayLink executables, firmware, and resources; calling it “contained” does
not make those materials open source or redistributable. Whether you may use
that process depends on the vendor license and the law in your jurisdiction.
This is not legal advice. Do not redistribute a generated application or attach
one to a GitHub issue or release.

DisplayLink and Synaptics are trademarks of their respective owners. Plugable
and Apple are trademarks of their respective owners. This project is
unofficial and is not affiliated with or endorsed by Synaptics, DisplayLink,
Plugable, or Apple.
