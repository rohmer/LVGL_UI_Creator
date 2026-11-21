#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Serialization::InteractionActions {
using json = nlohmann::json;
// Actions are plain data. They are never installed by the authoring loader.
json Normalize(const json& events);
bool ConditionMatches(const json& condition, const std::function<json(const std::string&)>& read);

// Resolve authored IDs to borrowed objects while editing. Deleted targets become
// an explicit broken reference, rather than silently targeting another widget.
void ResolveReferences(lv_obj_t* object, const std::map<int, lv_obj_t*>& objects);
void SetEvents(lv_obj_t* object, const json& events, const std::map<int, lv_obj_t*>& objects);
json EventsForSave(lv_obj_t* object, const std::unordered_map<lv_obj_t*, int>& indices);

struct Hooks {
    std::function<lv_obj_t*(size_t, int)> object;
    std::function<bool(size_t)> navigate;
    std::function<json(const std::string&)> read;
    std::function<void(const std::string&, const json&)> write;
    std::function<void(const std::string&)> error;
    // Optional host guard for programmatic output updates; ordinary contexts
    // keep processing authored user actions when this callback is absent.
    std::function<bool()> enabled;
};
class Context {
    struct Binding;
    Hooks hooks;
    bool executing = false;
    std::vector<std::unique_ptr<Binding>> bindings;
    static void Dispatch(lv_event_t* event);
    bool Execute(Binding& binding, const json& action);
public:
    explicit Context(Hooks callbacks);
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    void Bind(lv_obj_t* source, size_t screen, int oid, const json& events);
};
}
