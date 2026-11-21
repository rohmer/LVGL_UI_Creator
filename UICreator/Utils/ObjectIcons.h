#pragma once
#include <lvgl/lvgl.h>
#include <cstddef>
#include <cstdint>

// Decorative editor assets. These objects never enter the authored project.
namespace ObjectIcons {
struct Primitive;
struct Icon {
    const char* key;
    const char* name;
    const Primitive* commands;
    uint16_t commandCount;
};
const Icon* Widget(int id);
const Icon* Preset(int id);
const Icon* At(size_t index);
size_t Count();
const Icon* Action(const char* action);
const Icon* ActionAt(size_t index);
size_t ActionCount();
const Icon* Find(const char* key);
// Resolve authored objects without serializing them, changing layout, or using
// their editable display names. Saved palette provenance takes precedence.
const Icon* ForObject(lv_obj_t* object);
lv_obj_t* Create(lv_obj_t* parent, const Icon* icon, int size = 32);
const Icon* Get(lv_obj_t* object);
// Replace an existing decorative icon without changing its object or layout.
void Set(lv_obj_t* iconObject, const Icon* icon);
}
