#pragma once

#include <lvgl/lvgl.h>

#if LVGL_VERSION_MAJOR >= 9
#include <cstdlib>
#include <cmath>
#include "EditorEdit.h"
#include "EditorGrid.h"
#include <Serialization/LVObject.h>

namespace EditorDrag
{
    using Completion = void (*)(lv_obj_t*);

    struct Drag
    {
        lv_obj_t* target;
        Completion completion;
        lv_point_t pointerStart{};
        lv_point_t positionStart{};
        lv_indev_t* pointer = nullptr;
        bool moved = false;
        bool documentObject = false;
        EditorEdit::Observer* history = nullptr;
    };

    inline void HandleEvent(lv_event_t* event)
    {
        auto* drag = static_cast<Drag*>(lv_event_get_user_data(event));
        const auto code = lv_event_get_code(event);
        if (code == LV_EVENT_DELETE)
        {
            if(drag->documentObject)EditorGrid::Clear(drag->target);
            if(drag->history && EditorEdit::observer==drag->history)drag->history->EndEdit();
            delete drag;
            return;
        }
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
        {
            if(drag->documentObject)EditorGrid::Clear(drag->target);
            const bool moved = drag->moved;
            drag->moved = false;
            drag->pointer = nullptr;
            if (moved && drag->completion) drag->completion(drag->target);
            auto* history=drag->history;drag->history=nullptr;
            if(history && EditorEdit::observer==history)history->EndEdit();
            return;
        }
        if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING)
            return;

        if (drag->documentObject)
        {
            const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(drag->target));
            if (data && data->objectJson.contains("base") && !data->objectJson["base"].value("drag", true))
                return;
        }
        auto* pointer = lv_event_get_indev(event);
        if (!pointer || lv_indev_get_type(pointer) != LV_INDEV_TYPE_POINTER)
            return;
        lv_point_t point;
        lv_indev_get_point(pointer, &point);
        if (code == LV_EVENT_PRESSED)
        {
            if(drag->documentObject && !drag->history && EditorEdit::observer){drag->history=EditorEdit::observer;drag->history->BeginEdit(drag->target,false);}
            lv_obj_update_layout(drag->target);
            drag->pointer = pointer;
            drag->pointerStart = point;
            drag->positionStart = {lv_obj_get_x(drag->target), lv_obj_get_y(drag->target)};
            drag->moved = false;
            if(!drag->documentObject)lv_obj_move_foreground(drag->target);
            return;
        }
        if (drag->pointer != pointer)
            return;

        int dx = point.x - drag->pointerStart.x;
        int dy = point.y - drag->pointerStart.y;
        double scaleX = 1, scaleY = 1;
        for(auto* parent = lv_obj_get_parent(drag->target); parent; parent = lv_obj_get_parent(parent)) {
            scaleX *= lv_obj_get_style_transform_scale_x(parent, LV_PART_MAIN) / 256.0;
            scaleY *= lv_obj_get_style_transform_scale_y(parent, LV_PART_MAIN) / 256.0;
        }
        dx = (int)std::lround(dx / scaleX);dy = (int)std::lround(dy / scaleY);
        bool allowX = true, allowY = true;
        if (drag->documentObject)
        {
            const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(drag->target));
            if (data && data->objectJson.contains("base"))
            {
                const int direction = data->objectJson["base"].value("dragDir", 3);
                allowX = (direction & 1) != 0; allowY = (direction & 2) != 0;
                if (!allowX) dx = 0;
                if (!allowY) dy = 0;
            }
        }
        if (!drag->moved && std::abs(dx) < 4 && std::abs(dy) < 4)
            return;
        if(!drag->moved && drag->documentObject)lv_obj_move_foreground(drag->target);
        drag->moved = true;
        int x = drag->positionStart.x + dx, y = drag->positionStart.y + dy;
        if(drag->documentObject)EditorGrid::Move(drag->target, x, y, allowX, allowY);
        lv_obj_set_pos(drag->target, x, y);
    }

    inline void Attach(lv_obj_t* handle, lv_obj_t* target, Completion completion = nullptr, bool documentObject = false)
    {
        if (!documentObject)
        {
            lv_obj_set_clickable(handle, true);
            lv_obj_set_press_lock(handle, true);
            lv_obj_set_scrollable(handle, false);
            lv_obj_set_scroll_chain(handle, false);
        }
        auto* drag = new Drag{target, completion};
        drag->documentObject = documentObject;
        lv_obj_add_event_cb(handle, HandleEvent, LV_EVENT_ALL, drag);
    }
}
#endif
