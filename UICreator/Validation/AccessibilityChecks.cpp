#include "AccessibilityChecks.h"
#include <Serialization/LVVariable.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <sstream>
namespace ScreenValidation {
namespace {
using json=nlohmann::json;
const json& Object(const json& value,const char* key){static const json empty=json::object();const auto found=value.find(key);return found!=value.end()&&found->is_object()?*found:empty;}
json Screens(const json& document){return document.contains("screens")?document.at("screens"):json::array({document});}
void SyncAlias(json& doc,size_t screen){if(doc.contains("screens")&&doc.contains("obj")&&screen==doc.value("editorScreen",doc.value("activeScreen",size_t(0))))doc["obj"]=doc["screens"][screen].value("obj",json::array());}
json* Record(json& document,size_t screen,int oid){json* page=&document;if(document.contains("screens")){if(!document["screens"].is_array()||screen>=document["screens"].size())return nullptr;page=&document["screens"][screen];}if(!page->contains("obj")||!(*page)["obj"].is_array())return nullptr;size_t next=0;for(auto& r:(*page)["obj"])if(r.value("oid",int(next++))==oid)return &r;return nullptr;}
json Styles(const json& record,unsigned selector=0){json out=json::object();for(const auto& style:Object(record,"base").value("nativeStyles",json::array()))if(style.is_object()&&style.value("selector",0u)==selector){const auto values=style.value("values",json::object());if(values.is_object())for(const auto& item:values.items())out[item.key()]=item.value();}return out;}
std::optional<unsigned> Number(const json& value,const char* key){const auto found=value.find(key);if(found!=value.end()&&found->is_number_integer())return found->get<unsigned>();return {};}
std::string Name(const json& record,int oid){return Object(record,"base").value("name",std::string("Widget ")+std::to_string(oid));}
std::string Text(const json& record){if(record.contains("text")&&record["text"].is_string())return record["text"].get<std::string>();for(auto kind:{"button","textarea","textBox","cb","checkbox","label"}){const auto& part=Object(record,kind);if(part.contains("text")&&part["text"].is_string())return part["text"].get<std::string>();}return {};}
bool Control(const json& r){for(auto key:{"button","textarea","textBox","cb","checkbox","dropdown","roller","slider","spinbox","switch","btnm","calendar","cal","advancedKeyboard"})if(r.contains(key))return true;return false;}
bool Visible(const json& r){const auto& base=Object(r,"base");return !base.value("hidden",false)&&!Object(base,"flags").value("hidden",false);}
unsigned Color(lv_color_t color){return lv_color_to_u32(color)&0xffffffu;}
bool BrokenAction(const json& doc,size_t screen,const json& action){
    const auto pages=Screens(doc);const auto type=action.value("action",std::string{});if(type=="navigate")return action.value("screen",size_t(0))>=pages.size();
    if(!action.contains("targetOid"))return false;const int id=action.at("targetOid");if(id==-1)return false;const auto target=action.value("targetScreen",screen);if(target>=pages.size())return true;
    int next=0;for(const auto& record:pages[target].value("obj",json::array()))if(record.value("oid",next++)==id)return false;return true;
}
bool Unrendered(lv_obj_t* object){for(auto* node=object;node;node=lv_obj_get_parent(node))if(lv_obj_is_hidden(node))return true;return false;}
bool MissingBindingTarget(const json& document,const Serialization::VariableBinding& binding){
    const auto pages=Screens(document);
    if(binding.screen>=pages.size())return true;
    int ordinal=0;for(const auto& object:pages[binding.screen].value("obj",json::array()))
        if(object.value("oid",ordinal++)==binding.objectId)return false;
    return true;
}
bool RemoveBrokenBinding(json& document,const Issue& issue){
    if(issue.variableName.empty()||issue.bindingIndex<0)return false;
    auto found=document.find("variables");if(found==document.end()||!found->is_array())return false;
    for(auto& record:*found){
        auto variable=Serialization::LVVariable::FromJSON(record);
        if(variable.name!=issue.variableName)continue;
        const auto bindings=Serialization::LVVariable::Bindings(variable);
        const auto index=static_cast<size_t>(issue.bindingIndex);
        if(index>=bindings.size())return false;
        const auto& binding=bindings[index];
        if(binding.screen!=issue.screen||binding.objectId!=issue.oid||!MissingBindingTarget(document,binding))return false;
        auto& value=record["var"];
        if(value.contains("bindings"))value["bindings"].erase(index);
        else {value["bindings"]=json::array();value["oid"]=-1;value["property"]="";value.erase("screen");}
        return true;
    }
    return false;
}
bool MissingGlyph(const lv_font_t* font,const std::string& text){
    for(size_t i=0;i<text.size();){const auto first=static_cast<unsigned char>(text[i++]);uint32_t cp=first;int count=first<128?0:(first&0xe0)==0xc0?1:(first&0xf0)==0xe0?2:(first&0xf8)==0xf0?3:-1;if(count<0)return true;if(count){cp=first&((1u<<(6-count))-1);while(count--){if(i>=text.size()||(static_cast<unsigned char>(text[i])&0xc0)!=0x80)return true;cp=(cp<<6)|(static_cast<unsigned char>(text[i++])&63);}}if(cp<32||cp==0x200d||(cp>=0xfe00&&cp<=0xfe0f))continue;lv_font_glyph_dsc_t glyph{};if(!lv_font_get_glyph_dsc(font,&glyph,cp,0)||glyph.is_placeholder)return true;}return false;
}
}
double ContrastRatio(unsigned foreground,unsigned background){auto luminance=[](unsigned value){auto channel=[](unsigned byte){double c=byte/255.0;return c<=0.04045?c/12.92:std::pow((c+0.055)/1.055,2.4);};return 0.2126*channel((value>>16)&255)+0.7152*channel((value>>8)&255)+0.0722*channel(value&255);};const double a=luminance(foreground),b=luminance(background);return ((std::max)(a,b)+0.05)/((std::min)(a,b)+0.05);}
std::vector<Issue> AccessibilityIssues(const json& document,const std::vector<lv_obj_t*>& live){
    std::vector<Issue> issues;auto add=[&](const char* group,size_t si,int oid,std::string message){issues.push_back({Severity::Warning,group,si,oid,std::move(message)});};
    try{
        const auto screens=Screens(document);const auto current=document.value("editorScreen",document.value("activeScreen",size_t(0)));
        const auto translations=document.value("translations",json::object());const auto locales=translations.value("locales",json::object());
        for(size_t si=0;si<screens.size();++si){const auto records=screens[si].value("obj",json::array());std::map<int,json> byId;std::map<int,lv_obj_t*> objects;std::map<int,int> focus;size_t ordinal=0;
            for(const auto& r:records){const int oid=r.value("oid",int(ordinal));byId[oid]=r;if(si==current&&ordinal<live.size())objects[oid]=live[ordinal];++ordinal;}
            for(const auto& r:records){const int oid=r.value("oid",0);const auto name=Name(r,oid);const auto& base=Object(r,"base");const auto& accessibility=Object(base,"accessibility");
                if(accessibility.contains("tabIndex")){
                    if(!accessibility["tabIndex"].is_number_integer()||accessibility["tabIndex"].get<int>() < -1)add("Focus order",si,oid,name+" has an invalid focus index; use -1 to skip or a nonnegative number.");
                    else{const int index=accessibility["tabIndex"].get<int>();if(index>=0&&Control(r)){if(focus.count(index))add("Focus order",si,oid,name+" shares focus index "+std::to_string(index)+" with "+Name(byId[focus[index]],focus[index])+".");else focus[index]=oid;}if(index>=0&&(!Visible(r)||base.value("disabled",false)))add("Focus order",si,oid,name+" has an explicit focus index while hidden or disabled.");}
                }
                const auto text=Text(r);if(!Visible(r))continue;auto* object=objects.count(oid)?objects.at(oid):nullptr;if(object&&Unrendered(object))continue;
                if(!text.empty()){
                    auto values=Styles(r);std::optional<unsigned> foreground=Number(values,"text_color"),background;bool opaque=values.value("text_opa",255u)==255&&values.value("opa",255u)==255;
                    if(object){foreground=Color(lv_obj_get_style_text_color(object,LV_PART_MAIN));opaque=lv_obj_get_style_text_opa(object,LV_PART_MAIN)==LV_OPA_COVER&&lv_obj_get_style_opa(object,LV_PART_MAIN)==LV_OPA_COVER;for(auto* p=object;p;p=lv_obj_get_parent(p)){if(lv_obj_get_style_opa(p,LV_PART_MAIN)!=LV_OPA_COVER){opaque=false;break;}if(lv_obj_get_style_bg_image_src(p,LV_PART_MAIN)||lv_obj_get_style_bg_grad_dir(p,LV_PART_MAIN)!=LV_GRAD_DIR_NONE){opaque=false;break;}if(lv_obj_get_style_bg_opa(p,LV_PART_MAIN)==LV_OPA_COVER){background=Color(lv_obj_get_style_bg_color(p,LV_PART_MAIN));break;}if(lv_obj_get_style_bg_opa(p,LV_PART_MAIN)!=LV_OPA_TRANSP){opaque=false;break;}}}
                    else{
                        std::set<int> visited;const json* painted=&r;int parent=r.value("par",-1);
                        for(;;){const auto paint=Styles(*painted);if(paint.value("opa",255u)!=255||paint.contains("bg_image_src")||paint.value("bg_grad_dir",0)!=0||paint.contains("bg_grad")){opaque=false;break;}if(const auto color=Number(paint,"bg_color")){const auto alpha=paint.value("bg_opa",255u);if(alpha==255){background=*color;break;}if(alpha!=0){opaque=false;break;}}
                            if(parent<0){const auto canvas=screens[si].value("canvas",screens[si].value("screen",json::object()));const auto paint=Styles(canvas);if(paint.value("bg_opa",255u)==255)background=Number(paint,"bg_color");break;}
                            if(!byId.count(parent)||!visited.insert(parent).second)break;painted=&byId[parent];parent=painted->value("par",-1);
                        }
                    }
                    double required=4.5;if(accessibility.contains("minimumContrast")&&accessibility["minimumContrast"].is_number())required=std::clamp(accessibility["minimumContrast"].get<double>(),1.0,21.0);
                    if(opaque&&foreground&&background){const auto ratio=ContrastRatio(*foreground,*background);if(ratio+0.01<required){std::ostringstream message;message.setf(std::ios::fixed);message.precision(2);message<<name<<" text contrast is "<<ratio<<":1; recommended minimum "<<required<<":1.";add("Contrast",si,oid,message.str());}}
                }
                const auto key=base.value("translationKey",std::string{});if(key.empty()||!locales.is_object())continue;
                for(const auto& locale:locales.items()){
                    if(!locale.value().is_object()||!locale.value().contains(key)||!locale.value()[key].is_string()){add("Translations",si,oid,name+" is missing '"+key+"' in locale "+locale.key()+".");continue;}
                    if(!object)continue;const auto translated=locale.value()[key].get<std::string>();auto* font=lv_obj_get_style_text_font(object,LV_PART_MAIN);if(!font)continue;
                    if(MissingGlyph(font,translated))add("Translations",si,oid,name+" has missing font glyphs in locale "+locale.key()+".");
                    const auto width=lv_obj_get_content_width(object),height=lv_obj_get_content_height(object);if(width<=0||height<=0)continue;
                    const bool oneLine=r.contains("button")||(r.contains("textarea")&&Object(r,"textarea").value("oneLine",false));lv_point_t needed{};
                    lv_text_get_size(&needed,translated.c_str(),font,lv_obj_get_style_text_letter_space(object,LV_PART_MAIN),lv_obj_get_style_text_line_space(object,LV_PART_MAIN),oneLine?LV_COORD_MAX:width,LV_TEXT_FLAG_NONE);
                    if(needed.x>width||needed.y>height)add("Translations",si,oid,name+" text for "+locale.key()+" needs "+std::to_string(needed.x)+" x "+std::to_string(needed.y)+" px; available content is "+std::to_string(width)+" x "+std::to_string(height)+" px.");
                }
            }
        }
    }catch(const std::exception& error){issues.push_back({Severity::Error,"Accessibility",0,-1,error.what()});}
    return issues;
}
std::string FixCaption(const json& document,const Issue& issue){try{
    auto copy=document;auto* record=Record(copy,issue.screen,issue.oid);
    if(issue.group=="Touch targets"&&record)return "Make touch target at least 44 px";
    if(issue.group=="Focus order"&&record)return "Restore natural focus order";
    if(issue.group=="Actions"&&record)for(const auto& action:(*record)["base"].value("events",json::array()))if(action.is_object()&&BrokenAction(document,issue.screen,action))return "Remove broken action references";
    if(issue.group=="Bindings"&&RemoveBrokenBinding(copy,issue))return "Remove broken widget binding";
    return {};
    }catch(...){return {};}
}
bool ApplyFix(json& document,const Issue& issue){
    const auto caption=FixCaption(document,issue);if(caption.empty())return false;
    auto* record=Record(document,issue.screen,issue.oid);
    if(issue.group=="Touch targets"&&record){auto& coords=(*record)["base"]["coords"];for(auto key:{"width","height"})coords[key]=(std::max)(44,coords.value(key,0));auto& styles=(*record)["base"]["nativeStyles"];if(!styles.is_array())styles=json::array();auto found=std::find_if(styles.begin(),styles.end(),[](const json& value){return value.is_object()&&value.value("selector",0u)==0;});if(found==styles.end()){styles.push_back({{"selector",0},{"values",json::object()}});found=std::prev(styles.end());}auto& values=(*found)["values"];values["min_width"]=44;values["min_height"]=44;for(auto key:{"width","height"})if(values.contains(key)&&values[key].is_number_integer()&&values[key].get<int>()>=0&&values[key].get<int>()<44)values[key]=44;SyncAlias(document,issue.screen);return true;}
    if(issue.group=="Focus order"&&record){(*record)["base"]["accessibility"].erase("tabIndex");SyncAlias(document,issue.screen);return true;}
    if(issue.group=="Actions"&&record){auto& events=(*record)["base"]["events"];const auto before=events.size();events.erase(std::remove_if(events.begin(),events.end(),[&](const json& action){return action.is_object()&&BrokenAction(document,issue.screen,action);}),events.end());SyncAlias(document,issue.screen);return events.size()!=before;}
    if(issue.group=="Bindings")return RemoveBrokenBinding(document,issue);
    return false;
}
}
