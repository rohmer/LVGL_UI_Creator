# Optional keyboard script fonts

The keyboard has five 16-pixel, 4-bpp glyph assets. Each includes its own complete
SIL Open Font License 1.1 notice. Source font binaries are downloaded only for
regeneration and are not needed by applications or firmware builds.

| Script | Font source | Generated symbol | Feature flag |
| --- | --- | --- | --- |
| Hindi | Noto Sans Devanagari | `espos_keyboard_hindi_font` | `LV_ADVANCED_KEYBOARD_USE_HINDI_FONT` |
| Bengali | Noto Sans Bengali | `espos_keyboard_bengali_font` | `LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT` |
| Russian/Cyrillic | Noto Sans | `espos_keyboard_cyrillic_font` | `LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT` |
| Greek | Noto Sans | `espos_keyboard_greek_font` | `LV_ADVANCED_KEYBOARD_USE_GREEK_FONT` |
| Urdu | Noto Naskh Arabic | `espos_keyboard_urdu_font` | `LV_ADVANCED_KEYBOARD_USE_URDU_FONT` |

Feature flags default to zero for portable builds. Enable the relevant flag and
compile its `Keyboard…Font.c` source. The keyboard's script font falls back to
the existing Latin/control-symbol font. Mandarin uses LVGL's bundled Source Han
Sans SC CJK font and its matching Pinyin character dictionary. Arabic uses
LVGL's bundled DejaVu Persian/Hebrew glyph font. Indonesian needs no extra font.

`GenerateHindiFont.py --font all` regenerates all five assets. Individual script
names select a single asset. The script pins Google Fonts commits and verifies
SHA256 for every source font and OFL license. Its generated C header also records
the original source hash. Use `--cache` for offline regeneration or `--converter`
to select a separately installed `lv_font_conv.js` (for example in an ESPOS
checkout).

Text is stored in logical Unicode order. Set `LV_USE_BIDI=1` for Arabic/Urdu
direction and mixed-script cursor mapping. Keep `LV_USE_ARABIC_PERSIAN_CHARS=0`
for editable textareas: LVGL 9.6's Arabic preprocessing can replace the stored
characters with presentation forms. The RTL option changes display direction;
it never reverses stored text.

Stock LVGL does not provide full OpenType shaping for Hindi/Bengali conjuncts,
vowel placement or every Arabic/Urdu contextual form. These glyph assets do not
add a shaping engine. Windows native text controls in Keyboard Simulator use
the operating system's complex-script shaping. Applications needing equivalent
native LVGL rendering require a suitable shaping renderer. See
[LVGL's font limitations](https://lvgl.io/docs/open/9.4/details/main-modules/font.html).
