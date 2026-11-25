# Linux desktop packages

Linux releases contain the complete editor, compiler, C generator, native
Protobuf viewer, converters, language model tools, project creator, device SDKs,
documentation and licenses. x86_64 and ARM64 packages are separate downloads.
ARM64 is the 64-bit Raspberry Pi target; it is not a 32-bit ARM package.

Configure the root project with Ninja, a release build, the matching target
compiler/sysroot and the published SDK directory. `Packaging/BuildLinux.sh`
provides the build frontend. The `UICreatorLinuxPackages` target invokes CPack
and produces both formats for that build's architecture:

```sh
cmake --build build/linux-x86_64 --target UICreatorLinuxPackages --parallel 6
```

The frontend mirrors consumer sources into its Linux cache and keeps a genuine
detached checkout of the locked AdvancedKeyboard dependency inside that mirror.
It verifies the original and generated dependency revisions and refuses local
edits. Earlier source-only dependency mirrors are moved to the cache's
`source-backups` directory before cloning. Git metadata is never copied between
checkouts, and stale source-file cleanup never touches the generated dependency.
Only checkouts created by this frontend can advance automatically to a new lock;
an unrelated checkout at a different revision requires deliberate review.

The frontend also records a target-specific Cargo linker wrapper. Rust native
libraries use the same GCC 11, target sysroot, startup objects and binutils as
the C/C++ build, including ARM64 cross-links. Package rebuilds restore that
recorded environment instead of using the build host's default `cc`.

The outputs are named `LVGL-UI-Creator-0.9.2-Linux-x86_64.deb` and `.tar.gz`,
or `LVGL-UI-Creator-0.9.2-Linux-arm64.deb` and `.tar.gz`. A DEB installs the
application below `/usr/lib/lvgl-ui-creator`, launchers in `/usr/bin`, and a
desktop entry and scalable icon in the standard shared directories. Native
converter code is linked into the editor and `UICreatorConverter` CLI. The
portable archive retains the same relative `usr/` layout; after extraction run
`usr/bin/lvgl-ui-creator` without root privileges.

The DEB also declares the X11/Wayland libraries SDL loads dynamically and Zenity
for native file dialogs. Portable archive users need these desktop dependencies
on their system; the package manifest lists them alongside the measured ABI
requirements. Native converter binaries and the separate project-helper Python
runtime match the package's target architecture.

CPack inspects target ELF files with `readelf` and resolves their shared-library
closure only within the declared build, sysroot and private library directories.
It does not consult the build host's loader cache. Non-system dependencies are bundled privately with origin
relative lookup. glibc, libstdc++, libgcc and libatomic remain system libraries.
OpenGL/EGL/GLX/GLES loader and vendor libraries, GBM and DRM also remain system
libraries so they match the host's Mesa or vendor driver modules; the DEB
declares their runtime packages explicitly.
The DEB's minimum runtime requirements are derived from actual target symbol
versions, including native helper executables and bundled libraries. ARM
dependencies are never resolved with the x86 host's package database. The
package manifest records the measured glibc and GLIBCXX requirements; use these
requirements to select a compatible distribution.
The libstdc++ version mapping follows the [GNU ABI policy](https://gcc.gnu.org/onlinedocs/libstdc%2B%2B/manual/abi.html).

Cross builds set `UICREATOR_LINUX_DEPENDENCY_DIRECTORIES` to a semicolon-separated
list of target sysroot library directories. `UICREATOR_LINUX_READELF` selects the
ELF inspection tool. `UICREATOR_LINUX_SDK_DIRECTORY` selects published SDK archives and
their SHA256 sidecars. Packaging requires the complete SDK set by default;
`UICREATOR_LINUX_REQUIRE_SDKS=OFF` is available for local packaging fixtures.

Every package contains `lib/lvgl-ui-creator/package-manifest.json` below its
installation prefix, with hashes of application files, launchers and desktop
integration. The audit rejects foreign-architecture ELF files, missing tools,
missing helper directories and mismatched SDK checksums. The packaging script
also writes artifact SHA256 sidecars.

Verify an artifact without installing it globally:

```sh
python3 Packaging/Linux/linux_package.py verify \
  --package build/linux-x86_64/packages/LVGL-UI-Creator-0.9.2-Linux-x86_64.deb \
  --architecture x86_64 --output build/linux-package-tests --smoke
```

The test extracts the package, checks every file and ELF architecture, compares
DEB dependency metadata with the measured payload, and starts the actual editor,
compiler, C generator and viewer using SDL's dummy display. For a cross-built
ARM64 package, pass `--architecture aarch64 --emulator qemu-aarch64 --sysroot`
with the target runtime root. The verification report identifies native or QEMU
execution. This verifies startup and rendering without substituting for a test
on Raspberry Pi hardware. Python 3.10.12 or newer supports the complete build
and verification flow, including the backported [safe tar extraction filter](https://docs.python.org/3.10/library/tarfile.html#extraction-filters).
