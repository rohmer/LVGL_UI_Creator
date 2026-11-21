#pragma once
#include <lvgl/lvgl.h>
#include "ObjectUserData.h"
#include <algorithm>
#include <map>
#include <vector>
#if __has_include(<src/core/lv_group_private.h>)
#include <src/core/lv_group_private.h>
#endif

namespace Serialization::Accessibility {
// Explicit indexes precede natural document order. -1 excludes an authored
// widget and its internal focusable children (for example textBox.Entry).
inline void ApplyFocusOrder(lv_group_t* group,const std::vector<lv_obj_t*>& authored)
{
    if(!group)return;
    std::map<lv_obj_t*,int> indexes;
    for(auto* object:authored)if(object) {
        const auto* data=static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
        if(!data||!data->objectJson.is_object())continue;
        const auto base=data->objectJson.value("base",json::object());
        const auto accessibility=base.value("accessibility",json::object());
        if(accessibility.is_object()&&accessibility.contains("tabIndex")&&accessibility["tabIndex"].is_number_integer())indexes[object]=accessibility["tabIndex"].get<int>();
    }
    if(indexes.empty())return;
    struct Entry{lv_obj_t* object;int index;size_t natural;};std::vector<Entry> entries;
    std::vector<lv_obj_t*> original;
#if LV_VERSION_CHECK(9,2,0)
    for(auto* node=static_cast<lv_obj_t**>(lv_ll_get_head(&group->obj_ll));node;node=static_cast<lv_obj_t**>(lv_ll_get_next(&group->obj_ll,node)))original.push_back(*node);
#else
    for(auto* node=static_cast<lv_obj_t**>(_lv_ll_get_head(&group->obj_ll));node;node=static_cast<lv_obj_t**>(_lv_ll_get_next(&group->obj_ll,node)))original.push_back(*node);
#endif
    for(size_t i=0;i<original.size();++i) {
        int index=INT32_MAX;
        for(auto* ancestor=original[i];ancestor;ancestor=lv_obj_get_parent(ancestor))if(indexes.count(ancestor)){index=indexes.at(ancestor);break;}
        if(index>=0)entries.push_back({original[i],index,i});
    }
    std::stable_sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.index<b.index;});
    bool changed=entries.size()!=original.size();
    for(size_t i=0;!changed&&i<entries.size();++i)changed=entries[i].object!=original[i];
    if(!changed)return;
    auto* focused=lv_group_get_focused(group);lv_group_remove_all_objs(group);
    for(const auto& entry:entries)lv_group_add_obj(group,entry.object);
    if(focused&&std::any_of(entries.begin(),entries.end(),[&](const Entry& entry){return entry.object==focused;}))lv_group_focus_obj(focused);
}
}
