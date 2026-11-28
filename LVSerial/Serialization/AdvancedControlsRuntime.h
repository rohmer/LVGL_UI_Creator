/* Native, theme-aware advanced controls shared by the loader and C exports.
 * Define UI_ADV_USE_* to 1 for the controls required by this application.
 * This header deliberately depends only on the public LVGL 9 C API. */
#ifndef UI_ADVANCED_CONTROLS_RUNTIME_H
#define UI_ADVANCED_CONTROLS_RUNTIME_H
#include <lvgl/lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef UI_ADV_USE_RADIO_GROUP
#define UI_ADV_USE_RADIO_GROUP 0
#endif
#ifndef UI_ADV_USE_CHECKBOX_BUTTON
#define UI_ADV_USE_CHECKBOX_BUTTON 0
#endif
#ifndef UI_ADV_USE_LABELED_CHECKBOX
#define UI_ADV_USE_LABELED_CHECKBOX 0
#endif
#ifndef UI_ADV_USE_DATE_PICKER
#define UI_ADV_USE_DATE_PICKER 0
#endif
#ifndef UI_ADV_USE_MENU
#define UI_ADV_USE_MENU 0
#endif
#ifdef __cplusplus
#define UI_ADV_INLINE inline
#else
#define UI_ADV_INLINE static inline
#endif

#if UI_ADV_USE_RADIO_GROUP || UI_ADV_USE_CHECKBOX_BUTTON || UI_ADV_USE_LABELED_CHECKBOX || UI_ADV_USE_DATE_PICKER || UI_ADV_USE_MENU
typedef struct ui_adv_state_t ui_adv_state_t;
typedef struct ui_adv_node_t {
    struct ui_adv_node_t *next;
    ui_adv_state_t *owner;
    lv_obj_t *object;
    uint32_t id, parent;
    bool disabled;
} ui_adv_node_t;
struct ui_adv_state_t {
    lv_obj_t *root, *title, *label, *description, *content, *back, *popup, *calendar;
    ui_adv_node_t *nodes, *tail;
    int32_t selected;
    uint32_t page;
    uint8_t kind, first_day;
    bool checked, updating, interactive;
    char date[11], minimum[11], maximum[11], format[11];
    char *caption, *placeholder;
    void *date_grid;
};
enum { UI_ADV_RADIO=1, UI_ADV_CHECKBOX=2, UI_ADV_LABELED=3, UI_ADV_DATE=4, UI_ADV_MENU=5 };

UI_ADV_INLINE char *ui_adv_copy(const char *text)
{
    size_t size = strlen(text ? text : "") + 1;
    char *result = (char *)lv_malloc(size);
    if (result) memcpy(result, text ? text : "", size);
    return result;
}
UI_ADV_INLINE void ui_adv_delete(lv_event_t *event)
{
    ui_adv_state_t *state = (ui_adv_state_t *)lv_event_get_user_data(event);
    ui_adv_node_t *node = state->nodes;
    if (state->popup) { lv_obj_delete(state->popup); state->popup = NULL; }
    while (node) { ui_adv_node_t *next = node->next; lv_free(node); node = next; }
    lv_free(state->caption);
    lv_free(state->placeholder);
    lv_free(state->date_grid);
    lv_free(state);
}
UI_ADV_INLINE ui_adv_state_t *ui_adv_state(lv_obj_t *root)
{
    uint32_t index;
    if (!root) return NULL;
    for (index=0; index<lv_obj_get_event_count(root); ++index) {
        lv_event_dsc_t *descriptor = lv_obj_get_event_dsc(root, index);
        if (lv_event_dsc_get_cb(descriptor) == ui_adv_delete)
            return (ui_adv_state_t *)lv_event_dsc_get_user_data(descriptor);
    }
    return NULL;
}
UI_ADV_INLINE lv_obj_t *ui_adv_button(lv_obj_t *parent, const char *text)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);
    lv_obj_set_scrollable(button, false);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_height(button, 38);
    lv_label_set_text(label, text ? text : "");
    lv_obj_center(label);
    return button;
}
UI_ADV_INLINE lv_obj_t *ui_adv_create(lv_obj_t *parent, uint8_t kind)
{
    ui_adv_state_t *state = (ui_adv_state_t *)lv_malloc(sizeof(ui_adv_state_t));
    lv_obj_t *root;
    if (!state) return NULL;
    memset(state, 0, sizeof(*state));
    root = lv_obj_create(parent);
    if (!root) { lv_free(state); return NULL; }
    state->root=root; state->kind=kind; state->selected=-1; state->interactive=true;
    lv_obj_add_event_cb(root, ui_adv_delete, LV_EVENT_DELETE, state);
    lv_obj_set_style_pad_all(root, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(root, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(root, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    return root;
}
UI_ADV_INLINE ui_adv_node_t *ui_adv_node(ui_adv_state_t *state, uint32_t id)
{
    ui_adv_node_t *node;
    for (node=state->nodes; node; node=node->next) if (node->id==id) return node;
    return NULL;
}
UI_ADV_INLINE void ui_adv_set_interactive(lv_obj_t *root, bool interactive)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    if (!state) return;
    state->interactive=interactive;
    if (state->kind==UI_ADV_RADIO)
        for (node=state->nodes;node;node=node->next) lv_obj_set_checkable(node->object,interactive);
    if ((state->kind==UI_ADV_CHECKBOX || state->kind==UI_ADV_LABELED) && state->label)
        lv_obj_set_checkable(state->label,interactive);
    if (!interactive && state->popup) lv_obj_set_hidden(state->popup,true);
}
UI_ADV_INLINE ui_adv_node_t *ui_adv_append(ui_adv_state_t *state, uint32_t id, uint32_t parent, bool disabled)
{
    ui_adv_node_t *node = (ui_adv_node_t *)lv_malloc(sizeof(ui_adv_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(*node));
    node->id=id; node->parent=parent; node->disabled=disabled; node->owner=state;
    if (state->tail) state->tail->next=node;
    else state->nodes=node;
    state->tail=node;
    return node;
}
UI_ADV_INLINE void ui_adv_clear(lv_obj_t *root)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    if (!state) return;
    node=state->nodes;
    state->nodes=NULL; state->tail=NULL;
    while (node) {
        ui_adv_node_t *next=node->next;
        if (node->object) lv_obj_delete(node->object);
        lv_free(node); node=next;
    }
    state->selected=-1; state->page=0;
}
UI_ADV_INLINE lv_obj_t *ui_adv_part(lv_obj_t *root, const char *name)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    unsigned id;
    if (!state || !name) return NULL;
    if (!strcmp(name,"Title")) return state->title;
    if (!strcmp(name,"Label") || !strcmp(name,"Checkbox") || !strcmp(name,"Display")) return state->label;
    if (!strcmp(name,"Description")) return state->description;
    if (!strcmp(name,"Content") || !strcmp(name,"Menu")) return state->content;
    if (!strcmp(name,"Back")) return state->back;
    if (!strcmp(name,"Popup")) return state->popup;
    if (!strcmp(name,"Calendar")) return state->calendar;
    if (sscanf(name,"Option%u",&id)==1 || sscanf(name,"Item%u",&id)==1) {
        node=ui_adv_node(state,(uint32_t)id);
        return node ? node->object : NULL;
    }
    return NULL;
}
#endif

#if UI_ADV_USE_RADIO_GROUP
UI_ADV_INLINE bool ui_adv_radio_set_selected(lv_obj_t *root, int32_t selected)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    if (!state || selected< -1) return false;
    node=selected>=0 ? ui_adv_node(state,(uint32_t)selected) : NULL;
    if (selected>=0 && (!node || node->disabled)) return false;
    state->selected=selected;
    for (node=state->nodes; node; node=node->next)
        lv_obj_set_state(node->object, LV_STATE_CHECKED, selected==(int32_t)node->id);
    return true;
}
UI_ADV_INLINE int32_t ui_adv_radio_get_selected(lv_obj_t *root)
{ ui_adv_state_t *state=ui_adv_state(root); return state ? state->selected : -1; }
UI_ADV_INLINE void ui_adv_radio_event(lv_event_t *event)
{
    ui_adv_node_t *node=(ui_adv_node_t *)lv_event_get_user_data(event);
    if (node->disabled || !node->owner->interactive) return;
    if (ui_adv_radio_set_selected(node->owner->root,(int32_t)node->id))
        lv_obj_send_event(node->owner->root, LV_EVENT_VALUE_CHANGED, NULL);
}
UI_ADV_INLINE lv_obj_t *ui_adv_radio_create(lv_obj_t *parent, const char *title, bool horizontal)
{
    lv_obj_t *root=ui_adv_create(parent,UI_ADV_RADIO);
    ui_adv_state_t *state=ui_adv_state(root);
    if (!root) return NULL;
    lv_obj_set_size(root,300,150);
    state->title=lv_label_create(root);
    lv_label_set_text(state->title,title ? title : "");
    state->content=lv_obj_create(root);
    lv_obj_remove_style_all(state->content);
    lv_obj_set_size(state->content,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(state->content,horizontal ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(state->content,6,LV_PART_MAIN);
    lv_obj_set_style_pad_column(state->content,6,LV_PART_MAIN);
    return root;
}
UI_ADV_INLINE bool ui_adv_radio_add(lv_obj_t *root, const char *text, bool disabled)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    uint32_t id;
    if (!state) return false;
    id=state->tail ? state->tail->id+1 : 0;
    node=ui_adv_append(state,id,0,disabled);
    if (!node) return false;
    node->object=ui_adv_button(state->content,text);
    lv_obj_set_width(node->object,LV_SIZE_CONTENT);
    lv_obj_set_checkable(node->object,state->interactive);
    lv_obj_set_style_radius(node->object,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_state(node->object,LV_STATE_DISABLED,disabled);
    lv_obj_add_event_cb(node->object,ui_adv_radio_event,LV_EVENT_CLICKED,node);
    return true;
}
#endif

#if UI_ADV_USE_CHECKBOX_BUTTON || UI_ADV_USE_LABELED_CHECKBOX
UI_ADV_INLINE void ui_adv_checkbox_set_checked(lv_obj_t *root, bool checked)
{
    ui_adv_state_t *state=ui_adv_state(root);
    if (!state) return;
    state->checked=checked;
    lv_obj_set_state(root,LV_STATE_CHECKED,checked);
    lv_obj_set_state(state->label,LV_STATE_CHECKED,checked);
}
UI_ADV_INLINE bool ui_adv_checkbox_get_checked(lv_obj_t *root)
{ ui_adv_state_t *state=ui_adv_state(root); return state ? state->checked : false; }
UI_ADV_INLINE void ui_adv_checkbox_event(lv_event_t *event)
{
    ui_adv_state_t *state=(ui_adv_state_t *)lv_event_get_user_data(event);
    lv_obj_t *target=lv_event_get_target_obj(event);
    if (!state->interactive || lv_obj_has_state(state->root,LV_STATE_DISABLED)) return;
    if (target==state->label) ui_adv_checkbox_set_checked(state->root,lv_obj_has_state(state->label,LV_STATE_CHECKED));
    else ui_adv_checkbox_set_checked(state->root,!state->checked);
    lv_obj_send_event(state->root,LV_EVENT_VALUE_CHANGED,NULL);
}
UI_ADV_INLINE lv_obj_t *ui_adv_checkbox_create(lv_obj_t *parent, const char *text, const char *description, bool nested)
{
    lv_obj_t *root=ui_adv_create(parent,nested ? UI_ADV_LABELED : UI_ADV_CHECKBOX);
    ui_adv_state_t *state=ui_adv_state(root);
    if (!root) return NULL;
    lv_obj_set_size(root,nested ? 260 : 170,nested ? 96 : 44);
    lv_obj_set_scrollable(root,false);
    lv_obj_set_clickable(root,true);
    lv_obj_set_style_border_color(root,lv_theme_get_color_primary(root),(lv_style_selector_t)LV_PART_MAIN | LV_STATE_CHECKED);
    if (!nested) {
        lv_obj_set_style_bg_color(root,lv_theme_get_color_primary(root),(lv_style_selector_t)LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(root,LV_OPA_20,(lv_style_selector_t)LV_PART_MAIN | LV_STATE_CHECKED);
    }
    state->label=lv_checkbox_create(root);
    lv_checkbox_set_text(state->label,text ? text : "");
    lv_obj_set_width(state->label,LV_PCT(100));
    lv_obj_add_event_cb(state->label,ui_adv_checkbox_event,LV_EVENT_VALUE_CHANGED,state);
    if (nested) {
        state->description=lv_label_create(root);
        lv_obj_set_width(state->description,LV_PCT(100));
        lv_label_set_long_mode(state->description,LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(state->description,description ? description : "");
        lv_obj_set_clickable(state->description,false);
        lv_obj_set_style_text_opa(state->description,LV_OPA_70,LV_PART_MAIN);
    }
    lv_obj_add_event_cb(root,ui_adv_checkbox_event,LV_EVENT_CLICKED,state);
    return root;
}
#endif

#if UI_ADV_USE_DATE_PICKER
typedef struct {
    const char *map[56];
    char numbers[42][3];
} ui_adv_date_grid_t;
UI_ADV_INLINE bool ui_adv_date_parse(const char *text, lv_calendar_date_t *date)
{
    unsigned year,month,day;
    unsigned days;
    size_t index;
    static const uint8_t month_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    if (!text || strlen(text)!=10 || text[4]!='-' || text[7]!='-') return false;
    for (index=0; index<10; ++index) if (index!=4 && index!=7 && (text[index]<'0' || text[index]>'9')) return false;
    if (sscanf(text,"%4u-%2u-%2u",&year,&month,&day)!=3 || year<1 || year>9999 || month<1 || month>12) return false;
    days=month_days[month-1];
    if (month==2 && year%4==0 && (year%100!=0 || year%400==0)) ++days;
    if (day<1 || day>days) return false;
    if (date) { date->year=(uint16_t)year; date->month=(uint8_t)month; date->day=(uint8_t)day; }
    return true;
}
UI_ADV_INLINE bool ui_adv_date_set_date(lv_obj_t *root, const char *iso)
{
    ui_adv_state_t *state=ui_adv_state(root);
    lv_calendar_date_t date;
    char display[16];
    if (!state || !iso) return false;
    if (*iso && (!ui_adv_date_parse(iso,&date) || (*state->minimum && strcmp(iso,state->minimum)<0) || (*state->maximum && strcmp(iso,state->maximum)>0))) return false;
    memcpy(state->date,iso,strlen(iso)+1);
    if (!*iso) lv_label_set_text(lv_obj_get_child(state->label,0),state->placeholder ? state->placeholder : "Select date");
    else {
        if (!strcmp(state->format,"dd/mm/yyyy")) (void)snprintf(display,sizeof(display),"%02u/%02u/%04u",(unsigned)date.day,(unsigned)date.month,(unsigned)date.year);
        else if (!strcmp(state->format,"mm/dd/yyyy")) (void)snprintf(display,sizeof(display),"%02u/%02u/%04u",(unsigned)date.month,(unsigned)date.day,(unsigned)date.year);
        else memcpy(display,iso,11);
        lv_label_set_text(lv_obj_get_child(state->label,0),display);
    }
    return true;
}
UI_ADV_INLINE const char *ui_adv_date_get_date(lv_obj_t *root)
{ ui_adv_state_t *state=ui_adv_state(root); return state ? state->date : ""; }
UI_ADV_INLINE void ui_adv_date_close(lv_obj_t *root)
{
    ui_adv_state_t *state=ui_adv_state(root);
    if (state && state->popup) {
        lv_group_t *group=lv_obj_get_group(state->label);
        lv_obj_t *focused=group ? lv_group_get_focused(group) : NULL;
        bool restore=false;
        while (focused) {
            if (focused==state->popup) { restore=true; break; }
            focused=lv_obj_get_parent(focused);
        }
        lv_obj_set_hidden(state->popup,true);
        if (restore) lv_group_focus_obj(state->label);
    }
}
/* LVGL's built-in week start is a build option. Replace only its public button
 * map, retaining its public calendar/navigation/pressed-date APIs. Each popup
 * owns this small map, so different pickers can use different first weekdays. */
UI_ADV_INLINE void ui_adv_date_refresh(ui_adv_state_t *state)
{
    const lv_calendar_date_t *shown;
    ui_adv_date_grid_t *grid=(ui_adv_date_grid_t *)state->date_grid;
    lv_obj_t *matrix;
    uint32_t year,month,weekday,offset,day,index,column,row,count;
    static const char *days[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const uint8_t lengths[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    char iso[16];
    if (!state->calendar || !grid) return;
    shown=lv_calendar_get_showed_date(state->calendar);
    year=shown->year; month=shown->month;
    if (year<1 || year>9999 || month<1 || month>12) return;
    {
        uint32_t adjustment=month<3 ? 1u : 0u;
        uint32_t previous=year-adjustment;
        weekday=(1u+31u*(month-2u+12u*adjustment)/12u+previous+previous/4u-previous/100u+previous/400u)%7u;
    }
    offset=(weekday+7u-state->first_day)%7u;
    count=lengths[month-1];
    if (month==2 && year%4==0 && (year%100!=0 || year%400==0)) ++count;
    index=0;
    for (column=0;column<7;++column) grid->map[index++]=days[(column+state->first_day)%7];
    grid->map[index++]="\n";
    for (row=0;row<6;++row) {
        for (column=0;column<7;++column) {
            uint32_t cell=row*7+column;
            day=cell>=offset ? cell-offset+1 : 0;
            if (day && day<=count) (void)snprintf(grid->numbers[cell],3,"%u",(unsigned)day);
            else { grid->numbers[cell][0]=' '; grid->numbers[cell][1]='\0'; }
            grid->map[index++]=grid->numbers[cell];
        }
        grid->map[index++]=row<5 ? "\n" : "";
    }
    matrix=lv_calendar_get_btnmatrix(state->calendar);
    lv_buttonmatrix_set_map(matrix,grid->map);
    lv_buttonmatrix_clear_button_ctrl_all(matrix,(lv_buttonmatrix_ctrl_t)(LV_BUTTONMATRIX_CTRL_DISABLED | LV_BUTTONMATRIX_CTRL_CHECKED | LV_BUTTONMATRIX_CTRL_CUSTOM_1 | LV_BUTTONMATRIX_CTRL_CUSTOM_2));
    for (index=0;index<49;++index) {
        bool disabled=index<7;
        if (index>=7) {
            uint32_t cell=index-7;
            day=cell>=offset ? cell-offset+1 : 0;
            if (!day || day>count) disabled=true;
            else {
                (void)snprintf(iso,sizeof(iso),"%04u-%02u-%02u",(unsigned)year,(unsigned)month,(unsigned)day);
                if ((*state->minimum && strcmp(iso,state->minimum)<0) || (*state->maximum && strcmp(iso,state->maximum)>0)) disabled=true;
                if (!strcmp(iso,state->date)) lv_buttonmatrix_set_button_ctrl(matrix,index,LV_BUTTONMATRIX_CTRL_CHECKED);
            }
        }
        if (disabled) lv_buttonmatrix_set_button_ctrl(matrix,index,LV_BUTTONMATRIX_CTRL_DISABLED);
    }
}
UI_ADV_INLINE void ui_adv_date_month(lv_event_t *event)
{ ui_adv_date_refresh((ui_adv_state_t *)lv_event_get_user_data(event)); }
UI_ADV_INLINE void ui_adv_date_event(lv_event_t *event)
{
    ui_adv_state_t *state=(ui_adv_state_t *)lv_event_get_user_data(event);
    lv_event_code_t code=lv_event_get_code(event);
    if (code==LV_EVENT_DELETE) { state->popup=NULL; state->calendar=NULL; return; }
    if (!state->interactive) return;
    if (code==LV_EVENT_KEY && lv_event_get_key(event)==LV_KEY_ESC) { ui_adv_date_close(state->root); return; }
    if (code==LV_EVENT_CLICKED) {
        if (lv_event_get_target_obj(event)==state->popup) ui_adv_date_close(state->root);
        return;
    }
    if (code==LV_EVENT_VALUE_CHANGED) {
        lv_calendar_date_t date;
        char iso[16];
        if (lv_calendar_get_pressed_date(state->calendar,&date)!=LV_RESULT_OK) return;
        (void)snprintf(iso,sizeof(iso),"%04u-%02u-%02u",(unsigned)date.year,(unsigned)date.month,(unsigned)date.day);
        if (ui_adv_date_set_date(state->root,iso)) {
            ui_adv_date_close(state->root);
            lv_obj_send_event(state->root,LV_EVENT_VALUE_CHANGED,NULL);
        }
    }
}
UI_ADV_INLINE void ui_adv_date_open(lv_obj_t *root)
{
    ui_adv_state_t *state=ui_adv_state(root);
    lv_calendar_date_t date;
    lv_obj_t *screen;
    int32_t width,height;
    if (!state || !state->interactive || lv_obj_has_state(root,LV_STATE_DISABLED)) return;
    screen=lv_obj_get_screen(root);
    if (!state->popup) {
        if (!state->date_grid) state->date_grid=lv_malloc(sizeof(ui_adv_date_grid_t));
        if (!state->date_grid) return;
        state->popup=lv_obj_create(screen);
        lv_obj_remove_style_all(state->popup);
        lv_obj_set_size(state->popup,LV_PCT(100),LV_PCT(100));
        lv_obj_set_clickable(state->popup,true);
        lv_obj_set_floating(state->popup,true);
        lv_obj_set_scrollable(state->popup,false);
        lv_obj_set_style_bg_color(state->popup,lv_color_black(),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(state->popup,LV_OPA_30,LV_PART_MAIN);
        lv_obj_add_event_cb(state->popup,ui_adv_date_event,LV_EVENT_CLICKED,state);
        lv_obj_add_event_cb(state->popup,ui_adv_date_event,LV_EVENT_KEY,state);
        lv_obj_add_event_cb(state->popup,ui_adv_date_event,LV_EVENT_DELETE,state);
        state->calendar=lv_calendar_create(state->popup);
#if LV_USE_CALENDAR_HEADER_ARROW
        lv_obj_add_event_cb(lv_calendar_add_header_arrow(state->calendar),ui_adv_date_month,LV_EVENT_VALUE_CHANGED,state);
#endif
        lv_obj_add_event_cb(state->calendar,ui_adv_date_event,LV_EVENT_VALUE_CHANGED,state);
        lv_obj_add_event_cb(state->calendar,ui_adv_date_event,LV_EVENT_KEY,state);
    }
    lv_obj_update_layout(screen);
    width=lv_obj_get_width(screen)-16; height=lv_obj_get_height(screen)-16;
    if (width>300) width=300;
    if (height>300) height=300;
    if (width<1) width=1;
    if (height<1) height=1;
    lv_obj_set_size(state->calendar,width,height);
    lv_obj_center(state->calendar);
    if (!ui_adv_date_parse(state->date,&date) && !ui_adv_date_parse(state->minimum,&date)) {
        date.year=2026; date.month=1; date.day=1;
    }
    lv_calendar_set_today_date(state->calendar,date.year,date.month,date.day);
    lv_calendar_set_month_shown(state->calendar,date.year,date.month);
    ui_adv_date_refresh(state);
    lv_obj_set_hidden(state->popup,false);
    lv_obj_move_foreground(state->popup);
    {
        lv_group_t *group=lv_obj_get_group(state->label);
        if (group) {
            lv_obj_t *matrix=lv_calendar_get_btnmatrix(state->calendar);
            lv_group_add_obj(group,matrix);
            lv_group_focus_obj(matrix);
        }
    }
}
UI_ADV_INLINE void ui_adv_date_trigger(lv_event_t *event)
{ ui_adv_date_open((lv_obj_t *)lv_event_get_user_data(event)); }
UI_ADV_INLINE lv_obj_t *ui_adv_date_create(lv_obj_t *parent, const char *date, const char *format,
    const char *minimum, const char *maximum, const char *placeholder, uint8_t first_day, bool disabled)
{
    lv_obj_t *root=ui_adv_create(parent,UI_ADV_DATE);
    ui_adv_state_t *state=ui_adv_state(root);
    if (!root) return NULL;
    lv_obj_set_size(root,200,44);
    lv_obj_set_style_pad_all(root,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(root,0,LV_PART_MAIN);
    lv_obj_set_scrollable(root,false);
    state->placeholder=ui_adv_copy(placeholder ? placeholder : "Select date");
    if (!state->placeholder) { lv_obj_delete(root); return NULL; }
    state->first_day=first_day;
    (void)snprintf(state->format,sizeof(state->format),"%s",format ? format : "yyyy-mm-dd");
    (void)snprintf(state->minimum,sizeof(state->minimum),"%s",minimum ? minimum : "");
    (void)snprintf(state->maximum,sizeof(state->maximum),"%s",maximum ? maximum : "");
    state->label=ui_adv_button(root,"");
    lv_obj_set_size(state->label,LV_PCT(100),LV_PCT(100));
    lv_obj_set_state(root,LV_STATE_DISABLED,disabled);
    lv_obj_set_state(state->label,LV_STATE_DISABLED,disabled);
    lv_obj_add_event_cb(state->label,ui_adv_date_trigger,LV_EVENT_CLICKED,root);
    lv_obj_add_event_cb(state->label,ui_adv_date_event,LV_EVENT_KEY,state);
    lv_obj_add_event_cb(root,ui_adv_date_event,LV_EVENT_KEY,state);
    if (!ui_adv_date_set_date(root,date ? date : "")) { lv_obj_delete(root); return NULL; }
    return root;
}
#endif

#if UI_ADV_USE_MENU
UI_ADV_INLINE bool ui_adv_menu_children(ui_adv_state_t *state, uint32_t id)
{ ui_adv_node_t *node; for (node=state->nodes; node; node=node->next) if (node->parent==id) return true; return false; }
UI_ADV_INLINE void ui_adv_menu_show(lv_obj_t *root, uint32_t page)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    if (!state) return;
    state->page=page;
    lv_obj_set_hidden(state->back,page==0);
    node=page ? ui_adv_node(state,page) : NULL;
    lv_label_set_text(state->title,node ? lv_label_get_text(lv_obj_get_child(node->object,0)) : state->caption);
    for (node=state->nodes; node; node=node->next) {
        lv_obj_set_hidden(node->object,node->parent!=page);
        lv_obj_set_state(node->object,LV_STATE_CHECKED,(int32_t)node->id==state->selected);
        if (lv_obj_get_child_count(node->object)>1)
            lv_obj_set_hidden(lv_obj_get_child(node->object,1),!ui_adv_menu_children(state,node->id));
    }
}
UI_ADV_INLINE bool ui_adv_menu_set_selected(lv_obj_t *root, uint32_t selected)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node, *ancestor;
    uint32_t hops=0;
    if (!state) return false;
    if (!selected) { state->selected=0; ui_adv_menu_show(root,state->page); return true; }
    node=ui_adv_node(state,selected);
    if (!node || node->disabled || ui_adv_menu_children(state,selected)) return false;
    ancestor=node;
    while (ancestor->parent) {
        ancestor=ui_adv_node(state,ancestor->parent);
        if (!ancestor || ancestor->disabled || ++hops>256) return false;
    }
    state->selected=(int32_t)selected;
    ui_adv_menu_show(root,node->parent);
    return true;
}
UI_ADV_INLINE uint32_t ui_adv_menu_get_selected(lv_obj_t *root)
{ ui_adv_state_t *state=ui_adv_state(root); return state && state->selected>0 ? (uint32_t)state->selected : 0; }
UI_ADV_INLINE void ui_adv_menu_focus(lv_obj_t *source, lv_obj_t *target)
{
    lv_group_t *group=lv_obj_get_group(source);
    if (!target) return;
    if (group) lv_group_add_obj(group,target);
    if (lv_obj_get_group(target)) lv_group_focus_obj(target);
}
UI_ADV_INLINE void ui_adv_menu_event(lv_event_t *event)
{
    ui_adv_node_t *node=(ui_adv_node_t *)lv_event_get_user_data(event);
    ui_adv_state_t *state=node->owner;
    ui_adv_node_t *candidate,*first=NULL,*last=NULL,*previous=NULL,*next=NULL;
    if (node->disabled || !node->owner->interactive) return;
    if (lv_event_get_code(event)==LV_EVENT_KEY) {
        uint32_t key=lv_event_get_key(event);
        if (key==LV_KEY_LEFT) {
            candidate=ui_adv_node(state,state->page);
            ui_adv_menu_show(state->root,candidate ? candidate->parent : 0);
            if (candidate) ui_adv_menu_focus(node->object,candidate->object);
            return;
        }
        if (key==LV_KEY_UP || key==LV_KEY_DOWN) {
            bool passed=false;
            for (candidate=state->nodes;candidate;candidate=candidate->next) {
                if (candidate->parent!=state->page || candidate->disabled) continue;
                if (!first) first=candidate;
                if (candidate==node) { previous=last; passed=true; }
                else if (passed && !next) next=candidate;
                last=candidate;
            }
            candidate=key==LV_KEY_UP ? (previous ? previous : last) : (next ? next : first);
            if (candidate) ui_adv_menu_focus(node->object,candidate->object);
            return;
        }
        if (key!=LV_KEY_RIGHT) return;
    }
    if (ui_adv_menu_children(state,node->id)) {
        ui_adv_menu_show(state->root,node->id);
        for (candidate=state->nodes;candidate;candidate=candidate->next)
            if (candidate->parent==node->id && !candidate->disabled) { ui_adv_menu_focus(node->object,candidate->object);break; }
    }
    else if (lv_event_get_code(event)==LV_EVENT_CLICKED && ui_adv_menu_set_selected(state->root,node->id))
        lv_obj_send_event(state->root,LV_EVENT_VALUE_CHANGED,NULL);
}
UI_ADV_INLINE void ui_adv_menu_back(lv_event_t *event)
{
    ui_adv_state_t *state=(ui_adv_state_t *)lv_event_get_user_data(event);
    if (!state->interactive) return;
    ui_adv_node_t *node=ui_adv_node(state,state->page);
    ui_adv_menu_show(state->root,node ? node->parent : 0);
    if (node) ui_adv_menu_focus(state->back,node->object);
}
UI_ADV_INLINE lv_obj_t *ui_adv_menu_create(lv_obj_t *parent, const char *title)
{
    lv_obj_t *root=ui_adv_create(parent,UI_ADV_MENU);
    ui_adv_state_t *state=ui_adv_state(root);
    lv_obj_t *header;
    if (!root) return NULL;
    lv_obj_set_size(root,220,220);
    state->caption=ui_adv_copy(title ? title : "Menu");
    if (!state->caption) { lv_obj_delete(root); return NULL; }
    header=lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header,LV_PCT(100),36);
    lv_obj_set_flex_flow(header,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    state->back=ui_adv_button(header,LV_SYMBOL_LEFT);
    lv_obj_set_size(state->back,32,32);
    lv_obj_set_hidden(state->back,true);
    lv_obj_add_event_cb(state->back,ui_adv_menu_back,LV_EVENT_CLICKED,state);
    state->title=lv_label_create(header);
    lv_label_set_text(state->title,state->caption);
    lv_obj_set_flex_grow(state->title,1);
    state->content=lv_obj_create(root);
    lv_obj_remove_style_all(state->content);
    lv_obj_set_width(state->content,LV_PCT(100));
    lv_obj_set_flex_grow(state->content,1);
    lv_obj_set_flex_flow(state->content,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(state->content,4,LV_PART_MAIN);
    return root;
}
UI_ADV_INLINE bool ui_adv_menu_add(lv_obj_t *root, uint32_t id, uint32_t parent, const char *text, bool disabled)
{
    ui_adv_state_t *state=ui_adv_state(root);
    ui_adv_node_t *node;
    lv_obj_t *arrow;
    if (!state || !id || id>INT32_MAX || ui_adv_node(state,id)) return false;
    node=ui_adv_append(state,id,parent,disabled);
    if (!node) return false;
    node->object=ui_adv_button(state->content,text);
    lv_obj_set_width(node->object,LV_PCT(100));
    lv_obj_set_flex_flow(node->object,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(node->object,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(lv_obj_get_child(node->object,0),1);
    arrow=lv_label_create(node->object);
    lv_label_set_text(arrow,LV_SYMBOL_RIGHT);
    lv_obj_set_state(node->object,LV_STATE_DISABLED,disabled);
    lv_obj_add_event_cb(node->object,ui_adv_menu_event,LV_EVENT_CLICKED,node);
    lv_obj_add_event_cb(node->object,ui_adv_menu_event,LV_EVENT_KEY,node);
    ui_adv_menu_show(root,state->page);
    return true;
}
#endif
#undef UI_ADV_INLINE
#endif
