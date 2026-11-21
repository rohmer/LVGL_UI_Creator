# Firmware project creator

Open Export > Create firmware project... to create a board project around the current design. The bundled catalog includes 67 official templates from https://github.com/lvgl/lvgl_project_creator (manifest_all_v1.1.0.json), plus the built-in Raspberry Pi Linux build. The upstream application is not open source; this implementation consumes its public manifest format independently.

Select **Chip / board vendor > Raspberry Pi**, then **Project template > Raspberry Pi - Linux** for a native Linux CMake project. It is available when preparing projects on Windows, Linux or macOS and uses bundled LVGL 9.6.0. Creation works offline and includes the current screen, the portable display library, SDL3 sources and only the LVGL widgets required by that screen. Advanced Keyboard, its selected prediction trie, barcodes, QR codes, themes and event actions use the shared runtime.

Every output format includes a runnable `ui/project.pb` and editable `ui/project.json`. Choosing C code also saves the generated sources. Transfer the generated folder to Raspberry Pi OS 64-bit or another Linux machine with CMake 3.24+, Python 3, a C/C++17 compiler and Ninja, then run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/platform/Runtime/linux/lvgl-pb-viewer ui/project.pb
```

An installed SDL3 development package is used when available; otherwise the build uses the bundled SDL3 sources and the graphics/input development libraries on the target machine. The generated integration guide also describes the callback display backend and ARM64 cross-compilation. No specific Raspberry Pi panel or touch driver is configured. Regenerate the project after changing the screen so its minimized source profile matches the new widgets and assets. The built-in Linux entry remains available after catalog refreshes and when loading older cached catalogs.

Search and filter templates, inspect hardware and documentation links, load previews, select a branch, and configure manifest settings. Projects support JSON, generated C or Protobuf design output. Select the template LVGL version or override it; LVGL 9.6 is bundled for offline replacement. Other versions download on demand. Package-manager templates may require an explicit LVGL source folder for replacement.

The packaged worker downloads pinned GitHub archives and submodules without requiring Git or Python on the user's machine. Cached archives can be reused offline. Refresh updates the catalog. Creation refuses a nonempty destination, stages files before publishing, and records source revisions and settings in uicreator-project.json. On Windows, final directory publication allows six rename attempts with up to 1.55 seconds of total backoff for access/sharing/lock errors; it never restarts generation or overwrites a destination that appears meanwhile. Other failures are reported immediately. Open the result in its folder or VS Code after creation.

CMake packages the Python worker, catalog and LVGL archive as editor dependencies. Tests cover all bundled manifests, settings, safe extraction, offline submodules, all three design formats, native Protobuf loading and compilation/execution of a generated C project. Individual board builds still require their vendor SDKs and hardware.
