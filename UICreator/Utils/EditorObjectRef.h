#pragma once
#include <lvgl/lvgl.h>

// Observe a live editor object across callbacks that may delete it. LVGL 9.6's
// lv_obj_is_valid alias walks the object and cannot inspect a freed address.
class EditorObjectRef {
    lv_obj_t* object;
    static void Deleted(lv_event_t* event) {
        static_cast<EditorObjectRef*>(lv_event_get_user_data(event))->object = nullptr;
    }
public:
    explicit EditorObjectRef(lv_obj_t* value) : object(value) {
        if (object) lv_obj_add_event_cb(object, Deleted, LV_EVENT_DELETE, this);
    }
    ~EditorObjectRef() {
        if (object) lv_obj_remove_event_cb_with_user_data(object, Deleted, this);
    }
    EditorObjectRef(const EditorObjectRef&) = delete;
    EditorObjectRef& operator=(const EditorObjectRef&) = delete;
    lv_obj_t* Get() const { return object; }
};
