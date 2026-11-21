# Windows builds and installer

New native builds default to x64. Select `-DUICREATOR_WINDOWS_ARCH=x86` for a
32-bit application; `x32` is accepted as an alias. Ninja must run in the matching
Visual Studio developer environment. A mismatched compiler is rejected rather
than producing a mislabeled package. Use separate build folders for each target.

From PowerShell, build and stage both application architectures:

```powershell
./Packaging/BuildWindows.ps1 -Architecture both
```

The script finds Visual Studio, selects the proper native compiler for each
build, builds the editor, compiler, code generator and Windows display SDK,
installs the complete application, verifies the PE machine of all native tools
and DLLs, and writes SHA256 architecture manifests. Existing compatible build
folders can be reused with `-X86BuildDirectory` and `-BuildRoot`.

Create separate x64 and x86 installers with CMake's CPack NSIS generator:

```powershell
./Packaging/BuildInstaller.ps1 -Architecture both -TestInstaller -SmokeApplications
```

NSIS must be installed or selected with `-NsisExecutable`. The backend verifies
each complete payload manifest and native architecture before packaging.
`-Architecture both` (the default) writes two independent setup executables;
`-Architecture x64` or `x86` builds just that installer and needs only its stage.
Each package contains its matching editor, tools, runtime DLLs and device SDKs.
There is no architecture selection page. The x64 installer requires 64-bit
Windows; the x86 installer also runs on 32-bit Windows. Installation is per user,
requires no elevation and does not modify PATH. Separate architecture folders,
Start menu entries and uninstall registrations allow the packages to coexist.
Updating an architecture removes its previous exact package files and preserves
user-created projects.

The supported Windows baseline is Windows 10 or newer. The x86 editor, viewer,
compiler, code generator, display SDK and bundled conversion helpers are all
32-bit. x86 builds automatically prepare checksum-verified Node 22.23.5 win-x86
and CPython 3.14.10 x86, with pinned native image/conversion modules. Python and
Node need not be installed on the user's computer. Their build paths, versions,
source fingerprints and download hashes are recorded in
`build/windows-helper-tools/x86/cmake-helpers.json`. x64 retains its separate
native helper toolchain. Build preparation still runs on the 64-bit development
host described in [BUILD.md](../BUILD.md).

Staging starts with a clean architecture output directory. Before writing a
manifest or invoking CPack, the scripts recursively validate every native PE
file, including Python `.pyd`, Node `.node`, CRT DLLs, extensionless PE images
and native entries inside SDK ZIPs. A foreign machine type, wrong PE32/PE32+
format, malformed native file or stale helper runtime stops packaging.

The default output folder is `build/windows-installer/packages`, containing the
`LVGL-UI-Creator-<version>-windows-x64-setup.exe` and
`LVGL-UI-Creator-<version>-windows-x86-setup.exe`, each with a `.sha256` sidecar
and `.manifest.json`. `installer.manifest.json` catalogs the generated packages
and their verified payload manifest hashes. Payload input can be
changed with `-StageRoot`; use `-BuildDirectory` and `-OutputDirectory` for
isolated release jobs. The root CMake target `UICreatorWindowsInstaller` packages
already staged payloads; `UICreatorWindowsBoth` builds the native payloads.

Silent installation accepts `/S`; the selected setup filename determines its
architecture. NSIS requires `/D=<directory>` to be the final argument. Uninstall
uses `Uninstall.exe /S`.

`TestInstaller.ps1 -Architecture x64` or `x86` runs the corresponding actual
installer, verifies every installed file hash, and tests uninstall and
preservation of an unrelated user file. The internal `/TESTMODE=1` flag suppresses
registration and shortcuts, and persists this isolation setting for uninstall.
Smoke-test directories must be new directories inside the repository. The test
checks both registry views and existing Start menu shortcuts remain unchanged.
Use `BuildInstaller.ps1 -TestInstaller -SmokeApplications` for real release
payloads. In each installed architecture it starts the editor with
`--smoke-test`, generates C and Protobuf with the installed command-line tools,
and renders four viewer frames to a checked 320x240 BMP. It also generates a trie
and checks native suggestions/autocorrect and pointer width, converts a font
with a Unicode input path, converts an SVG to an LZ4-compressed LVGL image, and
creates a complete offline Linux device project with the installed helpers.
The editor, viewer, setup and uninstaller must contain all nine canonical icon
sizes with matching frame hashes. Native PE validation is repeated on the
installed files. Tests execute the genuine x86 processes through WoW64 on the
64-bit Windows build host; a separate 32-bit Windows installation is not part
of this test environment.

Each child uses SDL's dummy driver and exits before uninstall. Main tool tests
have a 30-second deadline; font/image conversions have 60 seconds and project
creation has 120 seconds. Logs and
`application-smoke.json` remain under the isolated test directory. This option
is off for the tiny synthetic installer fixtures.

The older combined wizard remains available explicitly with
`BuildInstaller.ps1 -Architecture combined`; its optional compatibility tests
also check architecture switching and invalid selections. Separate installers
are the release deliverables.

The executable is not code-signed by this build. Signing for distribution can
be added by the release owner after CPack packaging; regenerate the checksum
and manifest after signing.

Use a portable CMake 4.5.1 installation without changing the machine's PATH:

```powershell
$cmakeTool = (Resolve-Path './build/cmake-tools/cmake-4.5.1-windows-x86_64/bin/cmake.exe').Path
$nsisTool = (Resolve-Path './build/nsis-tools/nsis-3.12/makensis.exe').Path
./Packaging/BuildWindows.ps1 -Architecture both -CMakeExecutable $cmakeTool
./Packaging/BuildInstaller.ps1 -CMakeExecutable $cmakeTool -NsisExecutable $nsisTool -TestInstaller
```

Download the official Windows x64 ZIP and verify its SHA256 against Kitware's
published checksum file before extracting it. Both scripts accept
`-CMakeExecutable` as an explicit executable path or application command name.
They resolve an absolute path, print the chosen version, require CMake 3.24 or
newer, and verify that sibling `ctest.exe` and `cpack.exe` have the same version.
Omitting the parameter uses the current process's `cmake.exe` command.
The root CMake packaging targets pass their own `${CMAKE_COMMAND}` through to
these scripts, so a portable configure keeps the same toolset for nested builds
and packaging. Architecture and installer manifests record the selected version
and all three tool paths; these additional fields preserve manifest version 1.

`-NsisExecutable` selects an installed or portable NSIS compiler, validates its
version (3.03 or newer, as required by CPack), and forwards the absolute path
through the `CPACK_NSIS_EXECUTABLE` cache setting. Keep the full portable NSIS
distribution, including its Include, Contrib, Plugins and Stubs directories.
Verify the ZIP against the SHA256 on the official SourceForge file download
page before extracting it. The installer manifest records the explicit NSIS
path and version. With no override, CPack uses its normal NSIS discovery.
For the root `UICreatorWindowsInstaller` target, configure
`-DUICREATOR_NSIS_EXECUTABLE:FILEPATH=<absolute-path-to-makensis.exe>`.

The CMake 4.5.1 source audit keeps the existing 3.24 minimum and policy level.
That minimum accepts newer CMake releases; changing it to 4.5 is unnecessary.
The existing 3.5 compatibility floor accommodates bundled dependencies with
older minimum declarations. Repository-owned build files and generated-project
templates do not use the removed Visual Studio 2015 generator or legacy
`install_files`, `subdirs`, `write_file`, `FindPythonInterp`, or
`FindPythonLibs` interfaces. They use `FindPython3`'s host interpreter component.
All substitution names used by the custom combined NSIS template remain present
in CMake 4.5.1's default NSIS template. Actual configure/build/CTest/CPack runs
remain the release check; a source audit alone does not verify a tool upgrade.

References: [CMake 4.5 release notes](https://cmake.org/cmake/help/v4.5/release/4.5.html),
[minimum and policy version behavior](https://cmake.org/cmake/help/v4.5/command/cmake_minimum_required.html),
[policy compatibility floor](https://cmake.org/cmake/help/v4.5/variable/CMAKE_POLICY_VERSION_MINIMUM.html),
and [official downloads](https://cmake.org/download/).

Verify the tool resolver independently with the actual selected installation:

```powershell
powershell.exe -NoProfile -File Tests/Integration/Packaging/CMakeTools.ps1 -CMakeExecutable $cmakeTool -ExpectedVersion 4.5.1 -NsisExecutable $nsisTool
powershell.exe -NoProfile -File Tests/Integration/Packaging/PeArchitecture.ps1
```

Icon checks use the host Python interpreter; pass `-PythonExecutable` to
`BuildInstaller.ps1` or `TestInstaller.ps1` to select it explicitly. Application
conversion tests always execute the installed private helpers.
