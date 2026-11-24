# Linux and Raspberry Pi Protobuf display SDK

Full builds also accept LVGL UI Creator XML through the existing memory/file
and viewer APIs. `LVSERIAL_USE_XML` defaults to `ON`; set it to `OFF` to omit
the codec. A minimal profile enables it with `LVGL_PB_ENABLE_XML=ON` while keeping
the same selected widgets. XML resources are owned after loading, including
when using the borrowed-input API. This is not the LVGL Pro XML format.

This C ABI loads LVGL UI Creator `.pb` projects on Linux x86-64 and ARM64.
The SDL3 shared library and viewer use the same `lvgl_pb_window_*` API as the
Windows SDK. A separate board callback API renders without SDL and accepts
pointer/touch coordinates from application drivers. LVGL 9.6, the runtime,
serializers, fonts, QR/barcodes and the Advanced Keyboard prediction engine
are included in the source toolkit. An editor installation is not required.

Build on Raspberry Pi OS 64-bit or another Linux distribution with a C/C++17
compiler and CMake 3.24+. Install SDL3 development files for desktop output,
or let CMake build the pinned bundled SDL3. The bundled source build uses the
graphics/input development libraries installed on the machine; SDL's desktop
and direct-display drivers depend on that platform configuration.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software ctest --test-dir build --output-on-failure
build/Runtime/linux/lvgl-pb-viewer examples/project.pb --watch
cmake --install build --prefix "$PWD/install"
```

From the editor repository, use `cmake -S Runtime/linux -B build/linux`.
The full shared library accepts the complete widget set. Generate a smaller
SDK for one or several projects to compile only their safe LVGL dependency
closure:

```sh
python3 Runtime/linux/make_sdk.py project.pb --output generated/linux-project
cmake -S generated/linux-project -B generated/linux-project/build -G Ninja
cmake --build generated/linux-project/build --parallel
```

Each generated SDK is self-contained, with embedded project data, pinned
LVGL and SDL3 sources, an explicit `embedded/features.json` source/feature
report and a SHA-256 manifest. The builder reuses the ESP32 project's feature
planner, including image/font/code-widget dependencies and borrowed binary
trie/index assets. Regenerate after changing projects. `--system-sdl` omits
SDL source and requires an installed SDL3 package. `--color-depth 16` selects
RGB565; the default is 32-bit output. Unused widgets, drivers, fonts and
prediction models are excluded by the profile. Common LVGL core, software
drawing, the inherited default theme and a fallback font remain.

For a board display, configure `-DLVGL_PB_WITH_SDL=OFF`, include
`lvgl_pb_board.h`, and supply its flush callback. The callback receives a
borrowed packed pixel rectangle and must call `lvgl_pb_board_flush_ready`
when transport completes. Call `lvgl_pb_board_poll(board, elapsed_ms)` on the
owner thread and feed touch/mouse state with `lvgl_pb_board_pointer`. The
application supplies framebuffer, SPI, DRM or other hardware transport.
Complete pending pixel transfers before destroying the board.
The callback byte count includes row alignment; divide it by rectangle height
to obtain the source stride when copying into a hardware framebuffer.
This SDK does not configure a physical panel or touch controller.

Use either the SDL window backend or the board backend as the LVGL owner in
a process. Keep every SDK call on the creating thread. Normal memory loading
owns retained project resources. The board's explicit borrowed-assets loader
can retain immutable flash/memory-mapped trie/index spans until destruction;
the input mapping must remain alive. Predictor inference uses a per-keyboard
workspace and shared immutable trie bytes. External text fields keep their
own content and configuration when focus switches the Advanced Keyboard.

An example GNU/Linux ARM64 cross-toolchain is supplied:

```sh
cmake -S . -B build-arm64 -G Ninja -DLVGL_PB_WITH_SDL=OFF \
  -DCMAKE_TOOLCHAIN_FILE=Runtime/linux/cmake/aarch64-linux-gnu.cmake
cmake --build build-arm64 --parallel
```

This targets the cross-compiler's glibc/sysroot, which must match the target
Pi OS ABI. Supply a matching Pi sysroot/toolchain for older distributions.
An ARM64 ELF cross-build verifies compilation and linking; it does not
verify a physical Raspberry Pi display, touch device or scanner.

Package the complete offline source toolkit with:

```sh
python3 Runtime/linux/package_toolkit.py --output build/LVGL-Protobuf-Linux-Toolkit \
  --example-project project.pb
```

After a Linux build, package its installed C ABI, examples, matching SDL3
runtime and ELF architecture/symbol-version manifest with:

```sh
python3 Runtime/linux/package_binary.py --build-dir build/linux \
  --output build/LVGL-Protobuf-Linux-x86_64.tar.gz
```

Profile binary packages retain their exact feature report and project inputs.
Use matching architecture and runtime libraries, or rebuild the source SDK on
the target distribution. The binary packager runs on Linux.

The 0.9.2 release SDKs use GCC 11 and an Ubuntu 22.04 target sysroot. The full
x86-64 SDK includes SDL3 and the callback backend; its measured minimum symbol
versions are GLIBC 2.34, GLIBCXX 3.4.29 and CXXABI 1.3.9. The ARM64 keyboard
profile SDK uses the callback backend without SDL3; its measured minimums are
GLIBC 2.32, GLIBCXX 3.4.29 and CXXABI 1.3.9.
Every binary package prints its complete measured requirements in README and
`sdk.json`; a different compiler, backend or selected feature set may change
them. Compare those requirements with the libraries installed on the target,
or rebuild the source SDK using its toolchain. These ABI checks do not replace
testing the application's display and input drivers on the target hardware.
