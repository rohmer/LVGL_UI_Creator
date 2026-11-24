# Native WOFF2 decoding

Source: [google/brotli v1.2.1](https://github.com/google/brotli/releases/tag/v1.2.1),
commit `db24a98aed38a16de78cc05ca15cdd8dc2a0d92d`.

`UPSTREAM.json` records the immutable archive URL, its SHA-256 checksum, and
individual source checksums. The decoder, common implementation and public
headers are unmodified upstream sources. `LICENSE` contains their MIT license.
Version 1.2.1 includes the upstream decoder security fix from that release.

The local CMake file builds static native decoder/common libraries. Its finder
selects those targets for FreeType rather than an optional host dependency,
making WOFF2 support consistent on Windows x86/x64, Linux and macOS. Encoder,
Python, JavaScript and command-line tools are not part of the library.
