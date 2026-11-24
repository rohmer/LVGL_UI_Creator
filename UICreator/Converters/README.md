# Native font and image conversion

The editor calls C++17 conversion libraries directly on its conversion worker.
No Node, Python converter process, or converter helper directory is required.
FreeType and the bundled Brotli decoder rasterize fonts. Native PNG, LZ4,
imagequant and the resvg C API provide image codecs and SVG rendering.
Rust/Cargo is a build dependency of the linked imagequant and resvg libraries.
Firmware project creation retains its separate Python helper.

Build the standalone converter with:

```sh
cmake --build build-directory --config Release --target UICreatorConverter
```

The executable accepts UTF-8 JSON requests:

```sh
UICreatorConverter font --request font-request.json
UICreatorConverter image --request image-request.json
```

A font request supplies the same options as the editor:

```json
{
  "args": ["--font", "/path/to/font.ttf", "--range", "0x20-0x7E",
           "--size", "24", "--bpp", "4", "--format", "lvgl",
           "--output", "/path/to/font.c", "--lv-font-name", "ui_font_24"]
}
```

An image request uses the editor's conversion settings:

```json
{
  "sources": ["/path/to/image.svg"],
  "options": {"colorFormat": "ARGB8888", "format": "BIN", "compression": "LZ4", "align": 4}
}
```

Both commands write `result.json` beside the request. Font conversion also writes
`preview.bin`; image files are placed in a `converted` subdirectory. The result
contains file paths and the same image asset records used by the editor.
Invalid requests return a nonzero exit code and an error message.

CTest's `UICreatorNativeConverters` copies only native binaries into an isolated
Unicode-path folder, removes interpreters from PATH and exercises font and
SVG/LZ4 conversion. The font and image integration suites independently check
packed output, LVGL loading/rendering, compression, animation and cancellation.
