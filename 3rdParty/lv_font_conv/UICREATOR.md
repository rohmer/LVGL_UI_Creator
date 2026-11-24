# UICreator integration

Upstream: https://github.com/lvgl/lv_font_conv

Pinned commit: `24b94aefc751539f5c476b6f22e357500d1ab2c7`, package version 1.5.3.
The upstream JavaScript implementation is retained as the pinned format
reference and for optional maintainer font-generation scripts. Its MIT license
is in LICENSE. It is not copied into application packages or launched by the
editor's converters.

The application uses native C++17 code in `UICreator/Fonts`: FreeType/Brotli
rasterization, Unicode selection/remapping, kerning, LVGL bitmap compression,
C/binary output and PNG glyph dumps. `NativeFont.LICENSE` retains lv_font_conv
and opentype.js attribution for ported format/kerning logic. Native regression
fixtures compare against the upstream format and exercise LVGL loading/rendering.

Node/npm are needed only when explicitly running the optional legacy maintainer
generators, not for desktop conversion, normal builds or installed applications.
