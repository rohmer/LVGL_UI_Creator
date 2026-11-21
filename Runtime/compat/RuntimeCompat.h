#pragma once
#include <lvgl.h>
#if __has_include(<src/widgets/arc/lv_arc_private.h>)
#include <src/widgets/arc/lv_arc_private.h>
#endif
#if __has_include(<src/widgets/chart/lv_chart_private.h>)
#include <src/widgets/chart/lv_chart_private.h>
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 6
#include "Compat9.h"
#elif LVGL_VERSION_MAJOR == 8
#include "Compat8.h"
#endif

#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 3
#define LV_BUTTONMATRIX_CTRL_RECOLOR ((lv_buttonmatrix_ctrl_t)0)
#define lv_calendar_set_month_shown lv_calendar_set_showed_date
static inline uint32_t lv_arc_get_change_rate(lv_obj_t* o) { return ((lv_arc_t*)o)->chg_rate; }
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 2
typedef int lv_bar_orientation_t;
static inline int lv_bar_get_orientation(lv_obj_t*) { return 0; }
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 3
typedef int lv_slider_orientation_t;
static inline int lv_slider_get_orientation(lv_obj_t*) { return 0; }
#endif

#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 4
#if LVGL_VERSION_MINOR < 2
#define LV_IMAGEBUTTON_STATE_NUM _LV_IMAGEBUTTON_STATE_NUM
#endif
#define lv_tabview_set_tab_text lv_tabview_rename_tab
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 4
#if LVGL_VERSION_MINOR < 2
#define lv_ll_get_head _lv_ll_get_head
#define lv_ll_get_next _lv_ll_get_next
#endif
static inline void lv_chart_remove_cursor(lv_obj_t* object, lv_chart_cursor_t* cursor) {
#if LVGL_VERSION_MINOR < 3
    _lv_ll_remove(&((lv_chart_t*)object)->cursor_ll, cursor);
#else
    lv_ll_remove(&((lv_chart_t*)object)->cursor_ll, cursor);
#endif
    lv_free(cursor); lv_obj_invalidate(object);
}
#endif

#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 3
static inline void lv_obj_set_flag(lv_obj_t* o, lv_obj_flag_t f, bool value) { if(value)lv_obj_add_flag(o,f); else lv_obj_remove_flag(o,f); }
#endif

#if LVGL_VERSION_MAJOR == 8
#if LV_COLOR_DEPTH != 32
#error LVGL 8.3 runtime images require LV_COLOR_DEPTH 32
#endif
typedef int lv_bar_orientation_t;
typedef int lv_slider_orientation_t;
static inline int lv_bar_get_orientation(lv_obj_t*) { return 0; }
static inline void lv_obj_set_flag(lv_obj_t* o, lv_obj_flag_t f, bool value) { if(value)lv_obj_add_flag(o,f); else lv_obj_clear_flag(o,f); }
static inline uint32_t lv_color_to_u32(lv_color_t c) { return lv_color_to32(c); }
#define lv_image_cache_drop lv_img_cache_invalidate_src
static inline void lv_image_header_cache_drop(const void*) {}
#define LV_STYLE_BG_IMAGE_SRC LV_STYLE_BG_IMG_SRC
#define LV_STYLE_ARC_IMAGE_SRC LV_STYLE_ARC_IMG_SRC
#define lv_obj_set_style_bg_image_src lv_obj_set_style_bg_img_src
#define lv_obj_set_style_arc_image_src lv_obj_set_style_arc_img_src
#define lv_obj_set_style_bitmap_mask_src(o,s,sel) ((void)0)
#define LV_BUTTONMATRIX_CTRL_RECOLOR ((lv_buttonmatrix_ctrl_t)0)
#endif

#if LVGL_VERSION_MAJOR == 8
#define lv_calendar_set_month_shown lv_calendar_set_showed_date
#define lv_buttonmatrix_has_button_ctrl lv_btnmatrix_has_btn_ctrl
#define LV_LAYOUT_NONE 0
#endif

#if LVGL_VERSION_MAJOR == 8
typedef int lv_image_align_t;
typedef int lv_scale_mode_t;
typedef lv_coord_t lv_value_precise_t;
#define lv_button_class lv_btn_class
#define lv_buttonmatrix_class lv_btnmatrix_class
#define lv_imagebutton_class lv_imgbtn_class
#define lv_image_class lv_img_class
#define lv_msgbox_footer_button_class lv_btn_class
#define lv_msgbox_header_button_class lv_btn_class
#define lv_image_src_get_type lv_img_src_get_type
#define LV_IMAGE_SRC_SYMBOL LV_IMG_SRC_SYMBOL
static inline bool lv_obj_has_style_prop(lv_obj_t*, uint32_t, lv_style_prop_t) { return false; }
static inline void lv_tabview_set_tab_text(lv_obj_t* o, uint32_t i, const char* text) {
    auto* button=lv_tabview_get_tab_button(o,i);if(button)lv_label_set_text(lv_obj_get_child(button,0),text);
}
static inline uint32_t lv_tabview_get_tab_count(lv_obj_t* o) { return lv_obj_get_child_cnt(lv_tabview_get_content(o)); }
static inline int32_t lv_scale_get_range_min_value(lv_obj_t* o) { return ((ui_scale8*)ui_compat_find(o,3))->minimum; }
static inline int32_t lv_scale_get_range_max_value(lv_obj_t* o) { return ((ui_scale8*)ui_compat_find(o,3))->maximum; }
#define lv_ll_get_head _lv_ll_get_head
#define lv_ll_get_next _lv_ll_get_next
static inline void lv_chart_remove_cursor(lv_obj_t* object, lv_chart_cursor_t* cursor) {
    _lv_ll_remove(&((lv_chart_t*)object)->cursor_ll, cursor);lv_mem_free(cursor);lv_obj_invalidate(object);
}
#endif

#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 3
#define LV_COLOR_FORMAT_RGB565_SWAPPED ((lv_color_format_t)0x1B)
#define LV_COLOR_FORMAT_AL88 ((lv_color_format_t)0x15)
#endif
#if LVGL_VERSION_MAJOR == 8
typedef int lv_color_format_t;
enum { LV_COLOR_FORMAT_UNKNOWN=0, LV_COLOR_FORMAT_L8=6, LV_COLOR_FORMAT_I1=7, LV_COLOR_FORMAT_I2=8, LV_COLOR_FORMAT_I4=9, LV_COLOR_FORMAT_I8=10,
LV_COLOR_FORMAT_A1=11,LV_COLOR_FORMAT_A2=12,LV_COLOR_FORMAT_A4=13,LV_COLOR_FORMAT_A8=14,LV_COLOR_FORMAT_RGB888=15,LV_COLOR_FORMAT_ARGB8888=16,
LV_COLOR_FORMAT_XRGB8888=17,LV_COLOR_FORMAT_RGB565=18,LV_COLOR_FORMAT_ARGB8565=19,LV_COLOR_FORMAT_RGB565A8=20,LV_COLOR_FORMAT_AL88=21,LV_COLOR_FORMAT_RGB565_SWAPPED=27 };
#define LV_COLOR_FORMAT_IS_INDEXED(f) ((f)>=7 && (f)<=10)
#define LV_COLOR_INDEXED_PALETTE_SIZE(f) (LV_COLOR_FORMAT_IS_INDEXED(f) ? (1u << (1u << ((f)-7))) : 0u)
static inline unsigned lv_color_format_get_bpp(int f) {
    switch(f) { case 7:case 11:return 1;case 8:case 12:return 2;case 9:case 13:return 4;
    case 6:case 10:case 14:return 8;case 18:case 20:case 21:case 27:return 16;case 15:case 19:return 24;case 16:case 17:return 32;default:return 0; }
}
#endif

#if LVGL_VERSION_MAJOR == 8
#define LV_IMAGE_FLAGS_COMPRESSED 8
#endif

#if LVGL_VERSION_MAJOR == 8
#define LV_STYLE_ANIM_DURATION LV_STYLE_ANIM_TIME
#define LV_STYLE_BG_IMAGE_OPA LV_STYLE_BG_IMG_OPA
#define LV_STYLE_BG_IMAGE_RECOLOR LV_STYLE_BG_IMG_RECOLOR
#define LV_STYLE_BG_IMAGE_RECOLOR_OPA LV_STYLE_BG_IMG_RECOLOR_OPA
#define LV_STYLE_BG_IMAGE_TILED LV_STYLE_BG_IMG_TILED
#define LV_STYLE_IMAGE_OPA LV_STYLE_IMG_OPA
#define LV_STYLE_IMAGE_RECOLOR LV_STYLE_IMG_RECOLOR
#define LV_STYLE_IMAGE_RECOLOR_OPA LV_STYLE_IMG_RECOLOR_OPA
#endif

#if LVGL_VERSION_MAJOR == 8
static inline lv_obj_t* runtime_msg8_title(lv_obj_t* o) {
    auto* header=lv_msgbox_get_header(o);for(uint32_t i=0;i<lv_obj_get_child_cnt(header);++i) {
        auto* child=lv_obj_get_child(header,i);if(lv_obj_get_class(child)==&lv_label_class)return child;
    }return nullptr;
}
#define lv_msgbox_get_title runtime_msg8_title
static inline uint32_t runtime_tab8_active(lv_obj_t* o) {return ((ui_tab8*)ui_compat_find(o,1))->active;}
#undef lv_tabview_get_tab_active
#define lv_tabview_get_tab_active runtime_tab8_active
#endif
