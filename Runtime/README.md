# LVGL project runtime

Use the [ESP32](esp32/README.md), [Arduino Nano ESP32](arduino/README.md),
[Raspberry Pi/Linux](linux/README.md), or [Windows](windows/README.md) toolkit
to package the renderer for your platform. They share the same Protobuf schema,
Advanced Keyboard engine, external text-field bindings and imported tries.

Authored [event actions and validation](INTERACTIONS.md) run on these platforms and in the editor's Run screen. The authoring preview keeps the same bindings as data until Run or Debug starts.

Native backends target **LVGL 9.6.x, 9.5.x, 9.3.x, 9.2.x, 9.1.x, and 8.3.x**, with a stable C-callable API in `include/lvgl_runtime.h`. Firmware supplies LVGL, a display, input drivers, ticks, and periodic `lv_timer_handler()` calls. All runtime calls and widget access must run on the LVGL thread. Keep the display alive until its runtimes are destroyed.

## Build in firmware

```cmake
# In the firmware parent project use project(Firmware LANGUAGES C CXX).
# Add your selected LVGL library first; it must export lvgl or lvgl::lvgl.
set(LVGL_RUNTIME_TARGET "9.3" CACHE STRING "" FORCE) # Match your LVGL version
add_subdirectory(path/to/LVGL_UI_Creator/Runtime lvgl_runtime)
target_link_libraries(firmware PRIVATE LVGLRuntime::Runtime)
```

The implementation requires C++17, exceptions, RTTI as configured by your standard library, and heap memory. Its public API is usable from C; link firmware with the C++ linker. Internally it uses nanopb and a typed nlohmann document tree, reusing LVSerial's widget deserializers. This is not an allocation-free streaming renderer. No JSON text parsing, SDL, editor UI, Node, Python, FreeType, or converter runtime is needed to load a compiled project. A standalone runtime build excludes the host filesystem image-import function. Budget for the decoded property tree during load plus the LVGL objects, resources, and variable history retained afterward.

For a self-contained ESP32 component with bundled LVGL and a project-specific feature profile, use [the ESP32 component builder](esp32/README.md). Its minimal runtime build compiles widget adapters only for enabled `LV_USE_*` features, rejects project widgets omitted from that build before creating screens, and can omit all legacy PNG/JPEG/GIF decoding when compiled device image frames are available. Full editor and existing standalone builds retain their complete widget feature set.

Memory loading borrows the Protobuf input while decoding instead of making another complete input copy. Text properties decode directly into their owned strings. Duplicate-path validation retains keys only in currently open containers instead of retaining every property path in the document. The loader still allocates its typed document and retained widget models/resources; the input can be released after the load returns. File loaders require their own bounded input buffer.

Enable `LV_USE_FONT_COMPRESSED` for compressed fonts. Enable all widget features used by your project (including chart, scale, tabview, tileview, window and messagebox). The backends instantiate the complete editor widget set. LVGL 9.6 assets use their exact device color format, stride, flags and compressed bytes. Older backends normalize imported assets through their embedded previews to native ARGB8888; LVGL 8.3 uses native true-color-alpha descriptors and requires `LV_COLOR_DEPTH=32`. Enable `LV_USE_RLE`, `LV_USE_LZ4`, and `LV_BIN_DECODER_RAM_LOAD` for compressed images; RAW images need the matching LVGL decoder (for example LODEPNG or TJPGD). Indexed/NEMA data needs hardware support appropriate to the exported format. Older embedded PNG/JPG/GIF assets without device frames are decoded in software to ARGB8888. GIF animation timers are owned by the runtime. Shared image descriptors and restored font storage are owned per runtime and released after its objects are deleted.

## Load

Compile with `LVGLCompiler project.json --target 9.6 --format protobuf --output project.pb`. Load bytes from flash/network or use LVGL's registered filesystem driver:

```c
#include "lvgl_runtime.h"
lvgl_runtime_t *project = NULL;
char error[256];
if (!lvgl_runtime_load_file("S:/project.pb", NULL, &project, error, sizeof error)) {
    /* report error */
}
/* Or lvgl_runtime_load_memory(data, size, NULL, &project, error, sizeof error). */
lv_obj_t *widget = lvgl_runtime_object(project, 0, saved_object_id);
lvgl_runtime_show_screen(project, 1);
/* After all application use: */
lvgl_runtime_destroy(project);
```

NULL options select the default display, load the initial screen, and limit input to 16 MiB. Set options to override the display, limit, or initial screen loading. Failed loads clean up their screens/resources and restore the previous active screen. Source bytes may be released after loading. The caller owns the runtime; don't delete its display first. Deleting widgets/screens manually invalidates their lookup handles safely; recreating them requires loading a new project. Destroying an active project restores its previous screen, or creates a blank screen if that screen was deleted.

`lvgl_runtime_font` and `lvgl_runtime_image` look up project assets by stable ID. Image lookup returns the first `lv_image_dsc_t` frame on LVGL 9 or `lv_img_dsc_t` on 8.3. Borrowed resources expire at destroy. Variables expose typed get/set and saved history. Setters update supported widget bindings; interactive VALUE_CHANGED events update variables. String pointers are borrowed until mutation/destroy. History length zero grows without a bound, as in the editor/generator. Bindings support the same property subset as C generation: text, numeric widget values/selections, checked/hidden/disabled, geometry, opacity, LED brightness, and custom Text box/Layer view values. Application actions/hardware behavior are supplied by firmware.

The Advanced Keyboard requires LVGL 9.6 and `LVSERIAL_USE_ADVANCED_KEYBOARD=1`, with button, label and textarea support. It contains keys and suggestions and edits independent textareas or Custom Text box entries. `targetOid` selects a field; its default `-1` with `autoFocus=true` follows fields on the authored screen. It stays open across outside clicks until Enter/Done or the X control closes it. Enter commits autocorrect and sends READY to the field and keyboard without adding a newline; X sends CANCEL only to the keyboard. Both clear the active target before application callbacks and retain focus watchers so clicking a field reopens it. Programmatic newlines remain intact. Earlier Creator hide-on-defocus records are upgraded to this behavior. Text bindings belong to the external field; switching targets preserves each field's text and configuration, and deleting either side safely detaches it.

Full builds include the ESPOS suggestion/autocorrect engine and shared built-in English trie. `trieId` chooses the built-in model or a project trie, and `autocorrectThreshold` defaults to 0.3. Predictors borrow immutable model/index bytes and own their scratch workspace. Normal memory loading retains owned trie bytes; the explicit `lvgl_runtime_load_memory_borrowed_assets` API can retain trie/index spans in immutable flash input until runtime destruction. Project-specific firmware can omit the engine or built-in model separately. Prediction is suppressed for password fields. No hardware driver is included.

`lvgl_runtime_screen_size(runtime, index, &width, &height)` returns the authored screen dimensions. These remain stable when changing the display resolution causes LVGL to resize the active root object. Invalid runtimes, indices or output pointers return false.

## Version backends

`LVGL_RUNTIME_TARGET` selects 9.6, 9.5, 9.3, 9.2, 9.1, or 8.3 (default 9.6). All six are implemented. The selected target must match the LVGL headers/library supplied by firmware; a mismatch fails compilation. Compile Protobuf with the matching `LVGLCompiler --target` option. The project wire format retains the editor's 9.6 authoring model; the target stamp controls native adaptation. Different target stamps are rejected rather than silently interpreted as the selected version. Protobuf wire format versions 1 and 2 are accepted.

All backends share lifecycle, hierarchy validation, typed variables/history, resource ownership, and rollback in `common/`. Version-specific native API and composite adapters live in `compat/`. Call the same public C API on every version. Link one backend per firmware image. Exporting live device state back into an editor project is not part of this loading API; older embedded serializer helpers retain the saved authoring metadata where native getters are unavailable.

The older backends apply the C generator's capability checks. Unsupported nondefault flags/styles, forced orientations on releases without that API, nonuniform 8.3 image scaling, 8.3 arc knob offsets, and unsupported gradients fail explicitly. In addition, complex gradients require the corresponding native LVGL configuration. Native 8.3 image/canvas dimensions cannot exceed 2047 pixels. Its scale, tabview, messagebox, and aligned image implementations are composites, so use the returned runtime object and native-compatible helpers rather than assume each root has a newer LVGL class. Original/imported PNG/JPG/GIF and manager-converted assets retain software preview data for portable loading; custom compressed 8.3 image records must include that preview.

The version matrix is tested against the vendored 9.6 source and official releases 9.5.0, 9.3.0, 9.2.2, 9.1.0, and 8.3.11. See [test instructions](../Tests/README.md). Run `python Runtime/compat/Generate.py` after changing the generator's compatibility headers or capability table to refresh runtime flag adapters. Python is needed only for this development step, not by firmware.
