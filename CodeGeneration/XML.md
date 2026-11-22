# UI Creator XML projects

Choose **Export > UI Creator XML (.xml)...** in the editor. Select the runtime's
LVGL version and use **Check project before export** to validate the project.
The output is one self-contained XML file with the same widgets, screens,
variables, bindings, images, fonts and keyboard models used by Protobuf.
Exporting preserves the open document and its saved path.

The format is UI Creator's own typed project representation. Its versioned
`uicreator` envelope and property/value tree are documented in
[the format specification](../LVSerial/Serialization/Xml.md). XML is editable
text, with numeric and string types preserved rather than inferred from text.
Binary resources use base64; the project itself remains a readable hierarchy.

## Command line

```sh
LVGLCompiler project.json --format xml --target 9.6 --output screen.xml
UICreatorCodeGen --format xml project.pb screen.xml
LVGLCompiler screen.xml --format protobuf --output screen.pb
UICreatorViewer screen.xml --frames 20 --screenshot preview.bmp
```

Both compiler frontends accept XML as input for C or MicroPython generation.
Use the same supported-widget requirements as those output backends. The editor
also opens XML projects; Save offers an editor JSON path, and Export produces a
new XML device file. Invalid exports leave existing output files intact.

## Runtime

The existing C API recognizes Protobuf and XML automatically:

```c
lvgl_runtime_t *runtime = NULL;
char error[256];
/* Initialize LVGL, your display and the LVGL filesystem driver first. */
if (!lvgl_runtime_load_file("S:/screen.xml", NULL, &runtime, error, sizeof error)) {
    /* Report error to your application. */
}
/* Use the normal widget, variable and screen APIs. */
lvgl_runtime_destroy(runtime);
```

`lvgl_runtime_load_memory` and `lvgl_runtime_load_memory_ex` accept XML bytes
too. Loading restores the same typed record and uses the existing validator and
renderer directly. Advanced Keyboard, imported tries, barcodes, QR codes,
multi-widget variable bindings, histories, actions and lazy screen policies
share their Protobuf implementation. XML input may be freed after loading;
XML resources remain owned even when `LVGL_RUNTIME_BORROW_ASSETS` is requested.

Full builds enable XML by default. Set `-DLVSERIAL_USE_XML=OFF` to omit the
codec from the build. Minimal device profiles omit it by default; enable it
when generating a profile for the same design:

```sh
python Runtime/esp32/make_component.py project.pb --output build/device --enable-xml
python Runtime/arduino/make_library.py project.pb --output build/arduino --enable-xml
python Runtime/pico/make_sdk.py project.pb --board pico2 --output build/pico2 --enable-xml
```

The generated CMake projects expose `LVGL_PB_ENABLE_XML`; ESP-IDF also exposes
`CONFIG_LVGL_PB_XML`. XML enables a second input format without adding widgets
outside the profile's selected feature set. Generate the profile from the same
project's Protobuf or JSON; this initial version does not analyze XML inputs
when choosing the minimum LVGL source closure. Direct generated-C Pico output
has no runtime loader and does not need the XML codec.

XML is larger and requires more load-time memory than compact Protobuf with
borrowed flash assets. Use Protobuf for the smallest device footprint. Both
loaders enforce the configured input limit, and the XML reader also bounds
depth, element count and text length. The format does not resolve external
entities or load external XML resources.

## Tests

```sh
ctest --test-dir build/release -L xml --output-on-failure
```

The suite checks the codec, typed boundaries, Unicode and escaping, Protobuf/XML
pixel parity, assets and predictions after releasing input memory, lazy screen
lifecycles, LVGL filesystem loading, CLI conversion and editor export/open.
Reduced-runtime tests independently compile the parser-enabled and parser-free
profiles. Physical display, touch and board memory usage need hardware tests.
