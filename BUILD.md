# Building LVGL UI Creator

Run commands from the repository root unless stated otherwise. These instructions
cover the desktop editor, compiler, C and MicroPython exporters, Protobuf viewer, device SDK inputs,
and the four separate release installers:

| Target | Build argument | Installer |
| --- | --- | --- |
| Windows 64-bit | `x64` | `LVGL-UI-Creator-0.9.1-windows-x64-setup.exe` |
| Windows 32-bit | `x86` (`x32` is a build alias) | `LVGL-UI-Creator-0.9.1-windows-x86-setup.exe` |
| Linux x86_64 | `x86_64` | `LVGL-UI-Creator-0.9.1-Linux-x86_64.deb` |
| Linux ARM64 / Raspberry Pi | `aarch64` | `LVGL-UI-Creator-0.9.1-Linux-arm64.deb` |

The Linux release also produces a `.tar.gz` for each architecture. ARM64 means
64-bit Raspberry Pi OS/Linux, not ARMv7 or a 32-bit Raspberry Pi installation.

Figma importing needs no additional compiler or development package. Online import
uses Windows WinHTTP or the system `libcurl.so.4` library on Linux, with the system
CA certificates. The desktop package also includes the local Figma export plugin
in `integrations/figma`; its JavaScript and HTML run directly in Figma. See the
[Figma import guide](Integrations/Figma/README.md) for **File → Import Figma design…**
usage. Online import requires a Figma personal access token with `file_content:read`;
local plugin imports require no token.

## Source checkout and tools

Use a complete checkout containing the supplied `3rdParty` dependencies. If the
checkout uses Git submodules, initialize them with
`git submodule update --init --recursive`. LVGL, SDL3, FreeType, nanopb, JSON and
the editor's other native dependencies are supplied in the repository; do not
replace them with arbitrary system LVGL/SDL releases.

The root CMake minimum is 3.24; the release build and CPack workflows are verified
with **CMake 4.5.1**. Keep CMake, CTest and CPack from the same installation.
Older vendored projects use the existing `CMAKE_POLICY_VERSION_MINIMUM=3.5`
compatibility floor; this does not lower the editor's CMake requirement.

Initial preparation needs internet access for pinned npm, Python, Cargo and
Linux tool/sysroot downloads. Keep separate build trees for each architecture
and compiler. Do not move a configured CMake build tree to a different machine.

### Windows build host

Use a 64-bit Windows 10/11 development machine with:

- Visual Studio 2022/2026 or Build Tools, including **Desktop development with
  C++**, MSVC x86/x64 compilers and a Windows SDK.
- CMake 4.5.1 and Ninja on PATH, or an explicit CMake executable path.
- Python 3.10+ with pip and venv; Python 3.12+ is a convenient host choice.
- For x64 builds, Node.js **24.11.1** and npm **11.6.2**, matching the font
  converter's pins. x86 automatically prepares its private Node **22.23.5**.
- Rust/Cargo through rustup, including both Windows target standard libraries.
- NSIS **3.12** for the tested CPack setup builds.

```powershell
rustup target add x86_64-pc-windows-msvc i686-pc-windows-msvc
cmake --version
ctest --version
cpack --version
ninja --version
node --version
npm --version
python --version
cargo --version
```

CMake prepares a private Python environment for image/project helper packaging
and installs the pinned requirements. Font converter npm dependencies are
installed when missing. NSIS is a CPack backend; Inno Setup is not used.

Windows application packages support Windows 10 or newer. The x86 build includes
genuine 32-bit editor/viewer/compiler/display binaries and conversion helpers.
CMake automatically prepares pinned Node 22.23.5 win-x86 and CPython 3.14.10 x86
with the required native image modules under `build/windows-helper-tools/x86`.
Download hashes and exact helper paths are recorded in `cmake-helpers.json`.
Build-host Python/Node remain separate; users need no system interpreter to run
the packaged converters. x64 and Linux helper preparation is unchanged.

### Linux build host

The release frontend supports **Ubuntu x86_64**, either a Linux machine or WSL2
on Windows. The verified host is Ubuntu 26.04 x86_64 under WSL; the Ubuntu 22.04
sysroots describe the target baseline, not a claim of testing every host distro.
ARM64 applications are cross-compiled on that host; the bootstrap is
not a native ARM-host build frontend. The host needs Python **3.10.12+**.

Install the host prerequisites once:

```sh
sudo apt update
sudo apt install build-essential ninja-build pkg-config python3 ca-certificates \
  ubuntu-keyring binutils binutils-aarch64-linux-gnu qemu-user qemu-user-binfmt \
  dpkg-dev fakeroot zenity libwayland-bin
```

The prepare step downloads checksum-verified CMake 4.5.1, Node 24.11.1,
standalone CPython 3.11.18, Rust 1.85.1, and authenticated Ubuntu 22.04 target
sysroot/compiler packages, including GCC 11.4. It keeps these in its private
cache instead of replacing system tools. Download-only APT preparation works as
a normal user with a writable cache; only installing the host prerequisites
above needs administrator privileges.

QEMU/binfmt support is needed for ARM tests and their ARM child executables.
The prepared targets include X11 and Wayland desktop support. A visible desktop
or WSLg is needed for interactive GUI use; automated smoke checks use SDL's
dummy video driver.

## Build Windows x64 and x86

For Visual Studio's CMake support, open the repository folder containing the root
`CMakeLists.txt`. The checked-in `UICreator.sln` also provides Debug and Release
configurations for x86 and x64; its x86 solution configuration maps to the native
projects' `Win32` platform. Reload the solution after project-file updates so
Visual Studio refreshes project references and newly registered source files.
Use the CMake packaging workflow below to create release payloads and installers.

From ordinary PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Packaging/BuildWindows.ps1 -Architecture both
```

The script finds Visual Studio, loads the matching compiler environment, builds
the editor/tools/Windows display SDK, stages the application and checks native
PE architectures recursively, including native Python/Node modules, CRTs and
SDK ZIP entries. Each architecture stage is recreated to remove stale helper
files. It writes `architecture-manifest.json` with file hashes, a native PE inventory and
the selected CMake toolset. x64 is the default for a single-architecture build.

```powershell
./Packaging/BuildWindows.ps1 -Architecture x64
./Packaging/BuildWindows.ps1 -Architecture x86
```

Defaults:

- Native build trees: `build/windows-architectures/x64` and `x86`.
- Verified application stages: `build/windows-installer/payload/x64` and `x86`.
- Shared device SDK input directory: `build/professional/packages`.

The CMake entry points build both Windows architectures, then create their two
separate CPack installers. Run these commands sequentially from the repository
root, using an already configured Windows build tree:

```powershell
cmake --build build/windows-architectures/x64 --config Release --target UICreatorWindowsBoth
cmake --build build/windows-architectures/x64 --config Release --target UICreatorWindowsInstaller
```

`UICreatorWindowsInstaller` consumes the verified payloads produced by the first
target. The normal `package` target creates a ZIP for its configured architecture.

For an isolated release, configure a separate controller tree from an x64 Visual
Studio developer shell. This keeps its orchestration separate from the native
build trees and preserves previous installer files:

```powershell
cmake -S . -B build/release-controller -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUICREATOR_WINDOWS_BUILD_ROOT=build/windows-architectures `
  -DUICREATOR_INSTALLER_STAGE_ROOT=build/release-output/windows/payload `
  -DUICREATOR_RELEASE_SDK_DIRECTORY=build/release-sdk-inputs `
  -DUICREATOR_WINDOWS_PACKAGE_BUILD_ROOT=build/release-output/windows/cpack `
  -DUICREATOR_WINDOWS_PACKAGE_DIRECTORY=build/release-output/windows/packages `
  -DUICREATOR_RELEASE_JOBS=6
cmake --build build/release-controller --config Release --target UICreatorWindowsBoth
cmake --build build/release-controller --config Release --target UICreatorWindowsInstaller
```

Populate the SDK input directory with the current archives listed below before
running these targets. `UICREATOR_RELEASE_CONFIGURATION` defaults to `Release`.
Both targets share `UICREATOR_INSTALLER_STAGE_ROOT` so custom staging paths remain
consistent. The package output and CPack build directories are independently
configurable. The PowerShell frontends below accept equivalent path overrides.

Optional parameters include `-BuildRoot`, `-StageRoot`, `-SdkDirectory`,
`-X86BuildDirectory`, `-Configuration Release|RelWithDebInfo|Debug`, and `-Jobs`.
Use `-CMakeExecutable` to select a portable installation explicitly:

```powershell
$cmakeTool = 'C:\Program Files\CMake\bin\cmake.exe'
./Packaging/BuildWindows.ps1 -Architecture both -CMakeExecutable $cmakeTool -Jobs 6
```

For direct CMake use, open the matching Visual Studio Native Tools command
prompt first. Ninja's compiler architecture must match the option:

```sh
cmake -S . -B build/manual-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DUICREATOR_WINDOWS_ARCH=x64 -DBUILD_TESTING=ON
cmake --build build/manual-x64 --parallel 6
```

For a 32-bit compiler environment use a separate directory and
`-DUICREATOR_WINDOWS_ARCH=x86`; `x32` is accepted as an alias. Existing caches can
use `auto`, but new builds default to x64. A mismatch is rejected.

### Build the checked-in Visual Studio solution

The native projects in `UICreator.sln` use the **v145** toolset. Install Visual
Studio 2026 or Build Tools with that toolset, the x86/x64 C++ tools, and a Windows
10/11 SDK. SDK version `10.0` selects an installed compatible SDK. The Visual
Studio 2022 option above applies to the CMake workflow; it does not supply v145
for the checked-in solution.

From a Visual Studio 2026 Developer PowerShell or Developer Command Prompt, run
each full-solution build in sequence:

```powershell
msbuild UICreator.sln /t:Build /m:4 /p:Configuration=Debug /p:Platform=x86
msbuild UICreator.sln /t:Build /m:4 /p:Configuration=Release /p:Platform=x86
msbuild UICreator.sln /t:Build /m:4 /p:Configuration=Debug /p:Platform=x64
msbuild UICreator.sln /t:Build /m:4 /p:Configuration=Release /p:Platform=x64
```

Use `x86` at the solution level; it maps to the projects' `Win32` platform.
Editor executables are written to `Debug/UICreator.exe`,
`Release/UICreator.exe`, `x64/Debug/UICreator.exe`, and
`x64/Release/UICreator.exe`. Keep the projects' normal intermediate and output
directories; dependencies and post-build steps use these paths.

These builds include the solution's native libraries and test executables.
The editor's post-build steps also bundle the font, image, and project conversion
helpers. They detect the built editor's architecture and prepare private 32-bit
helpers for x86. Python, Node/npm, Rust/Cargo, and initial download access from
the prerequisites above are still required. Put `cmake` and `python` on PATH
for the native generation hooks. Direct MSBuild builds do not need Ninja or
NSIS. Use the CMake/CPack workflow for the complete SDK payloads and installers.

## Create the two Windows installers

After staging the payloads:

```powershell
./Packaging/BuildInstaller.ps1 -Architecture both -TestInstaller -SmokeApplications
```

This invokes actual CPack/NSIS and produces two independent installers. Use
`-Architecture x64` or `x86` for only one; that needs only its matching stage.
There is no architecture-selection page in these separate packages. The older
combined backend remains available through `-Architecture combined`.

Portable tool selection:

```powershell
./Packaging/BuildInstaller.ps1 -Architecture both -CMakeExecutable $cmakeTool -NsisExecutable 'C:\Program Files (x86)\NSIS\makensis.exe' -TestInstaller -SmokeApplications
```

Outputs are in `build/windows-installer/packages`: setup executables, SHA256
sidecars, per-package manifests and `installer.manifest.json`. Test mode installs
into isolated workspace directories, verifies all payload hashes, runs the
installed editor/compiler/codegen/viewer, and verifies uninstall and user-file
preservation without changing normal application registration or shortcuts.

## SDK inputs for complete release packages

Complete installers include these published SDK archives and their `.sha256`
sidecars in a flat input directory:

```text
LVGLProtobuf-ESP32-Toolkit.zip
LVGLProtobuf-Arduino-Toolkit.zip
LVGLProtobuf-Linux-Toolkit.zip
LVGLProtobuf-Linux-Keyboard-Profile.zip
LVGLProtobuf-Linux-aarch64-keyboard.tar.gz
LVGLProtobuf-Linux-x86_64.tar.gz
LVGLProtobufDisplay-windows.zip
```

Use SDK inputs from the matching release/CI artifacts. An existing Linux release
contains them and can supply a Linux-only build machine without a Windows build:

```sh
mkdir -p build/sdk-inputs build/sdk-extracted
dpkg-deb -x /path/to/LVGL-UI-Creator-0.9.1-Linux-x86_64.deb build/sdk-extracted
find build/sdk-extracted/usr/lib/lvgl-ui-creator/sdk -type f \
  \( -name '*.zip' -o -name '*.tar.gz' -o -name '*.sha256' \) \
  -exec cp -t build/sdk-inputs -- {} +
SDK_DIR=$(realpath build/sdk-inputs)
```

Building the Linux application itself does not require rebuilding a Windows
SDK. These archives are release-package inputs. For a local development build
and package that intentionally omits them, pass `--without-sdks` to
`BuildLinux.sh`; it sets `UICREATOR_LINUX_REQUIRE_SDKS=OFF`. Omit the option for
release builds to restore the complete SDK requirement.
Do not substitute stale ESP32/Arduino keyboard-profile ZIPs for current toolkits.

## Build on Linux, including ARM64 cross-compilation

In an Ubuntu terminal, change to the repository. On Windows use the same commands
inside WSL2, for example after `cd /mnt/e/source/LVGL_UI_Creator`.

```sh
sh Packaging/BuildLinux.sh --prepare --architecture x86_64 --jobs 6 --sdk-directory "$SDK_DIR"
sh Packaging/BuildLinux.sh --prepare --architecture aarch64 --jobs 6 --skip-sync --sdk-directory "$SDK_DIR"
```

Run the commands sequentially when sharing the default source mirror. `--prepare`
prepares pinned tools and the selected sysroot; omit it for incremental builds
after preparation. A normal build synchronizes source into a Linux filesystem
mirror. `--skip-sync` reuses that mirror, so omit it after changing source files.
This avoids compiling directly on a slow Windows-mounted filesystem.

For example, a fresh Linux-only checkout can build without release SDK inputs:

```sh
sh Packaging/BuildLinux.sh --prepare --architecture x86_64 --without-sdks --jobs 6
```

The default cache is `/var/tmp/lvgl-ui-desktop`. Use `--base` consistently on all
build/check commands to select another writable Linux directory. The default
builds are `build-x86_64-gcc11` and `build-aarch64-gcc11` below that base; executables
are in each `bin` directory. Generated toolchain files, logs, tool checksums and
`build-metadata.json` record the actual compiler/sysroot/options.

The frontend intentionally uses the matching GCC 11 compiler, headers and target
libraries. Mixing host GCC 15 headers or libraries into these builds can raise
the runtime ABI requirement or fail compilation. ARM uses generic ARMv8 code.

## Verify and package Linux

Run the prepared architecture checks:

```sh
python3 Packaging/BuildLinuxCheck.py --architecture x86_64
python3 Packaging/BuildLinuxCheck.py --architecture aarch64
```

The ARM checks use QEMU. They verify the actual target binaries and converters;
they do not replace testing on Raspberry Pi hardware.

Create DEB and portable archives using matching CMake/CPack:

```sh
BASE=/var/tmp/lvgl-ui-desktop
CMAKE_TOOL="$BASE/tools/cmake-4.5.1-linux-x86_64/bin/cmake"
python3 Packaging/PackageLinux.py --build-directory "$BASE/build-x86_64-gcc11" --architecture x86_64 --cmake "$CMAKE_TOOL" --output "$PWD/build/linux-desktop/packages"
python3 Packaging/PackageLinux.py --build-directory "$BASE/build-aarch64-gcc11" --architecture aarch64 --cmake "$CMAKE_TOOL" --emulator qemu-aarch64 --sysroot "$BASE/sysroots/arm64" --output "$PWD/build/linux-desktop/packages"
```

Within the prepared Linux build environment, each configured architecture also
provides the CMake target `UICreatorLinuxPackages`:

```sh
"$CMAKE_TOOL" --build "$BASE/build-x86_64-gcc11" --target UICreatorLinuxPackages --parallel 6
"$CMAKE_TOOL" --build "$BASE/build-aarch64-gcc11" --target UICreatorLinuxPackages --parallel 6
```

Refresh the source mirror with `BuildLinux.sh` after source changes. The
`PackageLinux.py` frontend restores the recorded toolchain environment and adds
payload audits and execution checks to CPack generation, making it the release
verification path.

Use `--output` for a shared artifact directory and `--jobs` to control build
parallelism. CPack stages on a Linux filesystem to preserve permissions; only
completed archives are copied to a Windows-mounted output directory.

Artifacts and verification reports are written to `--output`
(`build/linux-desktop/packages` above); without `--output`, they default to
`<build-directory>/packages`. These include DEB/TGZ files, SHA256 sidecars and a
`Linux-*.manifest.json`. Verification checks hashes, Unix permissions, ELF architecture, required helper resources,
desktop integration and runtime dependency metadata. It runs the packaged editor,
compiler, exporter, viewer and conversion helpers, natively or through QEMU.

The application ships its own SDL3 and helper runtimes. The operating system
supplies glibc, libstdc++, libgcc and its graphics-driver stack; the DEB declares
the measured requirements and desktop/Zenity dependencies. Check the final
package manifest for authoritative ABI requirements rather than assuming all
Linux distributions or 32-bit Raspberry Pi installations are compatible.

Install on the matching Linux architecture:

```sh
sudo apt install ./LVGL-UI-Creator-0.9.1-Linux-x86_64.deb
# On 64-bit Raspberry Pi OS, use the Linux-arm64.deb instead.
lvgl-ui-creator
```

Or extract the architecture's `.tar.gz` and run its `usr/bin/lvgl-ui-creator`
launcher. Keep the extracted `usr` tree together; the launcher locates private
libraries and resources relative to itself.

## Tests and launching Windows development builds

The Windows staging script builds release tools, not every test executable.
To run the complete native suite, use the matching developer environment:

```sh
cmake --build build/windows-architectures/x64 --parallel 6
ctest --test-dir build/windows-architectures/x64 --output-on-failure --parallel 4
```

The editor's tests cover multi-screen editing/undo, Save/Save As, keyboard input,
trie prediction, themes/components, responsive layouts, Problems and performance
replay. Optional older-LVGL matrices require their source checkouts; see
`Tests/README.md`. Visual baselines are updated explicitly, never automatically
because a baseline is missing; see `Tests/Visual/README.md`.

Launch a verified Windows stage in an isolated live copy:

```powershell
./Packaging/Launch.ps1 -ApplicationDirectory build/windows-installer/payload/x64 -Project path/to/project.json -KeepOpen
```

This keeps running app files separate from build outputs and uses an isolated
session directory. It does not close other copies or discard unsaved projects.

## Device libraries and firmware tools

ESP-IDF and Arduino CLI are not prerequisites for building the desktop editor.
Install and configure them when compiling/flashing those firmware targets.
The validated Arduino board is Nano ESP32 (`arduino:esp32:nano_nora`).

The Windows build script creates the matching Windows display SDK. Source
toolkits generate project-specific LVGL source lists from Protobuf inputs.
For complete generation/build commands and hardware settings, use:

- `Runtime/esp32/README.md` — ESP-IDF component and SPI example.
- `Runtime/arduino/README.md` — Arduino library generation and board setup.
- `Runtime/linux/README.md` — native Linux/Raspberry Pi runtime and board/display adapters.
- `Runtime/windows/README.md` — Windows display library and viewer.

## Collect the four releases

Once both Windows installers and both Linux package pairs pass verification:

```sh
python3 Packaging/collect_release.py --windows-packages build/windows-installer/packages --linux-packages build/linux-desktop/packages --output build/releases/0.9.1
```

In Windows PowerShell, use `python` instead of `python3` if that is the installed
host interpreter command.

The collector requires all four installers and two Linux archives, verifies the
published checksums, copies them and writes `release.json`. It refuses to replace
different existing release files; choose a new output directory for a changed
release.

## Troubleshooting

- **Architecture/compiler mismatch:** use a separate build directory and the
  matching developer environment; do not relabel an existing binary.
- **CMake/CPack selection:** pass `-CMakeExecutable` on Windows or `--cmake` on
  Linux. Keep sibling CTest/CPack at the same version.
- **NSIS not found:** install it or pass `-NsisExecutable` explicitly.
- **Missing converters:** check Node/npm, the private Python packaging log,
  prepared target helper paths and required Linux `python-runtime` assets.
- **ARM exec-format errors:** check QEMU/binfmt and the configured target
  sysroot. Do not copy x86 helper executables into an ARM package.
- **Missing/incompatible Linux libraries:** use the launchers and a distribution
  meeting the package's recorded ABI requirements; do not replace the system
  glibc with bundled libraries.
- **No Linux file chooser:** install Zenity. DEB dependencies install it; portable
  archives rely on the host's desktop runtime packages.
- **Changed toolchain/cache:** use a fresh build tree. The Linux frontend tracks
  its toolchain signature and refreshes compiler detection when it changes.

Additional details: `Packaging/WINDOWS.md`, `Packaging/LINUX.md`,
`Docs/Professional-workflows.md`, and `Tests/README.md`.

## Raspberry Pi Pico 1 and Pico 2 firmware

These are native Pico SDK targets, separate from the Linux ARM64 desktop app.
In the editor choose **Export > Create firmware project > Raspberry Pi**, then
**Raspberry Pi Pico 1** or **Raspberry Pi Pico 2**. C output links the exported
UI directly; Protobuf output includes the project-specific display library.

On Windows or Linux, install Pico SDK 2.3.1 or newer, Arm GNU bare-metal GCC,
Ninja, CMake 3.24+ and Python 3.10+. The SDK's `picotool` must be installed or
available to its normal extra-output build step. Put the compiler and Ninja on
PATH. The editor's **Build this project** handoff detects the board; set
**Pico SDK folder**, then Build. Tool version and SDK paths are saved with the
device target. UF2 flashing remains an explicit copy to the board's BOOTSEL drive.

The source generators work on both Windows and Linux:

```sh
python Runtime/pico/make_sdk.py project.pb --board pico --output build/pico-display --zip
python Runtime/pico/make_sdk.py project.pb --board pico2 --output build/pico2-display --zip
cmake -S build/pico-display -B build/pico-display/firmware -G Ninja -DPICO_SDK_PATH=/absolute/path/pico-sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build/pico-display/firmware --parallel
```

On Windows, use an SDK path such as `C:/pico/pico-sdk`. Build the `pico2-display`
folder separately for Pico 2. The outputs are `lvgl_pico_display.elf`, `.bin`
and `.uf2`. Pico 1 selects RP2040; Pico 2 selects RP2350 Arm Secure. The example
has display/touch callbacks for your own panel and a small partial draw buffer.
Configure the callbacks before expecting visible output on physical hardware.
See `Runtime/pico/README.md` for direct C input, source pruning, stack/heap
layout, memory budgets and integration into another firmware project.

## MicroPython source export

The editor and compiler include a MicroPython exporter. The normal Windows and
Linux builds above embed its runtime; no separate Python package is needed to
export a project. In the editor choose **Export > MicroPython project...**.
The command-line forms are:

```sh
UICreatorCodeGen --format micropython project.json generated_micropython
LVGLCompiler --format micropython --target 9.3 project.pb generated_micropython
```

The output requires LVGL-enabled MicroPython from the official
[lvgl/lv_binding_micropython](https://github.com/lvgl/lv_binding_micropython)
source. It targets binding revision
`032dda4b7e8419702a4b9c4c8748a4d85d51be61` and that revision's LVGL 9.3.0
submodule, independently of the editor's LVGL. Use the upstream
[lv_micropython](https://github.com/lvgl/lv_micropython) instructions for your
MicroPython port and board. Merge the generated `lv_conf.defaults.h` into the
binding's configuration, keeping its garbage collector and allocator settings,
then rebuild firmware to regenerate the enabled Python APIs.

Upload the generated scripts, `ui.json` and assets. Initialize your board's
display/input drivers and event loop, then call `ui.create()`. The exported
README includes setup and application variable-access examples. Preflight
reports custom native widgets or unsupported styles that require C/Protobuf
output. Barcode and QR content is exported as bitmap assets.
See [MicroPython export](CodeGeneration/MicroPython/README.md) for compatibility
and lifecycle details.

## UI Creator XML export and runtime loading

The standard build includes UI Creator XML, our typed XML representation of
the existing Protobuf project model. Select **Export > UI Creator XML (.xml)...**,
or run:

```sh
LVGLCompiler project.json --format xml --target 9.6 --output screen.xml
UICreatorViewer screen.xml --frames 20
ctest --test-dir build/release -L xml --output-on-failure
```

The existing runtime memory/file entry points detect either format. No extra
XML dependency, separate widget engine or Pro tool is needed. Full builds set
`LVSERIAL_USE_XML=ON`; disable it to remove the codec. Device profiles keep it
off by default and can opt in with their generator's `--enable-xml` option.
See [XML export and runtime integration](CodeGeneration/XML.md) for the schema,
platform settings, memory tradeoffs and conversion commands.
