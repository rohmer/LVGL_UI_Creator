# MicroPython export

Choose **Export > MicroPython project...** in the editor, select an output
folder, and use **Check project before export** to validate the design against
the supported binding. Exporting leaves the editor document and saved path
unchanged.

The command-line exporter accepts JSON and Protobuf projects:

```sh
UICreatorCodeGen --format micropython project.json generated_micropython
UICreatorCodeGen --format micropython project.pb generated_micropython
```

The output contains `ui.py`, `ui.json`, the portable `uicreator_lvgl.py` runtime,
an example `main.py`, deduplicated pixel assets, firmware configuration
requirements and a manifest. The runtime is embedded in the editor and CLI;
exporting needs no installed Python or downloaded firmware source.

## Binding and firmware

The firmware source is the official
[lvgl/lv_binding_micropython](https://github.com/lvgl/lv_binding_micropython)
repository, pinned to `032dda4b7e8419702a4b9c4c8748a4d85d51be61` with its own
LVGL **9.3.0**, revision `c033a98afddd65aaafeebea625382a94020fe4a7`.
The exported manifest records these revisions. Use its LVGL submodule when
building firmware; the editor's LVGL source is independent.

The binding's recommended firmware starting point is
[lvgl/lv_micropython](https://github.com/lvgl/lv_micropython).
Follow its port-specific build instructions, initialize submodules, and merge
the exported `lv_conf.defaults.h` requirements into the binding configuration.
Retain upstream's garbage collector and allocator configuration. Enabling or
disabling LVGL features requires regenerating the binding during the firmware
build. Ordinary MicroPython firmware without `import lvgl` support is
insufficient.

Initialize your display, input drivers and LVGL event loop, then run:

```python
import ui
app = ui.create()
app.set_variable("temperature", 21)  # A variable authored in this project.
value = app.get_variable("temperature")
widget = app.get_object(0, 1)       # Screen index and authored object ID.
app.show_screen(0)
```

Keep `app`, drivers and event-loop objects alive, and call LVGL from the same
owner thread. Call `app.deinit()` before removing the display. Keep application
logic outside the generated files. C/C++ variable symbols map to Python
accessors; application Python updates hardware values through `set_variable`.
The runtime uses the standard MicroPython `array` and `json`/`ujson` modules.
Custom stock-keyboard layouts stay cached once per mode after `deinit()`:
LVGL retains these pointers globally. Live UIs keep any layout snapshots they
borrowed until their own cleanup.

## Compatibility

Native LVGL 9.3 widgets, supported styles and layouts, multiple screens,
variables with multiple widget bindings, and interaction actions use the
upstream Python API. Image decoding happens during export; the device loads
raw pixel assets and needs no PNG/JPEG/GIF decoder for these assets. Budget
MicroPython heap for pixel buffers, widgets and retained variable histories.

Barcodes and QR codes are exported as bitmap assets, preserving their authored
content. Re-export to change that content. Dynamic code-widget bindings are
rejected during preflight.

Advanced Keyboard, Text box and Layer view depend on custom C++ widgets and
require C or Protobuf export. Custom bitmap fonts and unsupported newer styles
are reported during preflight rather than silently replaced. Select a supported
built-in font for a portable MicroPython export. The pinned binding also omits
the native Spinner class; preflight reports it and an Arc can be used instead.
The exported README and manifest
describe the requirements for each generated project.

## Verification

The standard build registers MicroPython CLI round-trip, assets, paths,
failure handling, runtime and editor tests. Select them with `ctest -L micropython`.
The runtime unit suite uses a bounded host test double. To also execute generated projects
against a real LVGL-enabled MicroPython interpreter on Linux, configure CMake
with `-DUICREATOR_MICROPYTHON_EXECUTABLE=/absolute/path/to/micropython` and run:

```sh
ctest --test-dir build -R MicroPython --output-on-failure
```

This adds `UICreatorMicroPythonNative`, which uses a display buffer in memory
and needs no panel or graphical window. Selecting just the native test also
regenerates its exported fixture. Windows hosts can set
`UICREATOR_MICROPYTHON_WSL_DISTRIBUTION=Ubuntu` to run a Linux interpreter through
WSL. See [test instructions](../../Tests/README.md#micropython-export) for commands,
coverage and reports. Physical display and touch drivers remain board-specific.
