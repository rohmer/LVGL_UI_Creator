# Arduino Nano ESP32 Protobuf display library

Pass `--enable-xml` to `make_library.py` to include the optional LVGL UI Creator
XML codec. The generated memory and file loaders then accept XML and Protobuf
through the same API. A default Protobuf-only library does not copy the XML
parser into Arduino's recursively compiled source tree; regenerate with the
flag to enable it. The widget profile must cover the XML design's features.
This is LVGL UI Creator's independent format, not LVGL Pro XML.

Generate a self-contained Arduino library for the saved projects your board
will display. This uses the same pinned LVGL 9.6, serializers, Protobuf runtime,
asset handling and minimum safe module closure as the ESP32 component. It
selects only the required widget/codec/layout/font sources. Advanced Keyboard,
QR codes and barcodes are included when used; prediction and built-in model
data have separate feature gates.

Advanced Keyboard supports 14 layouts: English, Spanish, French, German,
Italian, Portuguese, Greek, Mandarin Chinese, Arabic, Hindi, Bengali,
Indonesian, Urdu and Russian. Mandarin includes Pinyin composition; Arabic
and Urdu default to right-to-left display with an optional override. English
includes a built-in dictionary. A selected bundled or imported trie is embedded
with the project for predictions and autocorrect. Focusing a bound field scrolls
its viewport above the keyboard while preserving the saved layout.

```powershell
python Runtime/arduino/make_library.py dashboard.pb settings.pb --output build/LVGLProtobuf --zip
```

Install the generated ZIP through Arduino IDE's **Sketch → Include Library →
Add .ZIP Library**, or copy the library folder into your sketchbook libraries
directory. Install **Arduino ESP32 Boards** in Boards Manager and select
**Arduino Nano ESP32**. The verified FQBN is `arduino:esp32:nano_nora`.

Open **File → Examples → LVGLProtobuf → ProtobufDisplay**. Keep its
`build_opt.h` beside the sketch: this supplies the required C++17, exceptions,
RTTI and LVGL configuration flags to the Nano ESP32 compiler. Copy that file
when integrating the library into another sketch. No separate LVGL library
is required alongside this bundled copy.

Nano ESP32 has no built-in display or touch panel. Implement
`board_write_pixels` and `board_read_touch` in the example using your chosen
panel's Arduino driver, pins and resolution. The example's default callbacks
discard drawing and report no touches so the complete firmware can compile
before choosing hardware. It uses partial rendering with a 20-line buffer.
Signal `lv_display_flush_ready` only after each transfer finishes; adapt the
flush callback when using asynchronous DMA. The example assigns 16 KiB to
Arduino's loop task for document loading and LVGL events. The generated configuration
records the first project's authored width, height and color depth.

```cpp
lv_init();
lvgl_pb_arduino_configure_tick(); // omit if your port already owns LVGL ticks
// Create your LVGL display and optional input driver here.
lvgl_runtime_t* project = nullptr;
char error[192];
lvgl_pb_arduino_load_embedded("dashboard", nullptr, &project, error, sizeof error);
// Call lv_timer_handler() regularly from this same task.
```

Embedded Protobuf arrays remain in flash. On ESP32 boards with enabled,
initialized PSRAM, the prediction engine automatically prefers PSRAM for each
compact trie and index, sharing one copy between keyboards using that model.
Enable your board's PSRAM option before loading a project. If PSRAM is absent
or cannot fit the model, embedded models remain borrowed from immutable flash;
file loads retain owned model bytes. Selectable custom tries use the same engine
as Windows. Each active keyboard owns its inference workspace. Password fields
suppress predictions.
Enter commits autocorrect, emits `LV_EVENT_READY` on the field and keyboard,
and dismisses the keyboard. X emits `LV_EVENT_CANCEL` on the keyboard and
dismisses it. Clicking or focusing the field again restores the keyboard;
`lv_advanced_keyboard_close` dismisses it programmatically without an event.
The public runtime API exposes screens, objects, image/font resources, typed
variables and histories. Destroy the runtime before deleting its display.

For LittleFS, SD or another Arduino filesystem, mount it in your sketch and
use `lvgl_pb_arduino_load_file(filesystem, "/dashboard.pb", ...)`. That bounded
loader owns a temporary input buffer and releases it before returning. You
can also use `lvgl_runtime_load_memory` for temporary bytes or
`lvgl_runtime_load_memory_borrowed_assets` for immutable application storage
that outlives the runtime. `--no-embed` omits flash project arrays.

Compile with Arduino CLI after installing the official board package:

```powershell
arduino-cli core install arduino:esp32
arduino-cli compile --fqbn arduino:esp32:nano_nora --library build/LVGLProtobuf build/LVGLProtobuf/examples/ProtobufDisplay
```

The generator needs Python 3.10+ on the build machine. Arduino firmware has no
Python, editor, SDL or ESP-IDF CMake dependency. Its LVGL allocator uses the
board's C heap; budgets must include display buffers, the decoded property
tree, assets and prediction scratch. Re-run the generator after project
changes; the generated `projects` directory records inputs, while compiled
flash arrays are regenerated by the script. Retain `features.json` to audit
selected sources. Compile checks
do not test a physical panel, touch wiring or scanner.

Validation used Arduino CLI 1.6.0 and Arduino ESP32 Boards
2.0.18-arduino.5, with actual Nano ESP32 builds for a label-only project,
Advanced Keyboard with the built-in English model, and Advanced Keyboard
with a custom trie and the built-in dictionary disabled.
