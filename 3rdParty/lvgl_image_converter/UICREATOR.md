# LVGL image converter integration

Upstream: https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py

Pinned commit: `d3c5b419aa667775c292c8943edd94e4c42390bc` (see UPSTREAM_COMMIT).
The original script is unchanged and retains its upstream MIT license.

The application now implements conversion in native C++17 in
`UICreator/Images/ImageConverter.cpp`. The upstream script remains a pinned
MIT-licensed reference; its license is shipped with the native port.

Raster decoding, palette quantization, color packing, stride, alpha/chroma-key,
RLE, LZ4 and C/binary/PNG output run without Python. SVG uses the linked native
resvg C API, preserving filters, text and embedded images. Native regression
tests cover each format, compression round trips, animation and LVGL rendering.

There is no interpreter helper directory to distribute. CMake and Visual Studio
build native codec libraries for the selected target architecture. The resvg
and imagequant libraries use Rust/Cargo at build time; the conversion logic and
editor API are C++.

This editor targets LVGL 9.6, so RGB565_SWAPPED is the supported big-endian pixel
format. RAW source bytes require a matching device decoder; RLE and LZ4 require
their corresponding LVGL configuration options.
