#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH "SDL3/SDL.h"
#define LV_SDL_DIRECT_EXIT 0
#define LV_MEM_SIZE (16 * 1024 * 1024)
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_USE_FS_MEMFS 1
#define LV_FS_MEMFS_LETTER 'M'
#define LV_USE_RLE 1
#define LV_USE_FONT_COMPRESSED 1
/* The editor must preview imported large glyphs. Generated device profiles
 * choose this independently from their actual font metrics. */
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_USE_LZ4 1
#define LV_BIN_DECODER_RAM_LOAD 1
#define LV_USE_LODEPNG 1
#define LV_USE_TJPGD 1
#define LV_USE_BARCODE 1
#define LV_USE_QRCODE 1
#define LV_USE_BIDI 1
/* LVGL's label shaping mutates textarea storage. Keep editable Unicode logical. */
#define LV_USE_ARABIC_PERSIAN_CHARS 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 1
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK 1
#define LV_ADVANCED_KEYBOARD_USE_HINDI_FONT 1
#define LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT 1
#define LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT 1
#define LV_ADVANCED_KEYBOARD_USE_GREEK_FONT 1
#define LV_ADVANCED_KEYBOARD_USE_URDU_FONT 1
#define LV_ADVANCED_KEYBOARD_USE_PINYIN 1

#endif
