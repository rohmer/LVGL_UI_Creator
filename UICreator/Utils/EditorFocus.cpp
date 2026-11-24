#include "EditorFocus.h"
#include "EditorWindow.h"
#include "EditorObjectRef.h"
#include <algorithm>
#include <vector>

#if LVGL_VERSION_MAJOR >= 9
bool IsEditorControlAvailable(const lv_obj_t* control) {
    if (!control) return false;
    for (auto* parent = control; parent; parent = lv_obj_get_parent(parent)) {
        if (lv_obj_is_hidden(parent) || lv_obj_has_state(parent, LV_STATE_DISABLED)) return false;
        auto* content = lv_obj_get_parent(parent);
        auto* tabs = content ? lv_obj_get_parent(content) : nullptr;
        if (tabs && lv_obj_has_class(tabs, &lv_tabview_class) && lv_tabview_get_content(tabs) == content &&
            lv_obj_get_index(parent) != static_cast<int32_t>(lv_tabview_get_tab_active(tabs))) return false;
    }
    return true;
}
namespace {
lv_obj_t* Scope(lv_obj_t* control) {
    for (auto* parent = lv_obj_get_parent(control); parent; parent = lv_obj_get_parent(parent))
        if (EditorWindow::CloseData(parent)) return parent;
    return lv_obj_get_parent(control) ? lv_obj_get_parent(control) : control;
}
void Collect(lv_obj_t* parent, std::vector<lv_obj_t*>& fields) {
    if (!IsEditorControlAvailable(parent)) return;
    if (IsEditorInputControl(parent)) {
        if (lv_obj_is_clickable(parent) && lv_obj_is_click_focusable(parent))
            fields.push_back(parent);
        return;
    }
    for (uint32_t index = 0; index < lv_obj_get_child_count(parent); ++index) Collect(lv_obj_get_child(parent, index), fields);
}
void Clicked(lv_event_t* event) {
    FocusEditorControl(static_cast<lv_group_t*>(lv_event_get_user_data(event)), lv_event_get_current_target_obj(event));
}
}

bool IsEditorInputControl(const lv_obj_t* control) {
    return control && (lv_obj_has_class(control, &lv_textarea_class) || lv_obj_has_class(control, &lv_dropdown_class) ||
        lv_obj_has_class(control, &lv_roller_class) || lv_obj_has_class(control, &lv_checkbox_class) ||
        lv_obj_has_class(control, &lv_switch_class) || lv_obj_has_class(control, &lv_slider_class));
}

lv_group_t* EditorKeyboardGroup() {
    auto* display = lv_display_get_default();
    lv_group_t* fallback = nullptr;
    for (auto* input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input))
        if (lv_indev_get_type(input) == LV_INDEV_TYPE_KEYPAD)
            if (auto* group = lv_indev_get_group(input)) {
                if (lv_indev_get_display(input) == display) return group;
                if (!fallback) fallback = group;
            }
    return fallback;
}

void FocusEditorControl(lv_group_t* group, lv_obj_t* control) {
    if (!group || !control || !IsEditorControlAvailable(control)) return;
    EditorObjectRef alive(control);
    // Keep the clicked control attached and focused when refreshing its peers.
    // Removing it emits DEFOCUSED and closes an already-open dropdown popup.
    if (lv_group_get_focused(group) == control) {
        for (uint32_t index = lv_group_get_obj_count(group); index > 0; --index) {
            auto* member = lv_group_get_obj_by_index(group, index - 1);
            if (member != control) lv_group_remove_obj(member);
        }
    } else lv_group_remove_all_objs(group);
    // Defocusing an inline editor may finish it and recreate its controls.
    if (!alive.Get() || !IsEditorControlAvailable(control)) return;
    std::vector<lv_obj_t*> fields;
    if (IsEditorInputControl(control)) Collect(Scope(control), fields);
    auto position = std::find(fields.begin(), fields.end(), control);
    if (position == fields.end()) fields = {control};
    else std::rotate(fields.begin(), position, fields.end());
    for (auto* field : fields) {
        if (!alive.Get()) return;
        if (!IsEditorControlAvailable(field)) continue;
        if (lv_obj_has_class(field, &lv_textarea_class) && !lv_obj_has_class(field, &lv_spinbox_class))
            lv_textarea_set_text_selection(field, true);
        if (lv_obj_get_group(field) != group) lv_group_add_obj(group, field);
    }
    if (!alive.Get()) return;
    lv_group_focus_obj(control);
    lv_group_set_wrap(group, true);
    if (lv_obj_has_class(control, &lv_textarea_class)) lv_group_set_editing(group, true);
    lv_obj_scroll_to_view_recursive(control, LV_ANIM_OFF);
}

void BindEditorControl(lv_group_t* group, lv_obj_t* control) {
    if (!control) return;
    lv_obj_remove_event_cb(control, Clicked);
    if (group) lv_obj_add_event_cb(control, Clicked, LV_EVENT_CLICKED, group);
}
#endif
