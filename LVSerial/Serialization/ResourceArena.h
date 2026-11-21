#pragma once
#include <lvgl/lvgl.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace Serialization
{
struct ResourceArena
{
    std::vector<std::shared_ptr<void>> allocations;
    std::map<std::string, lv_font_t *> fonts;
    std::map<std::string, lv_font_t *> fontRefs;
};
inline thread_local ResourceArena *currentResourceArena = nullptr;
inline ResourceArena &Resources()
{
    static ResourceArena fallback;
    return currentResourceArena ? *currentResourceArena : fallback;
}
template <class T, class... Args> T &Resource(Args &&...args)
{
    auto item = std::make_shared<T>(std::forward<Args>(args)...);
    auto *result = item.get();
    Resources().allocations.push_back(std::move(item));
    return *result;
}
struct ResourceScope
{
    ResourceArena *previous;
    explicit ResourceScope(ResourceArena &arena) : previous(currentResourceArena)
    {
        currentResourceArena = &arena;
    }
    ~ResourceScope()
    {
        currentResourceArena = previous;
    }
};
} // namespace Serialization
