#pragma once

#include <lvgl/lvgl.h>
#include "EditorEdit.h"

#if LVGL_VERSION_MAJOR >= 9
using EditorEventCode = lv_event_code_t;
#else
using EditorEventCode = lv_event_t;
#endif

using EditorEventCallback = void (*)(lv_obj_t*, EditorEventCode);

#if LVGL_VERSION_MAJOR >= 9
inline void DispatchEditorEvent(lv_event_t* event)
{
    auto callback = *static_cast<EditorEventCallback*>(lv_event_get_user_data(event));
    auto* object=lv_event_get_current_target_obj(event);auto code=lv_event_get_code(event);
    EditorEdit::Scope edit(object, code==LV_EVENT_VALUE_CHANGED && lv_obj_get_class(object)==&lv_textarea_class,
        code==LV_EVENT_CLICKED || code==LV_EVENT_VALUE_CHANGED || code==LV_EVENT_READY);
    callback(object, code);
}
#endif

template<EditorEventCallback Callback>
void SetEditorEventCallback(lv_obj_t* object)
{
#if LVGL_VERSION_MAJOR >= 9
    static EditorEventCallback callback = Callback;
    // Replacing an editor handler preserves the behavior of the original API.
    lv_obj_remove_event_cb(object, DispatchEditorEvent);
    lv_obj_add_event_cb(object, DispatchEditorEvent, LV_EVENT_ALL, &callback);
#else
    lv_obj_set_event_cb(object, Callback);
#endif
}
