# macOS cross builds and app archives

The macOS build runs on Linux x86-64, including Ubuntu in WSL2 on Windows.
It downloads an Apple macOS SDK, pinned OSXCross, LLVM, CMake, Rust target
libraries and native macOS converter runtimes. It builds separate Intel
`x86_64` and Apple Silicon `arm64` applications. Both require macOS 13.5 or
newer because the bundled Node 24 runtime requires that version.

The download/bootstrap workflow is tested on Ubuntu 26.04 LTS with Python
3.12 or newer. It uses the distribution's Clang/LLVM/LLD 21.1.8 packages;
other distributions must provide that version and the host prerequisites
listed in `BuildMacOSTools.py` before running with `--prepare`.

From a Linux shell in the source checkout:

```sh
./Packaging/BuildMacOS.sh --prepare --install-dependencies --package
```

From Windows PowerShell with the checkout at `E:\source\LVGL_UI_Creator`:

```powershell
wsl.exe -d Ubuntu -- bash /mnt/e/source/LVGL_UI_Creator/Packaging/BuildMacOS.sh --prepare --install-dependencies --package
```

`--install-dependencies` permits installation of missing Linux build packages;
subsequent builds can omit it. `--prepare` downloads and verifies build inputs
and prepares both architectures. Later incremental builds can omit `--prepare`.
Every build refreshes source-owned converter/project scripts and wrappers from
the mirrored source snapshot, without redownloading or changing prepared native
runtimes. `--skip-sync` keeps that existing source snapshot for the refresh.
Use `--architecture x86_64` or `--architecture arm64` to build one target.
Use `--base /path/to/cache` to relocate the isolated toolchain/build cache,
`--jobs 6` to choose build concurrency and `--output /path/to/packages` to put
both architectures' archives in a common directory. Run `--help` for all options.

The default cache is `/var/tmp/lvgl-ui-macos`. Build trees are
`build-x86_64` and `build-arm64` inside it. Each contains `configure.log`,
`build.log`, toolchain metadata and a `packages` directory. Packages are named:

```text
LVGL-UI-Creator-<version>-macOS-x86_64.zip
LVGL-UI-Creator-<version>-macOS-x86_64.tar.gz
LVGL-UI-Creator-<version>-macOS-arm64.zip
LVGL-UI-Creator-<version>-macOS-arm64.tar.gz
```

Each has a SHA256 sidecar. `macOS-<architecture>.manifest.json` records every
payload file, Mach-O architectures and deployment versions, resolved dylib
dependencies, signed capabilities, integrity checks and downloaded artifact
provenance. `verification-<architecture>` contains staging/signing logs.
The LLVM compiler-rt builtins runtime used by SDL's availability checks is
statically linked; its license is included in the application notices.
`buildSource` records the source commit and whether the checkout had changes,
so a Mac build added after a version tag can be traced to its actual sources.

The normal build downloads the existing device SDK archives from the matching
published application release, verifies the release manifest checksums and
includes them in the application. Supply `--sdk-directory /path/to/sdk-inputs`
for an existing set of those archives and SHA256 sidecars, or `--sdk-release`
to use an explicitly selected published release during development.
`--without-sdks` creates a development package without those archives.
Apple's SDK and the host cross compiler stay in the build cache; they are not
included in the app archive.

## Application contents

`LVGL UI Creator.app/Contents/MacOS` contains `UICreator`, `UICreatorViewer`,
`LVGLCompiler`, `UICreatorCodeGen` and `UICreatorModelGen`.
`Contents/Frameworks` contains the protobuf display library and required
non-system dylibs. `Contents/Resources` contains the application icon,
Advanced Keyboard language models, ESPOS integration sources, conversion and
project helpers, device SDK archives, documentation and third-party notices.

Font conversion uses target-native Node 24.11.1. Image/project conversion uses
an isolated, relocatable CPython 3.11 runtime, with Pillow, PyPNG, LZ4 and resvg
wheels selected for the target architecture. No Python or Node installation
on the user's Mac is required. Host Node is used only while assembling the
JavaScript font helper; macOS executables are never run by the Linux builder.
Python helpers disable bytecode writes so running converters cannot add
unsigned cache files to the signed application bundle.

The build strips Python development headers and static/object files from its
runtime copy, and thins universal helper binaries to the selected architecture.
Packaging resolves each non-system dylib import, copies its dependency closure
into the bundle, rewrites imports relative to their loader and removes build
machine rpaths. Escaping symlinks, ELF/PE executables, missing dependencies,
wrong architectures and deployment targets newer than 13.5 fail packaging.

## Signing and verification

The packager creates **ad-hoc signatures** using the downloaded `rcodesign`
tool. It does not use a Developer ID, request an account or notarize the app.
Existing Node JIT entitlements, hardened-runtime flags and runtime version are preserved when
load-command changes require resigning.

Linux-side verification checks every CodeDirectory code-page hash, embedded
special-slot hashes, the signed `Info.plist` and `CodeResources`, resource
hashes/symlink targets and nested-code CDHashes. Archive readers independently
compare every ZIP/TGZ entry's content, permissions and symlink target with the
verified app. Tampered native code, entitlements, resources or archives fail
the regression tests. The `rcodesign verify` command in version 0.29.0 has a
documented ad-hoc CMS verification limitation, so it is not used as evidence
of successful verification.

These checks do not execute the app or establish Gatekeeper acceptance.
On a Mac, download the matching `arm64` archive for Apple Silicon or `x86_64`
archive for Intel. Open the ZIP in Finder, then drag `LVGL UI Creator.app` into
the Applications folder. Alternatively, extract and copy it from Terminal
(replace the example archive name with the downloaded version):

```sh
ditto -x -k LVGL-UI-Creator-0.9.2-macOS-arm64.zip .
ditto "LVGL UI Creator.app" "/Applications/LVGL UI Creator.app"
```

Check Apple's verifier before launching and testing the editor and converters:

```sh
codesign --verify --deep --strict --verbose=2 "/Applications/LVGL UI Creator.app"
open "/Applications/LVGL UI Creator.app"
```

An ad-hoc app downloaded from the internet can still require the user's
normal macOS approval for an unnotarized application. Distribution with
Developer ID signing/notarization is a separate release operation.

## Packaging again and tests

After a successful build, package without rebuilding all application targets:

```sh
python3 Packaging/PackageMacOS.py --base /var/tmp/lvgl-ui-macos \
  --build-directory /var/tmp/lvgl-ui-macos/build-x86_64 --architecture x86_64
```

The configured `UICreatorMacOSPackages` CMake target also creates/audits packages:

```sh
/var/tmp/lvgl-ui-macos/tools/cmake-4.5.1-linux-x86_64/bin/cmake \
  --build /var/tmp/lvgl-ui-macos/build-arm64 --target UICreatorMacOSPackages
```

Run the packaging regression tests on Linux or Windows:

```sh
python3 -m unittest discover -s Tests/UnitTests/Packaging -p 'test_macos_*.py' -v
python3 -m unittest discover -s Tests/Integration/Packaging -p test_macos_cross.py -v
```

The CMake cross build sets `BUILD_TESTING=OFF`; Linux cannot execute macOS
CTest binaries. Native/editor tests continue to run on their host platforms.
