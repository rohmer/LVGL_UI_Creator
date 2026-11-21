# LVGL image converter integration

Upstream: https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py

Pinned commit: `d3c5b419aa667775c292c8943edd94e4c42390bc` (see UPSTREAM_COMMIT).
The original script is unchanged and retains its upstream MIT license.

`UICreator/Images/image-converter-bridge.py` adapts PNG/JPEG/BMP/GIF and SVG input
using Pillow and resvg, then delegates LVGL color packing, stride, premultiplication,
RLE, LZ4 and C/binary output to the official script. The adapter replaces the
script's external pngquant shell call with Pillow palette quantization and fixes
its RGB565A8 PNG unpacking path. PNG preview generation happens before accelerator
palette layout and alpha premultiplication, so the editor shows the visual colors.

requirements.txt pins build dependencies. CMake builds a private environment and
freezes the adapter, Python and dependencies into `bin/image-converter` using
PyInstaller. Runtime conversion is offline and does not require a Python install.
Ship that entire folder beside UICreator.exe. The original script and license are
copied into its upstream directory. Native Visual Studio builds use
`UICreator/Images/BundleImageConverter.ps1` to prepare the same runtime.

This editor targets LVGL 9.6, so RGB565_SWAPPED is the supported big-endian pixel
format rather than the older LVGL 8 true-color/chroma-key encodings. Chroma-key
selection is applied to the source alpha channel and must use an alpha/indexed
output format. RAW source bytes require a matching device decoder; RLE and LZ4
require their corresponding LVGL configuration options.
