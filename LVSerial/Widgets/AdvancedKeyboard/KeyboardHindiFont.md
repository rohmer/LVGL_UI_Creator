# Hindi keyboard glyphs

`KeyboardHindiFont.c` provides `espos_keyboard_hindi_font`, a 16-pixel, 4-bpp
LVGL font for Devanagari U+0900–U+097F and dotted circle U+25CC. Existing keyboard
fonts provide Latin text and control symbols through the font fallback chain.
Set `LV_ADVANCED_KEYBOARD_USE_HINDI_FONT=1` when compiling the font and keyboard.
Builds that omit this source leave the feature disabled.

The source is Google Fonts' Noto Sans Devanagari, pinned to revision
`2bc5d431b584d89e646214bf6cfa75494e76ceea`. Its SHA256 is
`14ec4af41f27482216d1c2229f417ff9b1425e1babb014e57d1d40d03229853e`.
The complete SIL Open Font License 1.1 is in `KeyboardHindiFont.LICENSE.txt`.

From a checkout with Node.js and the bundled `lv_font_conv` dependencies, run
`python LVSerial/Widgets/AdvancedKeyboard/GenerateHindiFont.py`. The script checks
the pinned font and license hashes before generating C. `--cache` selects an
existing cache for offline regeneration; `--node` selects a Node executable.

Hindi input is stored in logical Unicode order, including vowel signs, virama
and combining marks. Stock LVGL has limited support for Devanagari conjuncts,
OpenType substitutions and mark positioning. The glyph asset does not supply
a shaping engine. Applications requiring fully shaped Hindi runs need a text
renderer with Indic shaping; Windows native text controls provide that support.
See [LVGL's font limitations](https://lvgl.io/docs/open/9.4/details/main-modules/font.html).
