# Barcode and QR-code widgets

**Advanced Widgets** includes **Barcode** and **QR code**. They use native LVGL
encoder/canvas widgets. Project files retain the content and encoding settings;
they do not embed a raster copy of the generated code. JSON, device Protobuf,
the on-device runtime and C export preserve both widget types.

Select a code to edit its **Data**, dark/light colors and encoding options in
Properties. Barcode supports raw Code 128 and GS1 Code 128, scale 1–16, horizontal
or vertical direction and tiled rendering. GS1 translates supplied `[FNC1]`
markers and removes spaces during encoding. Supply the leading FNC1 and valid
GS1 application-identifier data yourself; the widget does not insert or validate
them. Raw content accepts 1–1024 ASCII
bytes without NUL characters. It does not implement EAN, UPC, Code 39 or other
barcode symbologies.

QR supports a square size of 29–2048 pixels and a quiet-zone option. Its project
data is text; the device format retains its byte length, including embedded NUL
bytes supplied programmatically. Capacity also depends on the encoded content
and selected size. An unencodable change reports an error and restores the
previous valid code. Keep the quiet zone enabled and use contrasting colors for
scanning. Physical readability depends on module size, display quality and the
scanner.

The code widgets require the **bundled LVGL 9.6** implementation. Older compiler
targets reject them explicitly. Firmware features are `LV_USE_BARCODE` or
`LV_USE_QRCODE`, with their canvas/image dependencies; project-specific library
profiles select the necessary implementation. No barcode/QR features are needed
for a project without these widgets.

The renderer batches properties in native deferred-update mode, then encodes
once. Unchanged settings do not re-encode; changing colors updates the palette without
encoding again. Bitmaps use indexed monochrome canvas storage, and the native
LVGL object owns its bitmap and payload. Normal display refreshes reuse the
existing bitmap. Code 128 also caches its encoded bar pattern between resizes.

On a device, obtain the object with `lvgl_runtime_object(project, screen, id)` and
use the native `lv_barcode_update()` or `lv_qrcode_update()` APIs to display new
data. QR takes an explicit byte length. Check the returned LVGL result, and make
these calls on the LVGL thread. Device-side state export back into an editor
project is outside the loading runtime's API.
