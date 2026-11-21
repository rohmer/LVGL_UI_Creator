#pragma once

#include "LVObject.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include <unordered_set>
namespace Serialization
{
    inline thread_local const std::unordered_set<lv_obj_t*>* documentObjects = nullptr;

    class DocumentSerializationScope
    {
        const std::unordered_set<lv_obj_t*>* previous;
    public:
        explicit DocumentSerializationScope(const std::unordered_set<lv_obj_t*>& objects) : previous(documentObjects)
        {
            documentObjects = &objects;
        }
        ~DocumentSerializationScope() { documentObjects = previous; }
    };

    inline bool IsDocumentObject(lv_obj_t* object)
    {
        return documentObjects && documentObjects->count(object) != 0;
    }

    template <typename T>
    void FreeWidgetData(lv_event_t* event)
    {
        delete static_cast<T*>(lv_event_get_user_data(event));
    }

    template <typename T>
    T& WidgetData(lv_obj_t* object)
    {
#if LVGL_VERSION_MAJOR == 8
        if (auto* existing = lv_obj_get_event_user_data(object, FreeWidgetData<T>)) return *static_cast<T*>(existing);
#else
        for (uint32_t index = 0; index < lv_obj_get_event_count(object); ++index)
        {
            auto* descriptor = lv_obj_get_event_dsc(object, index);
            if (lv_event_dsc_get_cb(descriptor) == &FreeWidgetData<T>)
                return *static_cast<T*>(lv_event_dsc_get_user_data(descriptor));
        }
#endif
        auto* state = new T{};
        lv_obj_add_event_cb(object, &FreeWidgetData<T>, LV_EVENT_DELETE, state);
        return *state;
    }

    inline json ReadSection(lv_obj_t* object, const char* widget)
    {
        const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
        if (data)
        {
            const auto section = data->objectJson.find(widget);
            if (section != data->objectJson.end() && section->is_object())
                return *section;
        }
        return json::object();
    }

    inline void PreserveSection(lv_obj_t* object, const char* widget, const json& record)
    {
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        if (data && record.is_object())
            data->objectJson[widget] = record;
    }
}
#endif
