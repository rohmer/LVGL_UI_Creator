# Automatic builds and tests

[Build and test](https://github.com/rohmer/LVGL_UI_Creator/actions/workflows/ci.yml)
runs after every push, for pull requests, and from **Actions → Build and test →
Run workflow**. New commits on the same branch cancel an older in-progress run.
The workflow uses GitHub-hosted runners and needs no repository secrets.

| Job | What runs |
| --- | --- |
| Windows x64 and x86, Debug and Release | Complete default CMake build and every registered CTest in each configuration |
| Linux x64 and ARM64 | Native builds and complete CTest suites on each architecture |
| macOS Intel and Apple Silicon | Native builds and complete CTest suites on real hosted Macs; deployment target 13.5 |
| Workflow and packaging checks | actionlint, Python helper compilation, packaging unit tests and the shared-keyboard source guard |
| Corpus Workbench and keyboard simulator | .NET build, native/managed trie parity, corpus workflow and keyboard language/RTL/Pinyin checks |
| ESP32, Arduino Nano ESP32, Pico 1 and Pico 2 | Generate a keyboard/trie Protobuf profile and compile/link actual firmware; Pico builds must produce UF2 files |

Each desktop job downloads all five older official LVGL source releases before
configuration. CI verifies that all six generated-C and runtime-backend tests
(9.6, 9.5, 9.3, 9.2, 9.1 and 8.3) are registered before running CTest, so a missing
download cannot silently reduce coverage. Default builds include the generated
C consumers, unit-test executables, editor, viewer and converters. Tests use
SDL's dummy/software drivers and do not need a graphical desktop.

Linux x64 also builds a real LVGL-enabled MicroPython interpreter from the
official pinned sources and enables `UICreatorMicroPythonNative` in its complete
CTest run. The normal test-double/runtime and export tests still run on all
desktop platforms. Native macOS jobs test their freshly built native outputs;
they do not claim to execute the separately produced OSXCross release archives.

Compiler/SDK dependencies are installed automatically. Desktop CI pins CMake
4.5.1, Node 24.11.1, npm 11.6.2 and Rust 1.85.1; x86 conversion helpers use the
repository's separate verified 32-bit toolchain. Firmware CI pins the official
ESP-IDF 6.0 container digest, Arduino CLI 1.3.1 with Nano ESP32 core
2.0.18-arduino.5, and Pico SDK 2.3.1. GitHub Actions are pinned to verified commit
IDs. Cargo/npm download caches reduce repeated setup; compiled outputs are
rebuilt from each checked-in source tree.

Linux dependencies use Ubuntu's signed official HTTPS repositories through a
temporary APT source file. This avoids a slow regional runner mirror without
replacing the machine's repository configuration; package installation is
noninteractive with bounded connection retries.

Open a workflow run to see each build/test step. Its artifacts retain CTest
JUnit reports, complete test inventories, nested build logs and firmware
hashes/binaries for 14 days, including on failure. A failed platform does not
cancel the other matrix entries. The final **CI result** check fails if any
required job fails, is cancelled or is skipped. That is the single check to
select if branch protection is enabled later; this setup does not change
existing branch-protection rules or publish releases automatically.

Hardware display/touch/PSRAM behavior, flashing, real network Figma credentials,
Developer ID signing/notarization and physical board tests require their own
environments. CI compiles firmware and runs hardware-free tests; it never
flashes devices. The paired ESPOS checkout is not present on hosted runners;
the manifest-owned Creator sources are checked against the committed sync
baseline, and sync unit tests run in CTest.

The separate **macOS release packages** workflow runs manually after publishing
the Mac archives. Supply the release tag and the full source commit recorded in
their manifests. Intel and Apple Silicon runners download the exact release
ZIPs, check their hashes and payloads, run Apple's strict signature verifier,
and exercise the bundled editor, viewer, exporters, model generator and native
conversion helpers with a system-only PATH. The final checks verify that those
operations did not change the signed app. This tests the published cross-built
packages rather than substituting a native rebuild; it does not establish
Gatekeeper acceptance or notarization.

For local reproduction use the commands in [BUILD.md](../BUILD.md), or run the
same CI desktop helper after installing host prerequisites:

```sh
python .github/scripts/desktop.py --architecture x86_64
```

On a Windows PowerShell terminal use
`.github/scripts/windows.ps1 -Architecture x86 -Configuration Release` to load
the matching Visual Studio environment automatically. The desktop helper uses
`build/ci-desktop`; use a separate checkout for each architecture/configuration.

Runner labels follow GitHub's [hosted runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
Workflow validation uses the official [actionlint release](https://github.com/rhysd/actionlint/releases/tag/v1.7.12).
