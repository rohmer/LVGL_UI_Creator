#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <algorithm>
#include <climits>
#include <set>
#include <stdexcept>

namespace Serialization {
// Preserve projects authored with the original combined editor/keyboard. Its
// editor becomes an independent textarea next to the keyboard on the same page.
inline void MigrateAdvancedKeyboardEditors(nlohmann::json& document) {
    using json=nlohmann::json;
    auto screen=[&](json& page,size_t screenIndex) {
        if(!page.contains("obj") || !page["obj"].is_array())return;
        std::set<int> ids;int ordinal=0;
        for(const auto& object:page["obj"])ids.insert(object.value("oid",ordinal++));
        json additions=json::array();ordinal=0;
        for(auto& object:page["obj"]) {
            const int oldId=object.value("oid",ordinal++);
            if(!object.contains("advancedKeyboard") || !object["advancedKeyboard"].is_object())continue;
            auto& keyboard=object["advancedKeyboard"];
            if(!keyboard.contains("text") && !keyboard.contains("placeholder") && !keyboard.contains("password") && !keyboard.contains("maxLength"))continue;
            int id=0;while(ids.count(id) && id<INT_MAX)++id;
            if(ids.count(id))throw std::invalid_argument("No free object ID for legacy keyboard text field");
            ids.insert(id);object["oid"]=oldId;
            auto base=object.value("base",json::object());
            auto coordinates=base.value("coords",json::object());
            const auto storedHeight=coordinates.value("height",json(220));
            const int height=storedHeight.is_number_integer() && storedHeight.get<int64_t>()>=3 && storedHeight.get<int64_t>()<=65535?storedHeight.get<int>():220;
            const int editorHeight=std::max(1,height-std::min(height*46/100,height-2)-46);
            base["name"]=base.value("name",std::string{}).empty()?"Keyboard text":base["name"].get<std::string>()+" text";
            base["coords"]["height"]=editorHeight;
            base["flags"]["clickable"]=true;base["flags"]["click_focusable"]=true;base["flags"]["scrollable"]=true;
            additions.push_back({{"oid",id},{"par",object.value("par",-1)},{"base",base},
                {"textarea",{{"text",keyboard.value("text",std::string{})},{"placeholder",keyboard.value("placeholder",std::string("Type here"))},
                    {"password",keyboard.value("password",false)},{"maxLength",keyboard.value("maxLength",0u)}}}});
            if(coordinates.contains("y") && coordinates["y"].is_number_integer())object["base"]["coords"]["y"]=std::clamp<int64_t>(coordinates["y"].get<int64_t>(),INT_MIN,INT_MAX-editorHeight)+editorHeight;
            object["base"]["coords"]["height"]=std::max(2,height-editorHeight);
            keyboard["targetOid"]=id;keyboard["autoFocus"]=true;
            for(const auto* field:{"text","placeholder","password","maxLength"})keyboard.erase(field);
            if(document.contains("variables"))for(auto& item:document["variables"]) {
                auto& variable=item.contains("var")?item["var"]:item;
                const auto property=variable.value("property",std::string{});
                if(variable.value("screen",size_t(0))==screenIndex && variable.value("oid",-1)==oldId &&
                    (property=="text" || property=="/advancedKeyboard/text")) {
                    variable["oid"]=id;variable["property"]="/textarea/text";
                }
            }
        }
        for(auto& object:additions)page["obj"].push_back(std::move(object));
    };
    if(document.contains("screens") && document["screens"].is_array()) {
        for(size_t i=0;i<document["screens"].size();++i)screen(document["screens"][i],i);
    } else screen(document,0);
}
}
