# Native image converter provenance

The native C++17 image converter adapts the LVGL pixel formats and compression
envelope of
[LVGLImage.py](https://github.com/lvgl/lvgl/blob/d3c5b419aa667775c292c8943edd94e4c42390bc/scripts/LVGLImage.py)
at commit `d3c5b419aa667775c292c8943edd94e4c42390bc`. `UPSTREAM_COMMIT` records
that revision. The unchanged upstream MIT license, including LVGL Kft's copyright
notice, is retained in the adjacent `LICENSE` file and installed as
`licenses/lvgl_image_converter/LICENSE` within the application resources.

Creator compiles its implementation from `UICreator/Images/ImageConverter.cpp`.
Raster decoding, palette quantization, color packing, stride, alpha/chroma-key,
RLE, LZ4 and C/binary/PNG output use linked native codecs. SVG rendering uses the
resvg C API, including filters, text and embedded images. Native regression tests
cover formats, compression round trips, animation and LVGL rendering.

The former Python converter and interpreter helper are no longer vendored or
distributed. Native conversion uses no Python subprocess. CMake and Visual
Studio build the codecs for the selected target architecture; Rust/Cargo builds
the linked resvg and imagequant libraries.

This editor targets LVGL 9.6, so RGB565_SWAPPED is the supported big-endian pixel
format. RAW source bytes require a matching device decoder; RLE and LZ4 require
their corresponding LVGL configuration options.
