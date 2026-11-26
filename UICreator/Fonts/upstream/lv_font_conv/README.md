# Native font converter provenance

The native C++17 font converter adapts the LVGL descriptor/binary layout, bitmap
compression and output behavior of
[lv_font_conv](https://github.com/lvgl/lv_font_conv/tree/24b94aefc751539f5c476b6f22e357500d1ab2c7),
package version 1.5.3 at commit `24b94aefc751539f5c476b6f22e357500d1ab2c7`.
`UPSTREAM_COMMIT` records that revision. The unchanged upstream MIT license is
retained in the adjacent `LICENSE` file and installed as
`licenses/lv_font_conv/LICENSE` within the application resources.

Creator compiles its implementation from `UICreator/Fonts`. FreeType/Brotli
rasterize fonts; native code handles Unicode selection/remapping, kerning, LVGL
bitmap compression, C/binary output and PNG glyph dumps. The additional
lv_font_conv and opentype.js attribution for adapted format/kerning logic remains
in `UICreator/Fonts/NativeFont.LICENSE`, installed as
`licenses/native-converters/NativeFont.LICENSE` within the application resources.

The former vendored JavaScript implementation and Node packages have been
removed. Desktop conversion and normal builds run without Node/npm. Maintainers
regenerating assets with an upstream converter must obtain the pinned upstream
tool separately and supply it explicitly to the relevant generation script.
