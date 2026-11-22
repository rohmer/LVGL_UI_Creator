# Generated C performance and memory

Exported projects keep their public `ui_*` APIs, saved object IDs, C/C++ variable
bindings, chronological history access and lifecycle behavior. Re-export a
project to use these changes; existing generated directories are not patched.

Fixed labels, checkbox captions and dropdown options use their immutable strings
directly. Authored DOTS labels and Arabic/Persian preprocessing retain copying
setters. Normal LVGL text setters and generated variable updates still copy
dynamic text. For direct insert/cut operations or a later change to DOTS mode,
first call `lv_label_set_text(label, lv_label_get_text(label))` to make a fixed
label mutable. Alternatively configure the generated CMake project with
`-DUI_USE_STATIC_TEXT=OFF`, or compile `ui.c` with `UI_USE_STATIC_TEXT=0`.

Identical read-only bytes and typed arrays share storage. Writable canvas buffers
remain independent. Static images omit animation tables and timers. Automatic
button captions need no persistent private handle or deletion callback; their
parent still owns them. Small projects use switches for object lookup; larger
ones binary-search immutable ID/slot arrays. Lookup adds no writable table or
heap allocation. Deleted objects still return null through their tracked slots.

Bounded histories use circular storage and the configured C element type.
Single-entry histories use inline storage without a history allocation. String
defaults remain literals; explicit updates allocate one string shared by the
current value and newest history entry. Numeric text is formatted once per
variable update across its text bindings. A history bound of zero continues to
mean unlimited storage; use a positive bound on constrained targets.

## Measured host comparison

The baseline is commit `551e1630`. Both versions use the same Windows x64 MSVC
Release build, LVGL 9.6 archive and fixture: two screens, 602 saved objects, 100
automatic button captions, fixed text/options and a 1,024-entry `uint8_t` history.
Timing values are medians from five paired process runs, each with five samples.

| Measurement | Before | After |
| --- | ---: | ---: |
| Requested LVGL heap after UI initialization/layout | 255,640 B | 222,328 B |
| Peak requested LVGL heap during initialization/layout | 265,214 B | 231,934 B |
| Generated object-file writable static storage | 5,680 B | 4,888 B |
| Generated object-file code plus read-only data | 108,478 B | 102,524 B |
| Object lookup | 54.865 ns | 24.921 ns |
| Full bounded history update | 121.341 ns | 4.177 ns |
| Filled numeric history payload | 8,192 B | 1,024 B |
| Generated `ui.c` source | 322,630 B | 295,141 B |

Heap figures count LVGL-requested payload bytes, excluding allocator overhead,
static data, existing display allocations, framebuffers and unrelated C/C++
allocations. Code/read-only sizes are compiler object sections, not firmware
image sizes. The separate literal/image fixture reduced literal heap payload
from 196 B to zero and compiled asset read-only data from 136 B to 96 B.

These host observations do not predict a board's total RAM, refresh rate or
instruction timings. Use a linker map, the target allocator and device timing
when setting a firmware budget. The [standalone benchmark](../Tests/Benchmarks/GeneratedC/README.md)
records the fixture, matching-archive configuration and measurement method.

Regression coverage includes LVGL 9.6 x64 and 8.3 x86, both text-storage modes,
allocation failures, wrapped/unbounded histories, aliases, bindings, deleted
object lookups and repeated initialization/cleanup. Protobuf library changes
and their separate C++ allocation scope are described in
[the runtime performance guide](../Runtime/PERFORMANCE.md).

## Protobuf display library measurements

The runtime comparison links identical benchmark code against the preserved
baseline and rebuilt serializer/runtime libraries. These figures measure the
incremental **C++ allocation overhead of loading**, excluding existing input
buffers, C/LVGL allocations, PSRAM, framebuffers and allocator headers.

| Loading fixture | Before peak | After peak |
| --- | ---: | ---: |
| Four screens, 160 labels | 411,992 B | 239,968 B |
| 256 nested widgets | 483,955 B | 367,452 B |
| Twenty IDs referencing identical trie data | 657,876 B | 64,841 B |

Retained screen memory stays unchanged. Removing retained initial variable
metadata reduced the history fixture's C++ retained bytes from 8,000 B to
3,680 B. Five thousand scalar updates with initial history dropped from
570,000 C++ allocations to zero. Widget updates can still allocate through
LVGL; this count is not a claim that every bound UI update is allocation-free.

Runtime behavior, eager/lazy screen lifecycle, Windows display loading and
minimal libraries with prediction enabled/disabled passed regression checks.
The changed runtime, trie catalog and variable validator also compile with the
actual ESP-IDF ESP32 and Arduino ESP32-S3 toolchains. No hardware was flashed.
