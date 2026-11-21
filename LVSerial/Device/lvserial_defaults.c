#include "lvserial_defaults.h"
#define N(key,value) {key,lvserial_ValueKind_SIGNED,value,NULL}
#define B(key,value) {key,lvserial_ValueKind_BOOLEAN,value,NULL}
#define T(key,value) {key,lvserial_ValueKind_TEXT,0,value}
#define A(key) {key,lvserial_ValueKind_ARRAY,0,NULL}
#define O(key) {key,lvserial_ValueKind_OBJECT,0,NULL}
static const lvserial_default base[] = {
    T("name",""),B("click",1),B("hidden",0),B("parevent",0),B("checked",0),B("disabled",0),
    N("opascale",255),B("opascaleen",0),B("drag",0),B("dragThrow",0),B("dragParent",0),B("top",0),
    N("dragDir",3),B("protNone",1),B("protChildChange",0),B("protParent",0),B("protPos",0),
    B("protFollow",0),B("protPressLost",0),B("protClickFocus",0),N("x",0),N("y",0),A("nativeStyles")
};
static const lvserial_default flags[] = {
    B("hidden",0),B("clickable",0),B("click_focusable",1),B("checkable",0),B("scrollable",1),
    B("scroll_elastic",1),B("scroll_momentum",1),B("scroll_one",0),B("scroll_chain_hor",1),
    B("scroll_chain_ver",1),B("scroll_on_focus",1),B("scroll_with_arrow",0),B("snappable",1),
    B("press_lock",0),B("event_bubble",0),B("gesture_bubble",1),B("adv_hittest",0),B("ignore_layout",0),
    B("floating",0),B("send_draw_task_events",0),B("overflow_visible",0),B("event_trickle",0),
    B("state_trickle",0),B("radio_button",0),B("flex_in_new_track",0),B("checked",0),B("disabled",0),
    B("state_user_1",0),B("state_user_2",0),B("state_user_3",0),B("state_user_4",0)
};
static const lvserial_default scroll[] = {
    B("enabled",1),N("direction",15),N("scrollbar",1),N("snapX",0),N("snapY",0),N("x",0),N("y",0),
    B("elastic",1),B("momentum",1),B("chainHorizontal",1),B("chainVertical",1)
};
static const lvserial_default native_style[] = {
    N("pad_top",0),N("pad_bottom",0),N("pad_left",0),N("pad_right",0),N("pad_row",0),N("pad_column",0),
    N("border_width",0),N("shadow_width",0),N("radius",0),N("bg_opa",255),N("opa",255),N("text_opa",255),
    N("text_letter_space",0),N("text_line_space",0),N("image_opa",255),N("image_recolor_opa",0),
    N("bg_grad_dir",0),N("bg_main_stop",0),N("bg_grad_stop",255),N("bg_main_opa",255),N("bg_grad_opa",255),
    N("outline_width",0),N("translate_x",0),N("translate_y",0),N("transform_rotation",0),N("transform_scale_x",256),
    N("transform_scale_y",256),N("anim_duration",0),N("layout",0),N("flex_grow",0),T("text_font","default"),
    N("align",0),N("x",0),N("y",0),N("arc_opa",255),N("line_opa",255),N("border_opa",255),N("outline_opa",255),
    N("shadow_opa",255),N("blend_mode",0),N("text_align",0),N("text_decor",0),N("bg_image_opa",255),
    N("bg_image_recolor_opa",0),N("arc_rounded",0),N("line_rounded",0),N("clip_corner",0)
};
static const lvserial_default basic[] = {
    N("min",0),N("max",100),N("mode",0),N("orientation",0),N("value",0),N("startValue",0),N("leftValue",0),
    N("rotation",0),N("knobOffset",0),N("changeRate",720),N("animT",0),B("sym",0),B("toggle",0),N("layout",0),
    N("left",0),N("right",0),N("top",0),N("bottom",0),N("in",0),N("out",0),N("wait",0),B("oneToggle",0),
    B("recolor",0),N("selected",0),N("longmode",0),N("textalign",0),B("drawback",0),B("checked",0),B("inact",0),
    N("offX",0),N("offY",0),N("scaleX",256),N("scaleY",256),B("antialias",1),N("blendMode",0),N("innerAlign",0),
    N("srcType",0),N("step",1),N("digits",5),N("decimalPlaces",0),B("rollover",0),B("invertY",0),N("brightness",255),
    N("duration",1000),N("sweep",60),N("visibleRows",3),B("highlight",1),T("text",""),T("placeholder",""),
    B("oneLine",0),B("password",0),T("passwordBullet","*"),N("passwordShowTime",1500),N("maxLength",0),N("cursor",0),
    B("cursorClick",1),B("selection",0),A("points"),N("value",-32768)
};
static const lvserial_default advanced[] = {
    N("active",0),N("barSize",40),N("headerHeight",40),T("title",""),A("children"),A("header"),A("headerButtons"),
    A("footer"),A("tabs"),A("tiles"),N("ticks",11),N("majorEvery",5),N("rotation",0),N("angleRange",270),
    B("showLabels",1),N("mode",0),B("popovers",0),N("type",1),N("pointCount",10),N("updateMode",0),
    N("horizontalDivisions",3),N("verticalDivisions",5),A("series"),A("cursors"),N("start",0),B("hidden",0),
    N("axis",0),N("flags",0),A("hlDates"),N("selectedRow",65535),N("selectedColumn",65535),N("column",0),
    N("row",0),N("scrollbarMode",1),N("needleLength",50),N("needleWidth",3),B("positionSet",1),N("point",0)
};
static const lvserial_default custom[] = {
    T("title","Value"),T("text","0"),N("value",0),N("min",0),N("max",100),N("step",1),B("numeric",1),
    B("readOnly",0),B("password",0),T("placeholder",""),N("maxLength",32),T("acceptedChars",""),
    B("showButtons",1),N("titleWidth",64),N("buttonWidth",28),N("spacing",6),A("children"),N("indent",18),
    N("rowHeight",32),N("selected",1),B("showTitle",1),N("parent",0),B("expanded",1),B("disabled",0)
};
static const lvserial_default variable[] = {
    T("name",""),N("dataType",1),N("history",1),N("oid",-1),T("property",""),N("default",0),A("value"),
    B("default",0),T("default",""),{"default",lvserial_ValueKind_REAL,0,NULL}
};
static const lvserial_default legacy_style[] = {
    N("opa",255),N("radius",0),N("width",0),N("left",0),N("right",0),N("top",0),N("bottom",0),N("inner",0),
    N("letter_space",0),N("line_space",0),N("intense",0),B("rounded",0),N("line.width",1),N("line.opa",255),
    B("line.rounded",0),O("style"),O("styles"),O("fit"),O("ink")
};
static const lvserial_default font[] = {
    N("bitmapFmt",0),N("kernScale",0),N("kernClasses",0),N("bitmapIdx",0),N("adv_w",0),N("box_w",0),N("box_h",0),
    N("ofs_x",0),N("ofs_y",0),A("unicodeList"),A("glyphIDOffsetList"),N("glyphIDSize",0)
};
typedef struct { const lvserial_default* values; size_t count; } profile;
#define PROFILE(name) {name,sizeof(name)/sizeof(name[0])}
static const profile profiles[] = {
    PROFILE(base),PROFILE(flags),PROFILE(scroll),PROFILE(native_style),PROFILE(basic),PROFILE(advanced),
    PROFILE(custom),PROFILE(variable),PROFILE(legacy_style),PROFILE(font)
};
size_t lvserial_defaults_profile_count(void) { return sizeof(profiles)/sizeof(profiles[0]); }
const lvserial_default* lvserial_defaults_profile(uint32_t id, size_t* count) {
    if (!id || id > lvserial_defaults_profile_count()) { *count=0; return NULL; }
    *count=profiles[id-1].count; return profiles[id-1].values;
}
