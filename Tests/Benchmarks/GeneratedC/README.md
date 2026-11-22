# Generated C benchmark

This standalone host benchmark compares two exports of the same project against
the same LVGL 9.6 CLIB test archive. It measures requested LVGL heap bytes during
initialization, additional history storage after filling a bounded history, and
median object-lookup/history-update time over five samples. It checks that all
tracked LVGL allocations are released after teardown.

Generate one shared fixture and export it with the two compiler builds:

```sh
python Tests/Benchmarks/GeneratedC/CreateFixture.py --output build/c-perf/fixture
path/to/before/UICreatorCodeGen build/c-perf/fixture/project.json build/c-perf/before/generated
path/to/after/UICreatorCodeGen build/c-perf/fixture/project.json build/c-perf/after/generated
```

Build each export separately, changing `before` to `after` for the second run:

```sh
cmake -S Tests/Benchmarks/GeneratedC -B build/c-perf/before/benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGENERATED_DIR=/absolute/path/build/c-perf/before/generated \
  -DFIXTURE_DIR=/absolute/path/build/c-perf/fixture \
  -DLVGL_SOURCE_DIR=/absolute/path/3rdParty/LVGL/lvgl \
  -DLVGL_ARCHIVE=/absolute/path/to/lvgl.lib
cmake --build build/c-perf/before/benchmark
build/c-perf/before/benchmark/generated_c_performance
```

On Windows, use an MSVC developer shell matching the archive's architecture,
append `.exe` to the executable name, and use PowerShell backticks or a single
line instead of the shell continuations above. On Linux, supply `liblvgl.a`.
The cached `version-matrix/9.6` archive uses the configuration declared in this
benchmark's CMake file. An archive built with a different LVGL configuration is
not interchangeable.

The benchmark replaces LVGL's CLIB backend with a counting wrapper around the
same C allocator. Reported bytes exclude the tracking header, allocator overhead,
static data, existing display allocations, framebuffer memory and unrelated C/C++
allocations. `history_live_bytes` is additional storage after initialization;
older generated code can already allocate a small history buffer during init.

The fixture has two screens, 602 saved objects, 100 automatic button captions,
fixed text/options and one 1,024-entry `uint8_t` history. Both builds disable
interprocedural optimization. Lookup results feed a volatile sink. Run without
concurrent compilation and retain both raw JSON results; host timing and heap
figures do not predict a particular board's frame rate or total RAM use.
