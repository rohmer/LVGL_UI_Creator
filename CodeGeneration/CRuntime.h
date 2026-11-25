#pragma once
namespace CGeneration {
inline constexpr const char* Runtime = R"UICGEN(
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>

static bool ui_interaction_increment_i64(int64_t value, int64_t by, int64_t *result)
{ if ((by > 0 && value > INT64_MAX-by) || (by < 0 && value < INT64_MIN-by)) return false; *result=value+by; return true; }
static bool ui_interaction_validate(lv_obj_t *target, const char *text, bool required, bool has_min, double minimum, bool has_max, double maximum)
{
    const char *first=text; char *end=NULL; double number; bool valid;
    if (!target || !text) return false;
    while (*first && isspace((unsigned char)*first)) ++first;
    valid=!required || *first;
    if (valid && *first && (has_min || has_max)) {
        number=strtod(first,&end); while (end && *end && isspace((unsigned char)*end)) ++end;
        valid=end!=first && end && !*end && isfinite(number) && (!has_min || number>=minimum) && (!has_max || number<=maximum);
    }
    lv_obj_set_style_border_color(target,lv_color_hex(0xd92d20),(lv_style_selector_t)LV_PART_MAIN|LV_STATE_USER_4);
    lv_obj_set_style_border_width(target,2,(lv_style_selector_t)LV_PART_MAIN|LV_STATE_USER_4);
    if (valid) lv_obj_remove_state(target,LV_STATE_USER_4);
    else {lv_obj_add_state(target,LV_STATE_USER_4);lv_obj_scroll_to_view_recursive(target,LV_ANIM_OFF);}
    return valid;
}

#if LV_USE_CALENDAR
static lv_obj_t* ui_calendar_header(lv_obj_t* calendar){lv_obj_t* matrix=lv_calendar_get_btnmatrix(calendar);uint32_t i;for(i=0;i<lv_obj_get_child_count(calendar);++i){lv_obj_t* child=lv_obj_get_child(calendar,i);if(child!=matrix)return child;}return NULL;}
static lv_obj_t* ui_calendar_month_label(lv_obj_t* calendar){lv_obj_t* header=ui_calendar_header(calendar);uint32_t i;if(!header)return NULL;for(i=0;i<lv_obj_get_child_count(header);++i){lv_obj_t* child=lv_obj_get_child(header,i);if(lv_obj_has_class(child,&lv_label_class))return child;}return NULL;}
#endif

static void ui_style_num(lv_obj_t* obj, lv_style_prop_t property, int32_t number, lv_style_selector_t selector)
{ lv_style_value_t value; value.num = number; lv_obj_set_local_style_prop(obj, property, value, selector); }
static void ui_style_color(lv_obj_t* obj, lv_style_prop_t property, uint32_t color, lv_style_selector_t selector)
{ lv_style_value_t value; value.color = lv_color_hex(color); lv_obj_set_local_style_prop(obj, property, value, selector); }
static void* ui_reserve(void* storage, size_t* capacity, size_t count, size_t maximum, size_t size)
{
    size_t wanted; void* next;
    if (count < *capacity || (maximum && count >= maximum)) return storage;
    wanted = *capacity ? *capacity * 2 : 4;
    if (wanted < *capacity) return NULL;
    if (maximum && wanted > maximum) wanted = maximum;
    if (wanted > SIZE_MAX / size) return NULL;
    next = lv_realloc(storage, wanted * size);
    if (!next) return NULL;
    *capacity = wanted; return next;
}
static void ui_forget_object(lv_event_t* event)
{ lv_obj_t **slot=(lv_obj_t**)lv_event_get_user_data(event); if (*slot == lv_event_get_target_obj(event)) *slot = NULL; }
static void ui_track_object(lv_obj_t* obj, lv_obj_t** slot)
{ *slot = obj; lv_obj_add_event_cb(obj, ui_forget_object, LV_EVENT_DELETE, slot); }

typedef struct {
    lv_obj_t* object;
    const lv_image_dsc_t* const* frames;
    const uint32_t* delays;
    size_t count, index;
    lv_timer_t* timer;
    int target, state, side;
    lv_style_selector_t selector;
} ui_animation_t;
static void ui_animation_apply(ui_animation_t* animation)
{
    const void* source = animation->frames[animation->index];
    if (!animation->object) return;
    switch (animation->target) {
#if LV_USE_IMAGE
    case 0: lv_image_set_src(animation->object, source); break;
#endif
    case 1: lv_obj_set_style_bg_image_src(animation->object, source, animation->selector); break;
    case 2: lv_obj_set_style_arc_image_src(animation->object, source, animation->selector); break;
    case 3: lv_obj_set_style_bitmap_mask_src(animation->object, source, animation->selector); break;
#if LV_USE_IMAGEBUTTON
    case 4: {
        const void* left = lv_imagebutton_get_src_left(animation->object, (lv_imagebutton_state_t)animation->state);
        const void* middle = lv_imagebutton_get_src_middle(animation->object, (lv_imagebutton_state_t)animation->state);
        const void* right = lv_imagebutton_get_src_right(animation->object, (lv_imagebutton_state_t)animation->state);
        if (animation->side == 0) left = source; else if (animation->side == 1) middle = source; else right = source;
        lv_imagebutton_set_src(animation->object, (lv_imagebutton_state_t)animation->state, left, middle, right);
        break;
    }
#endif
    }
}
static void ui_animation_tick(lv_timer_t* timer)
{
    ui_animation_t* animation = (ui_animation_t*)lv_timer_get_user_data(timer);
    animation->index = (animation->index + 1) % animation->count;
    ui_animation_apply(animation);
    lv_timer_set_period(timer, animation->delays[animation->index]);
}
static void ui_animation_deleted(lv_event_t* event)
{
    ui_animation_t* animation = (ui_animation_t*)lv_event_get_user_data(event);
    if (animation->timer) lv_timer_delete(animation->timer);
    animation->timer = NULL; animation->object = NULL;
}
static void ui_animation_start(ui_animation_t* animation, lv_obj_t* object)
{
    animation->object = object; animation->index = 0;
    ui_animation_apply(animation);
    if (animation->count > 1) animation->timer = lv_timer_create(ui_animation_tick, animation->delays[0], animation);
    lv_obj_add_event_cb(object, ui_animation_deleted, LV_EVENT_DELETE, animation);
}

#if LV_USE_TEXTAREA && LV_USE_BUTTON && LV_USE_LABEL
typedef struct {
    lv_obj_t* root; lv_obj_t* entry;
    int64_t value, minimum, maximum, step;
    bool numeric, read_only, updating;
} ui_textbox_t;
static void ui_textbox_read_value(ui_textbox_t* box)
{
    if (box->numeric) {
        char* end; const char* text = lv_textarea_get_text(box->entry);
        int64_t value = strtoll(text, &end, 10);
        if (*text && !*end) box->value = value < box->minimum ? box->minimum : value > box->maximum ? box->maximum : value;
    }
}
static void ui_textbox_assign(ui_textbox_t* box, int64_t value)
{
    char text[32];
    if (!box->root) return;
    box->value = value < box->minimum ? box->minimum : value > box->maximum ? box->maximum : value;
    snprintf(text, sizeof text, "%" PRId64, box->value);
    box->updating = true; lv_textarea_set_text(box->entry, text); box->updating = false;
}
static void ui_textbox_entry(lv_event_t* event)
{
    ui_textbox_t* box = (ui_textbox_t*)lv_event_get_user_data(event);
    if (box->updating || box->read_only) return;
    ui_textbox_read_value(box);
    lv_obj_send_event(box->root, LV_EVENT_VALUE_CHANGED, NULL);
}
static void ui_textbox_plus(lv_event_t* event)
{
    ui_textbox_t* box = (ui_textbox_t*)lv_event_get_user_data(event);
    if (!box->numeric || box->read_only) return;
    ui_textbox_assign(box, box->value > box->maximum - box->step ? box->maximum : box->value + box->step);
    lv_obj_send_event(box->root, LV_EVENT_VALUE_CHANGED, NULL);
}
static void ui_textbox_minus(lv_event_t* event)
{
    ui_textbox_t* box = (ui_textbox_t*)lv_event_get_user_data(event);
    if (!box->numeric || box->read_only) return;
    ui_textbox_assign(box, box->value < box->minimum + box->step ? box->minimum : box->value - box->step);
    lv_obj_send_event(box->root, LV_EVENT_VALUE_CHANGED, NULL);
}
typedef struct ui_layer_t ui_layer_t;
typedef struct { ui_layer_t* owner; lv_obj_t* button; lv_obj_t* toggle; lv_obj_t* caption;
    uint32_t id, parent; bool expanded, disabled; } ui_layer_row_t;
struct ui_layer_t { lv_obj_t* root; lv_obj_t* content; ui_layer_row_t* rows; size_t count; uint32_t selected; };
static void ui_layer_refresh(ui_layer_t* layer)
{
    size_t i, j, remaining;
    for (i = 0; i < layer->count; ++i) {
        bool visible = true; uint32_t parent = layer->rows[i].parent;
        remaining = layer->count;
        while (parent && remaining--) {
            for (j = 0; j < layer->count && layer->rows[j].id != parent; ++j) {}
            if (j == layer->count) { visible = false; break; }
            if (!layer->rows[j].expanded) visible = false;
            parent = layer->rows[j].parent;
        }
        lv_obj_set_hidden(layer->rows[i].button, !visible);
        lv_obj_set_state(layer->rows[i].button, LV_STATE_DISABLED, layer->rows[i].disabled);
        lv_obj_set_state(layer->rows[i].button, LV_STATE_CHECKED, layer->rows[i].id == layer->selected);
    }
}
static void ui_layer_select(lv_event_t* event)
{
    ui_layer_row_t* row = (ui_layer_row_t*)lv_event_get_user_data(event);
    if (row->disabled) return;
    row->owner->selected = row->id; ui_layer_refresh(row->owner);
    lv_obj_send_event(row->owner->root, LV_EVENT_VALUE_CHANGED, NULL);
}
static void ui_layer_toggle(lv_event_t* event)
{
    ui_layer_row_t* row = (ui_layer_row_t*)lv_event_get_user_data(event);
    if (row->disabled) return;
    row->expanded = !row->expanded;
    lv_label_set_text(lv_obj_get_child(row->toggle, 0), row->expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
    ui_layer_refresh(row->owner);
}
#endif
)UICGEN";
}
