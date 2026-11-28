#pragma once
#include "LVObject.h"
#include "WidgetRecords.h"
#include "RuntimeFeatures.h"
#include <algorithm>
#include <set>
#include <map>
#include <stdexcept>

#define UI_ADV_USE_RADIO_GROUP LVSERIAL_USE_RADIO_GROUP
#define UI_ADV_USE_CHECKBOX_BUTTON LVSERIAL_USE_CHECKBOX_BUTTON
#define UI_ADV_USE_LABELED_CHECKBOX LVSERIAL_USE_LABELED_CHECKBOX
#define UI_ADV_USE_DATE_PICKER LVSERIAL_USE_DATE_PICKER
#define UI_ADV_USE_MENU LVSERIAL_USE_MENU
#include "AdvancedControlsRuntime.h"

namespace Serialization {
class AdvancedControls {
    static int Integer(const json& record,const char* key,int fallback,int minimum,int maximum)
    {
        const auto found=record.find(key);
        if(found==record.end()) return fallback;
        if(!found->is_number_integer() || (found->is_number_unsigned() && found->get<uint64_t>()>uint64_t(maximum)))
            throw std::invalid_argument(std::string(key)+" must be an integer in its supported range");
        const auto value=found->get<int64_t>();
        if(value<minimum || value>maximum) throw std::invalid_argument(std::string(key)+" is outside its supported range");
        return static_cast<int>(value);
    }
    static std::string Text(const json& record,const char* key,const char* fallback,size_t limit=4096)
    {
        const auto value=record.value(key,std::string(fallback));
        if(value.size()>limit || value.find('\0')!=std::string::npos)
            throw std::invalid_argument(std::string(key)+" is too long or contains a NUL byte");
        return value;
    }
    static bool Date(const std::string& text)
    {
        if(text.size()!=10 || text[4]!='-' || text[7]!='-') return false;
        for(size_t i=0;i<text.size();++i) if(i!=4 && i!=7 && (text[i]<'0'||text[i]>'9')) return false;
        const auto year=std::stoi(text.substr(0,4)),month=std::stoi(text.substr(5,2)),day=std::stoi(text.substr(8,2));
        static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
        if(year<1 || month<1 || month>12) return false;
        const auto count=days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
        return day>=1 && day<=count;
    }
    static void Event(lv_event_t* event)
    {
        auto* object=lv_event_get_current_target_obj(event);
        const auto* type=Type(object);
        if(!type || lv_event_get_target_obj(event)!=object) return;
        auto record=ReadSection(object,type);
#if LVSERIAL_USE_RADIO_GROUP
        if(std::string(type)=="radioGroup") record["selected"]=ui_adv_radio_get_selected(object);
#endif
#if LVSERIAL_USE_CHECKBOX_BUTTON || LVSERIAL_USE_LABELED_CHECKBOX
        if(std::string(type)=="checkboxButton" || std::string(type)=="labeledCheckbox") record["checked"]=ui_adv_checkbox_get_checked(object);
#endif
#if LVSERIAL_USE_DATE_PICKER
        if(std::string(type)=="datePicker") record["date"]=ui_adv_date_get_date(object);
#endif
#if LVSERIAL_USE_MENU
        if(std::string(type)=="menu") record["selected"]=ui_adv_menu_get_selected(object);
#endif
        PreserveSection(object,type,record);
    }
public:
    static json Normalize(const std::string& type,const json& input)
    {
        if(!input.is_object()) throw std::invalid_argument("Advanced control properties must be an object");
        auto record=input;
        if(type=="radioGroup") {
            record["title"]=Text(input,"title","Location",256);
            record["horizontal"]=input.value("horizontal",true);
            if(!record.contains("options")) record["options"]=json::array({{{"text","New York"},{"disabled",false}},{{"text","Paris"},{"disabled",false}},{{"text","London"},{"disabled",false}}});
            if(!record["options"].is_array() || record["options"].empty() || record["options"].size()>128)
                throw std::invalid_argument("Radio group needs between 1 and 128 options");
            for(auto& option:record["options"]) {
                if(!option.is_object()) throw std::invalid_argument("Each radio option must be an object");
                option["text"]=Text(option,"text","Option",1024);
                option["disabled"]=option.value("disabled",false);
            }
            auto selected=Integer(input,"selected",0,-1,127);
            if(selected>=static_cast<int>(record["options"].size()) || (selected>=0 && record["options"][selected].value("disabled",false))) selected=-1;
            record["selected"]=selected;
        } else if(type=="checkboxButton" || type=="labeledCheckbox") {
            record["text"]=Text(input,"text",type=="checkboxButton" ? "Option" : "Send me updates",1024);
            record["checked"]=input.value("checked",false);
            record["disabled"]=input.value("disabled",false);
            if(type=="labeledCheckbox") record["description"]=Text(input,"description","Click anywhere in this label to select.");
        } else if(type=="datePicker") {
            record["date"]=Text(input,"date","2026-01-01",10);
            record["minDate"]=Text(input,"minDate","",10);
            record["maxDate"]=Text(input,"maxDate","",10);
            record["format"]=Text(input,"format","yyyy-mm-dd",10);
            record["placeholder"]=Text(input,"placeholder","Select date",1024);
            record["firstDay"]=Integer(input,"firstDay",0,0,6);
            record["disabled"]=input.value("disabled",false);
            for(const auto* key:{"date","minDate","maxDate"})
                if(!record.at(key).get_ref<const std::string&>().empty() && !Date(record.at(key)))
                    throw std::invalid_argument(std::string(key)+" must be a valid yyyy-mm-dd date");
            const auto date=record.at("date").get<std::string>(),minimum=record.at("minDate").get<std::string>(),maximum=record.at("maxDate").get<std::string>();
            if(!minimum.empty() && !maximum.empty() && minimum>maximum) throw std::invalid_argument("Minimum date must not exceed maximum date");
            if(!date.empty() && ((!minimum.empty() && date<minimum) || (!maximum.empty() && date>maximum)))
                throw std::invalid_argument("Selected date must be within the permitted range");
            const auto format=record.at("format").get<std::string>();
            if(format!="yyyy-mm-dd" && format!="dd/mm/yyyy" && format!="mm/dd/yyyy")
                throw std::invalid_argument("Date format must be yyyy-mm-dd, dd/mm/yyyy or mm/dd/yyyy");
        } else if(type=="menu") {
            record["title"]=Text(input,"title","Menu",256);
            if(!record.contains("items")) record["items"]=json::array({{{"id",1},{"parent",0},{"text","Books"},{"disabled",false}},{{"id",2},{"parent",0},{"text","Electronics"},{"disabled",false}},{{"id",3},{"parent",2},{"text","Accessories"},{"disabled",false}}});
            if(!record["items"].is_array() || record["items"].size()>256) throw std::invalid_argument("Menu supports at most 256 items");
            std::map<int,int> parents;
            std::map<int,bool> disabled;
            int next=1;
            for(const auto& item:record["items"])
                if(item.is_object() && item.contains("id")) next=(std::max)(next,Integer(item,"id",1,1,INT32_MAX-1)+1);
            for(auto& item:record["items"]) {
                if(!item.is_object()) throw std::invalid_argument("Each menu item must be an object");
                const auto id=Integer(item,"id",next,1,INT32_MAX-1),parent=Integer(item,"parent",0,0,INT32_MAX-1);
                next=(std::max)(next,id+1);
                if(!parents.emplace(id,parent).second) throw std::invalid_argument("Menu item IDs must be unique");
                item["id"]=id; item["parent"]=parent;
                item["text"]=Text(item,"text","Item",1024); item["disabled"]=item.value("disabled",false);
                disabled[id]=item.at("disabled").get<bool>();
            }
            for(const auto& item:parents) {
                std::set<int> visited{item.first};
                for(auto parent=item.second;parent;) {
                    if(!parents.count(parent)) throw std::invalid_argument("Menu parent must reference another item or 0");
                    if(!visited.insert(parent).second) throw std::invalid_argument("Menu hierarchy must not contain cycles");
                    parent=parents.at(parent);
                }
            }
            auto selected=Integer(input,"selected",0,0,INT32_MAX-1);
            if(!parents.count(selected)) selected=0;
            for(const auto& item:record["items"])
                if((item.at("id")==selected && item.value("disabled",false)) || item.at("parent")==selected) selected=0;
            for(auto ancestor=selected;ancestor;ancestor=parents.at(ancestor))
                if(disabled.at(ancestor)) { selected=0;break; }
            record["selected"]=selected;
        } else throw std::invalid_argument("Unknown advanced control: "+type);
        return record;
    }

    static const char* Type(lv_obj_t* object)
    {
#if LVGL_VERSION_MAJOR >= 9 && (LVSERIAL_USE_RADIO_GROUP || LVSERIAL_USE_CHECKBOX_BUTTON || LVSERIAL_USE_LABELED_CHECKBOX || LVSERIAL_USE_DATE_PICKER || LVSERIAL_USE_MENU)
        const auto* state=ui_adv_state(object);
        if(!state) return nullptr;
        switch(state->kind) {
#if LVSERIAL_USE_RADIO_GROUP
        case UI_ADV_RADIO:return "radioGroup";
#endif
#if LVSERIAL_USE_CHECKBOX_BUTTON
        case UI_ADV_CHECKBOX:return "checkboxButton";
#endif
#if LVSERIAL_USE_LABELED_CHECKBOX
        case UI_ADV_LABELED:return "labeledCheckbox";
#endif
#if LVSERIAL_USE_DATE_PICKER
        case UI_ADV_DATE:return "datePicker";
#endif
#if LVSERIAL_USE_MENU
        case UI_ADV_MENU:return "menu";
#endif
        }
#else
        (void)object;
#endif
        return nullptr;
    }

    static lv_obj_t* Part(lv_obj_t* object,const std::string& name)
    {
#if LVGL_VERSION_MAJOR >= 9 && (LVSERIAL_USE_RADIO_GROUP || LVSERIAL_USE_CHECKBOX_BUTTON || LVSERIAL_USE_LABELED_CHECKBOX || LVSERIAL_USE_DATE_PICKER || LVSERIAL_USE_MENU)
        return ui_adv_part(object,name.c_str());
#else
        (void)object;(void)name;return nullptr;
#endif
    }
    static void Apply(lv_obj_t* object,const json& incoming)
    {
        const auto* type=Type(object);
        if(!type) throw std::invalid_argument("Advanced control disabled or unavailable in this runtime");
        auto previous=ReadSection(object,type),merged=previous;
        merged.update(incoming);
        const auto record=Normalize(type,merged);
        if(previous==record) return;
#if LVGL_VERSION_MAJOR >= 9 && (LVSERIAL_USE_RADIO_GROUP || LVSERIAL_USE_CHECKBOX_BUTTON || LVSERIAL_USE_LABELED_CHECKBOX || LVSERIAL_USE_DATE_PICKER || LVSERIAL_USE_MENU)
        auto* state=ui_adv_state(object);
#if LVSERIAL_USE_RADIO_GROUP
        if(std::string(type)=="radioGroup") {
            lv_label_set_text(state->title,record.at("title").get_ref<const std::string&>().c_str());
            lv_obj_set_flex_flow(state->content,record.value("horizontal",true) ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
            if(!previous.contains("options") || previous.at("options")!=record.at("options")) {
                ui_adv_clear(object);
                for(const auto& option:record.at("options"))
                    if(!ui_adv_radio_add(object,option.at("text").get_ref<const std::string&>().c_str(),option.value("disabled",false))) throw std::bad_alloc();
            }
            ui_adv_radio_set_selected(object,record.at("selected").get<int32_t>());
        }
#endif
#if LVSERIAL_USE_CHECKBOX_BUTTON || LVSERIAL_USE_LABELED_CHECKBOX
        if(std::string(type)=="checkboxButton" || std::string(type)=="labeledCheckbox") {
            lv_checkbox_set_text(state->label,record.at("text").get_ref<const std::string&>().c_str());
            if(state->description) lv_label_set_text(state->description,record.at("description").get_ref<const std::string&>().c_str());
            ui_adv_checkbox_set_checked(object,record.value("checked",false));
            lv_obj_set_state(object,LV_STATE_DISABLED,record.value("disabled",false));
            lv_obj_set_state(state->label,LV_STATE_DISABLED,record.value("disabled",false));
        }
#endif
#if LVSERIAL_USE_DATE_PICKER
        if(std::string(type)=="datePicker") {
            auto* placeholder=ui_adv_copy(record.at("placeholder").get_ref<const std::string&>().c_str());
            if(!placeholder) throw std::bad_alloc();
            lv_free(state->placeholder); state->placeholder=placeholder;
            std::memcpy(state->format,record.at("format").get_ref<const std::string&>().c_str(),11);
            std::strcpy(state->minimum,record.at("minDate").get_ref<const std::string&>().c_str());
            std::strcpy(state->maximum,record.at("maxDate").get_ref<const std::string&>().c_str());
            state->first_day=record.at("firstDay").get<uint8_t>();
            ui_adv_date_set_date(object,record.at("date").get_ref<const std::string&>().c_str());
            lv_obj_set_state(object,LV_STATE_DISABLED,record.value("disabled",false));
            lv_obj_set_state(state->label,LV_STATE_DISABLED,record.value("disabled",false));
            ui_adv_date_close(object);
        }
#endif
#if LVSERIAL_USE_MENU
        if(std::string(type)=="menu") {
            auto* caption=ui_adv_copy(record.at("title").get_ref<const std::string&>().c_str());
            if(!caption) throw std::bad_alloc();
            lv_free(state->caption); state->caption=caption;
            if(!previous.contains("items") || previous.at("items")!=record.at("items")) {
                ui_adv_clear(object);
                for(const auto& item:record.at("items"))
                    if(!ui_adv_menu_add(object,item.at("id").get<uint32_t>(),item.at("parent").get<uint32_t>(),item.at("text").get_ref<const std::string&>().c_str(),item.value("disabled",false))) throw std::bad_alloc();
            }
            ui_adv_menu_set_selected(object,record.at("selected").get<uint32_t>());
            ui_adv_menu_show(object,state->page);
        }
#endif
#endif
        PreserveSection(object,type,record);
    }

    static lv_obj_t* Create(const std::string& type,lv_obj_t* parent=nullptr)
    {
        if(!parent) parent=lv_screen_active();
        lv_obj_t* object=nullptr;
#if LVGL_VERSION_MAJOR >= 9
#if LVSERIAL_USE_RADIO_GROUP
        if(type=="radioGroup") object=ui_adv_radio_create(parent,"",true);
#endif
#if LVSERIAL_USE_CHECKBOX_BUTTON
        if(type=="checkboxButton") object=ui_adv_checkbox_create(parent,"","",false);
#endif
#if LVSERIAL_USE_LABELED_CHECKBOX
        if(type=="labeledCheckbox") object=ui_adv_checkbox_create(parent,"","",true);
#endif
#if LVSERIAL_USE_DATE_PICKER
        if(type=="datePicker") object=ui_adv_date_create(parent,"","yyyy-mm-dd","","","Select date",0,false);
#endif
#if LVSERIAL_USE_MENU
        if(type=="menu") object=ui_adv_menu_create(parent,"Menu");
#endif
#endif
        if(!object) return nullptr;
        try {
            LVObject::FromJSON(json::object(),object);
            lv_obj_add_event_cb(object,Event,LV_EVENT_VALUE_CHANGED,nullptr);
            Apply(object,json::object());
            return object;
        } catch(...) { lv_obj_delete(object);throw; }
    }
    static json ToJSON(lv_obj_t* object)
    {
        const auto* type=Type(object);
        if(!type) throw std::invalid_argument("Object is not an advanced control");
        auto record=ReadSection(object,type);
#if LVSERIAL_USE_RADIO_GROUP
        if(std::string(type)=="radioGroup") record["selected"]=ui_adv_radio_get_selected(object);
#endif
#if LVSERIAL_USE_CHECKBOX_BUTTON || LVSERIAL_USE_LABELED_CHECKBOX
        if(std::string(type)=="checkboxButton" || std::string(type)=="labeledCheckbox") record["checked"]=ui_adv_checkbox_get_checked(object);
#endif
#if LVSERIAL_USE_DATE_PICKER
        if(std::string(type)=="datePicker") record["date"]=ui_adv_date_get_date(object);
#endif
#if LVSERIAL_USE_MENU
        if(std::string(type)=="menu") record["selected"]=ui_adv_menu_get_selected(object);
#endif
        return {{"base",LVObject::ToJSON(object)},{type,Normalize(type,record)}};
    }
    static lv_obj_t* FromJSON(const json& record)
    {
        for(const auto* type:{"radioGroup","checkboxButton","labeledCheckbox","datePicker","menu"}) {
            if(!record.contains(type)) continue;
            const auto normalized=Normalize(type,record.at(type));
            auto* object=Create(type);
            if(!object) throw std::invalid_argument(std::string("Advanced control unavailable in this runtime: ")+type);
            try {
                LVObject::FromJSON(record.at("base"),object);
                Apply(object,normalized);
                LVObject::FromJSON(record.at("base"),object);
                return object;
            } catch(...) { lv_obj_delete(object);throw; }
        }
        return nullptr;
    }
};
}
