# ESP32 Protobuf display component

Generate a standalone ESP-IDF component around the projects your firmware will
display. The output contains the pinned LVGL 9.6 implementation, LVSerial,
nanopb, JSON headers, the C-callable runtime, and its required licenses. It has
no editor/SDL/FreeType/Node/Python dependency on the device.

```powershell
python Runtime/esp32/make_component.py dashboard.pb settings.pb --output build/lvgl_pb_esp32 --zip
```

Saved self-contained JSON is also accepted. Protobuf input is decoded from the
typed wire schema, including compact version-2 defaults. The generator takes
the union of all input projects, follows widget dependencies (for example QR →
canvas → image, keyboard → textarea/button-matrix → label), detects layouts,
font and image codecs, and emits both a configuration and an explicit source
list. It excludes unused widget sources, display/GPU/OS backends, converters,
examples, demos, unused built-in fonts and unused image libraries. Basic core,
software rendering, an inherited default theme and one fallback font remain.
`features.json` records selected modules, reasons, source counts and input sizes.
This is the minimum safe feature closure for the saved projects, rather than a
claim of globally optimal firmware size. Section garbage collection removes
unreferenced functions from the retained modules when the application links.

Advanced Keyboard binds independent textareas and appears when a bound field
receives focus. It needs only the button, label and textarea modules. The LVGL
keyboard, button matrix, dropdown and flex modules are unnecessary. Its default
`builtin-en-us` selection uses the portable ESPOS prefix/autocorrect engine and
a small English NGTR model with its prebuilt NGIX index. Imported custom models
are shared project assets referenced by `advancedKeyboard.trieId`; an empty ID
disables prediction. A no-model profile omits the three engine translation units
and model tables, while a custom-only profile omits the built-in English tables.
Projects without Advanced Keyboard omit all its native implementation and font.

The generator validates imported NGTR v3 structure and indices, generates missing
NGIX data on the build machine, removes unused catalog entries and stores model
bytes as binary Protobuf payloads. `lvgl_pb_esp32_load_embedded` borrows those
immutable payloads directly inside flash; it does not expand the trie or copy
its model/index into RAM. Files loaded through VFS use owned shared model bytes
so their temporary input buffer can be freed. Each keyboard has a private,
reused inference workspace; query its exact compiled size with
`lv_advanced_keyboard_predictor_workspace_bytes()`. Inference does not allocate.
The public prediction callback remains available for application-specific engines.
No ESPOS board driver, default flash partition or hardware input port is imported.
Source, font and vocabulary notices are retained in each bundle.

If an asset has exact device frames, redundant originals and preview images are
removed from the bundled Protobuf by default. Firmware still retains its device
frames/delays and stable resource IDs. `--retain-editor-assets` preserves the
authoring data. Legacy embedded PNG/JPEG/GIF without device frames enables only
the needed software import decoder. Reconvert these assets in the editor for
lower peak memory and a smaller embedded input. GPU-only/NEMA/YUV pixel formats
are rejected: reconvert for the ESP32 software renderer first.
External image filenames are rejected because their bytes and required decoder
cannot be established from a self-contained Protobuf profile. Import/embed those
images before bundling; loading the Protobuf file itself through VFS remains
supported.

## ESP-IDF integration

Copy the generated folder to `your_firmware/components/lvgl_pb_esp32`, or list
its absolute path in `EXTRA_COMPONENT_DIRS`. Include the supplied
`sdkconfig.defaults` in your project defaults. ESP-IDF 5.3+ is required; enable
C++ exceptions for validation rollback. The implementation uses C++17 and RTTI;
the public interface is C. Do not add a second LVGL component to the same
firmware. Your board supplies display/input drivers and its display buffers.

After initializing LVGL and a display:

```c
#include "lvgl_pb_esp32.h"

lv_init();
lvgl_pb_esp32_configure_tick();  /* omit if your port already supplies ticks */
/* Create/register your board's LVGL display and input drivers here. */
lvgl_runtime_t *project = NULL;
char error[192];
if (!lvgl_pb_esp32_load_embedded("dashboard", NULL, &project, error, sizeof error)) {
    /* Show/log error. Failed loads release partial objects and resources. */
}
/* The same LVGL task calls lv_timer_handler() regularly. */
/* lvgl_runtime_object(project, screen_index, saved_id) attaches board behavior. */
/* lvgl_runtime_destroy(project) before deleting its display. */
```

`lvgl_pb_esp32_load_file("/spiffs/dashboard.pb", ...)` accepts a mounted ESP-IDF
VFS path and releases its temporary input buffer after loading. To supply a
flash, network or memory-mapped partition buffer directly, use
`lvgl_runtime_load_memory`. `--no-embed` omits project flash arrays when firmware
uses files or partitions. Runtime screen/resource/typed-variable access remains
available through `lvgl_runtime.h`.

All loads, lookups and LVGL calls run on the application's LVGL task (or under
its external LVGL mutex). The component uses `LV_OS_NONE`, one software draw
unit and the ESP monotonic tick callback; it creates no extra timer task or
internal RTOS scheduler. The caller drives `lv_timer_handler` and chooses an
appropriate FreeRTOS delay. Keep filesystem/network operations away from the
render loop when responsiveness matters.

The loader owns a decoded property tree, object metadata, assets and variable
histories. It is not allocation-free or a streaming renderer. The LVGL allocator
uses ESP-IDF's C heap, avoiding a separate fixed LVGL memory pool. Enable PSRAM
allocation in your board configuration if required; DMA display buffers still
need DMA-capable memory. Budget for the temporary file buffer, decoded project,
retained resources, LVGL objects and display buffers. No universal RAM estimate
is possible without the project and display driver. Keep variable histories
bounded for constrained devices.

`features.json` reports trie model/index byte counts and selected prediction
features. Include those bytes when sizing the firmware application partition;
larger imported models need a partition that fits the linked image on the board.
The SPI example selects ESP-IDF's 1.5 MB single-factory layout, which fits 2 MiB
or larger flash. It does not add OTA slots. Configure the physical flash size and
your own partition layout in menuconfig when using different hardware or OTA;
the library's general `sdkconfig.defaults` does not select a partition scheme.
For an application-owned memory mapping, the opt-in
`lvgl_runtime_load_memory_borrowed_assets` API has the same flash behavior: keep
the input immutable and mapped until runtime destruction. The existing
`lvgl_runtime_load_memory` API continues owning its trie resources and allows
the caller to release its input immediately after loading.

## Updating a project and checking size

Re-run the generator when a saved project changes. A bundle refuses changed
`projects/*.pb` at CMake configuration rather than silently building a stale
feature set. `--replace` refreshes a previously generated component and discards
edits inside it; keep application/driver code outside that folder. An arbitrary
downloaded Protobuf can only use the features compiled into its profile; disabled
widgets fail explicitly. Include all intended designs as generator inputs.

Use `idf.py size` and `idf.py size-components` after building to measure actual
flash/static RAM. `features.json` source-byte counts are an audit of the selected
sources, not a substitute for these linked firmware measurements. The same
generated component supports ordinary host CMake for automated load/render and
disabled-widget tests using the exact pruned source/configuration set.

The generated component's `example/` folder provides an ESP-IDF SPI ST7789
wiring example. Set its pins, resolution and panel options for your hardware:

```powershell
idf.py -C build/lvgl_pb_esp32/example set-target esp32
idf.py -C build/lvgl_pb_esp32/example menuconfig
idf.py -C build/lvgl_pb_esp32/example build
```

The example selects the first bundled design unless a project name is set in
menuconfig. Source checkouts also provide `Runtime/esp32/example`; pass
`-DLVGL_PB_COMPONENT_DIR=absolute/path/to/generated/component` when using that
unbundled example. A successful host or ESP32 cross-build does not verify a
physical display or wiring.
