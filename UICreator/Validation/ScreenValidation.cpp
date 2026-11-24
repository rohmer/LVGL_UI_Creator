#include "ScreenValidation.h"
#include "AccessibilityChecks.h"
#include "../Windows/PropertyWindow.h"
#include "../Widgets/TreeView.h"
#include "../Utils/EditorWindow.h"
#include "../../LVSerial/Serialization/InteractionActions.h"
#include "../../LVSerial/Serialization/LVVariable.h"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace ScreenValidation {
namespace {
using json=nlohmann::json;
const json& Section(const json& value,const char* key){static const json empty=json::object();auto i=value.find(key);return i!=value.end()&&i->is_object()?*i:empty;}
std::string Kind(const json& record){for(auto& item:record.items())if(item.key()!="base"&&item.value().is_object()&&item.key()!="design")return item.key();return "object";}
bool Interactive(const json& r){static const std::set<std::string> controls={"btn","button","textarea","checkbox","dropdown","roller","slider","arc","spinbox","switch","btnm","buttonmatrix","keyboard","advancedKeyboard","textBox","list","calendar"};return controls.count(Kind(r))||!Section(r,"base").value("events",json::array()).empty();}
bool Hidden(const json& r){auto& base=Section(r,"base");return base.value("hidden",false)||Section(base,"flags").value("hidden",false);}
int Integer(const json& value,const char* key,int fallback=0){auto i=value.find(key);return i!=value.end()&&i->is_number_integer()?i->get<int>():fallback;}
struct Box{int x=0,y=0,w=0,h=0;bool visible=true;};
bool Outside(const Box& c,const Box& p){return c.x<p.x||c.y<p.y||c.x+c.w>p.x+p.w||c.y+c.h>p.y+p.h;}
uint32_t NextCodepoint(const std::string& text,size_t& i){auto c=static_cast<unsigned char>(text[i++]);if(c<128)return c;int n=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:(c&0xf8)==0xf0?3:-1;if(n<0)return 0xfffd;uint32_t cp=c&((1u<<(6-n))-1);for(int j=0;j<n;++j){if(i>=text.size()||(static_cast<unsigned char>(text[i])&0xc0)!=0x80)return 0xfffd;cp=(cp<<6)|(static_cast<unsigned char>(text[i++])&0x3f);}return cp;}
std::string Text(const json& record){std::string result;if(record.contains("text")&&record.at("text").is_string())result=record.at("text").get<std::string>()+'\n';for(auto& part:record.items())if(part.value().is_object())for(auto key:{"text","placeholder","options","title"}){auto i=part.value().find(key);if(i!=part.value().end()&&i->is_string()){result+=i->get<std::string>();result+='\n';}}return result;}
std::string Name(const json& record,int oid){return Section(record,"base").value("name",std::string("Widget ")+std::to_string(oid));}
}
std::vector<Issue> Analyze(const json& document,const std::vector<lv_obj_t*>& live) {
    std::vector<Issue> result;
    auto add=[&](Severity severity,const char* group,size_t screen,int oid,std::string message){result.push_back({severity,group,screen,oid,std::move(message)});};
    try {
        auto screens=document.contains("screens")?document.at("screens"):json::array({document});
        std::vector<std::map<int,json>> records(screens.size());std::map<std::string,json> variables;
        const auto variableRecords=document.value("variables",json::array());
        if(!variableRecords.is_array())throw std::invalid_argument("Variables must be an array.");
        bool validVariables=true;
        for(const auto& record:variableRecords)try{
            const auto variable=Serialization::LVVariable::FromJSON(record);
            if(variable.name.empty()||!variables.emplace(variable.name,record).second){add(Severity::Error,"Bindings",0,-1,"Variables need unique, nonempty names.");validVariables=false;}
        }catch(const std::exception& error){add(Severity::Error,"Bindings",0,-1,error.what());validVariables=false;}
        if(validVariables)try{Serialization::LVVariable::NormalizeList(variableRecords);}
        catch(const std::exception& error){add(Severity::Error,"Bindings",0,-1,error.what());}
        std::map<std::string,std::set<std::string>> assets;
        for(auto catalog:{"fonts","images","tries"})for(const auto& a:document.value(catalog,json::array()))assets[catalog].insert(a.value("id",std::string{}));
        assets["tries"].insert("builtin-en-us");
        for(size_t si=0;si<screens.size();++si){int next=0;for(const auto& r:screens[si].value("obj",json::array())){int oid=r.value("oid",next++);if(oid<0||!records[si].emplace(oid,r).second)add(Severity::Error,"Structure",si,oid,"Duplicate or negative widget ID.");}}
        auto targetExists=[&](size_t si,int oid){return si<records.size()&&records[si].count(oid);};
        for(const auto& pair:variables){
            const auto variable=Serialization::LVVariable::FromJSON(pair.second);
            const auto bindings=Serialization::LVVariable::Bindings(variable);
            for(size_t index=0;index<bindings.size();++index){
                const auto& binding=bindings[index];
                if(!targetExists(binding.screen,binding.objectId)){
                    result.push_back({Severity::Error,"Bindings",binding.screen,binding.objectId,
                        "Variable '"+pair.first+"' references a missing widget (connection "+std::to_string(index+1)+").",pair.first,static_cast<int>(index)});
                    continue;
                }
                try{Serialization::LVVariable::ValidateBinding(variable,binding,records[binding.screen].at(binding.objectId));}
                catch(const std::exception& error){add(Severity::Error,"Bindings",binding.screen,binding.objectId,
                    "Variable '"+pair.first+"' connection "+std::to_string(index+1)+": "+error.what());}
            }
        }
        const auto current=document.value("editorScreen",document.value("activeScreen",size_t(0)));
        for(size_t si=0;si<screens.size();++si) {
            std::map<int,lv_obj_t*> objects;if(si==current){size_t i=0;for(const auto& r:screens[si].value("obj",json::array())){if(i<live.size())objects[r.value("oid",int(i))]=live[i];++i;}}
            std::map<int,Box> boxes;std::set<int> resolving;
            auto canvas=Section(screens[si],"canvas");if(canvas.empty())canvas=Section(screens[si],"screen");Box root{0,0,canvas.value("width",320),canvas.value("height",240),true};
            auto box=[&](auto&& self,int oid)->Box {if(boxes.count(oid))return boxes.at(oid);if(!records[si].count(oid)||!resolving.insert(oid).second)return root;const auto& r=records[si].at(oid);const int par=r.value("par",-1);auto parent=par>=0?self(self,par):root;auto& c=Section(Section(r,"base"),"coords");Box b{parent.x+Integer(c,"x"),parent.y+Integer(c,"y"),Integer(c,"width"),Integer(c,"height"),parent.visible&&!Hidden(r)};
                auto found=objects.find(oid);if(found!=objects.end()&&found->second){auto* o=found->second;lv_obj_update_layout(o);lv_area_t area;lv_obj_get_coords(o,&area);b={area.x1,area.y1,area.x2-area.x1+1,area.y2-area.y1+1,!lv_obj_is_hidden(o)};for(auto* p=lv_obj_get_parent(o);p;p=lv_obj_get_parent(p))if(lv_obj_is_hidden(p))b.visible=false;}
                resolving.erase(oid);boxes[oid]=b;return b;};
            for(const auto& pair:records[si]) {
                const int oid=pair.first;const auto& r=pair.second;const auto name=Name(r,oid);const int par=r.value("par",-1);
                if(par>=0&&!records[si].count(par))add(Severity::Error,"Structure",si,oid,name+" has a missing parent.");
                std::set<int> ancestry{oid};for(int ancestor=par;ancestor>=0&&records[si].count(ancestor);ancestor=records[si].at(ancestor).value("par",-1))if(!ancestry.insert(ancestor).second){add(Severity::Error,"Structure",si,oid,name+" has a parent cycle.");break;}
                const auto b=box(box,oid);
                if(b.visible&&Interactive(r)&&b.w>0&&b.h>0&&(b.w<44||b.h<44))add(Severity::Warning,"Touch targets",si,oid,name+" is "+std::to_string(b.w)+" x "+std::to_string(b.h)+" px; use at least 44 x 44 px for touch.");
                Box clip=par>=0?box(box,par):root;bool scroll=false,overflow=false;
                if(par>=0&&records[si].count(par)){const auto& parent=Section(records[si].at(par),"base");scroll=Section(parent,"scroll").value("enabled",false)||Section(parent,"flags").value("scrollable",false);overflow=Section(parent,"flags").value("overflow_visible",false);}
                if(objects.count(oid)&&objects.at(oid)){auto* o=objects.at(oid);auto* parent=lv_obj_get_parent(o);if(parent){lv_area_t area;lv_obj_get_content_coords(parent,&area);clip={area.x1,area.y1,area.x2-area.x1+1,area.y2-area.y1+1,true};scroll=lv_obj_is_scrollable(parent);overflow=lv_obj_is_overflow_visible(parent);}}
                if(b.visible&&b.w>0&&b.h>0&&!scroll&&!overflow&&Outside(b,clip))add(Severity::Warning,"Layout",si,oid,name+" extends beyond its visible parent and may be clipped.");
                const auto& base=Section(r,"base");for(auto field:{std::make_pair("fontId","fonts"),std::make_pair("imageId","images")})if(base.contains(field.first)&&base.at(field.first).is_string()&&!assets[field.second].count(base.at(field.first)))add(Severity::Error,"Assets",si,oid,name+" references a missing "+field.first+" asset.");
                if(r.contains("advancedKeyboard")){auto trie=r.at("advancedKeyboard").value("trieId",std::string("builtin-en-us"));if(!trie.empty()&&!assets["tries"].count(trie))add(Severity::Error,"Assets",si,oid,name+" references a missing language trie.");auto target=r.at("advancedKeyboard").value("targetOid",-1);if(target>=0&&!targetExists(si,target))add(Severity::Error,"Bindings",si,oid,name+" has a missing keyboard text box target.");}
                try{for(const auto& a:Serialization::InteractionActions::Normalize(base.value("events",json::array()))) {
                    const auto action=a.at("action").get<std::string>();
                    if(action=="navigate"&&a.at("screen").get<size_t>()>=screens.size())add(Severity::Error,"Actions",si,oid,name+" navigates to a missing screen.");
                    if(a.contains("targetOid")){int target=a.at("targetOid");size_t ts=a.value("targetScreen",si);if(target==-1)target=oid;if(!targetExists(ts,target))add(Severity::Error,"Actions",si,oid,name+" has a broken action target.");else if(action=="validate"){auto type=Kind(records[ts].at(target));if(type!="textarea"&&type!="textBox")add(Severity::Error,"Actions",si,oid,name+" validates a widget that is not a text box.");}}
                    for(const auto& condition:{a,a.value("condition",json::object())})if(condition.contains("variable")&&!variables.count(condition.at("variable")))add(Severity::Error,"Actions",si,oid,name+" references a missing action variable.");
                    if(a.contains("variable")&&variables.count(a.at("variable"))){auto v=Serialization::LVVariable::FromJSON(variables.at(a.at("variable")));if(action=="set_variable"){v.defaultValue=a.at("value");v.values=json::array();Serialization::LVVariable::ToJSON(v);}else if(action=="increment_variable"&&(v.type==Serialization::VariableType::String||v.type==Serialization::VariableType::Boolean))add(Severity::Error,"Actions",si,oid,name+" increments a variable that is not numeric.");}
                }}catch(const std::exception& e){add(Severity::Error,"Actions",si,oid,name+": "+e.what());}
                if(objects.count(oid)&&objects.at(oid)) {auto* font=lv_obj_get_style_text_font(objects.at(oid),LV_PART_MAIN);std::set<uint32_t> missing;auto text=Text(r);for(size_t i=0;i<text.size();){auto cp=NextCodepoint(text,i);if(cp<32||cp==0x200d||(cp>=0xfe00&&cp<=0xfe0f))continue;lv_font_glyph_dsc_t glyph{};if(font&&(!lv_font_get_glyph_dsc(font,&glyph,cp,0)||glyph.is_placeholder))missing.insert(cp);}if(!missing.empty()){std::ostringstream out;out<<name<<" has missing font glyphs:";size_t n=0;for(auto cp:missing){if(n++==6){out<<" ...";break;}out<<" U+"<<std::hex<<std::uppercase<<cp;}add(Severity::Warning,"Typography",si,oid,out.str());}}
            }
            // A spatial index keeps long forms and large grids linear in the
            // usual case. Report one useful overlap per widget, rather than
            // allocating thousands of duplicate findings for a stacked group.
            struct Hit {int oid;Box bounds;};
            struct Bucket {std::map<std::pair<int64_t,int64_t>,std::vector<Hit>> cells;std::vector<Hit> large,all;};
            std::map<std::string,Bucket> groups;std::set<int> overlapReported;
            auto cell=[](int64_t value){return value>=0?value/64:(value-63)/64;};
            for(const auto& entry:records[si]) {
                const auto& record=entry.second;if(!Interactive(record))continue;const auto bounds=box(box,entry.first);if(!bounds.visible||bounds.w<=0||bounds.h<=0)continue;
                const auto key=std::to_string(record.value("par",-1))+":"+record.value("hostPath",json::array()).dump();auto& bucket=groups[key];
                const auto x1=cell(bounds.x),x2=cell(int64_t(bounds.x)+bounds.w-1),y1=cell(bounds.y),y2=cell(int64_t(bounds.y)+bounds.h-1);
                const bool large=(x2-x1+1)>32||(y2-y1+1)>32||(x2-x1+1)*(y2-y1+1)>256;
                std::vector<Hit> candidates=bucket.large;std::set<int> seen;for(const auto& hit:candidates)seen.insert(hit.oid);
                if(large)candidates=bucket.all;
                else for(auto x=x1;x<=x2;++x)for(auto y=y1;y<=y2;++y){const auto found=bucket.cells.find({x,y});if(found!=bucket.cells.end())for(const auto& hit:found->second)if(seen.insert(hit.oid).second)candidates.push_back(hit);}
                for(const auto& other:candidates){const auto& a=other.bounds;const int64_t width=std::max<int64_t>(0,std::min(int64_t(a.x)+a.w,int64_t(bounds.x)+bounds.w)-std::max(a.x,bounds.x));const int64_t height=std::max<int64_t>(0,std::min(int64_t(a.y)+a.h,int64_t(bounds.y)+bounds.h)-std::max(a.y,bounds.y));const auto area=width*height;if(area<=0||area*5<std::min(int64_t(a.w)*a.h,int64_t(bounds.w)*bounds.h))continue;
                    const int reported=overlapReported.count(other.oid)?entry.first:other.oid;const int sibling=reported==entry.first?other.oid:entry.first;
                    if(overlapReported.insert(reported).second)add(Severity::Warning,"Overlap",si,reported,Name(records[si].at(reported),reported)+" overlaps sibling control '"+Name(records[si].at(sibling),sibling)+"'.");break;
                }
                const Hit hit{entry.first,bounds};bucket.all.push_back(hit);if(large)bucket.large.push_back(hit);else for(auto x=x1;x<=x2;++x)for(auto y=y1;y<=y2;++y)bucket.cells[{x,y}].push_back(hit);
            }
        }
    } catch(const std::exception& e){add(Severity::Error,"Document",0,-1,e.what());}
    auto accessibility=AccessibilityIssues(document,live);result.insert(result.end(),accessibility.begin(),accessibility.end());return result;
}
std::string BlockingErrors(const json& document,const std::vector<lv_obj_t*>& objects){
    std::string result;size_t count=0;for(const auto& issue:Analyze(document,objects))if(issue.severity==Severity::Error){if(count++>=8){result+="\nMore errors are listed in Problems.";break;}if(!result.empty())result+='\n';result+="Screen "+std::to_string(issue.screen+1)+": "+issue.message;}return result;
}
namespace {
struct Window{PropertyWindow* properties;std::function<json()> document;lv_obj_t *root,*list,*summary;};Window* active=nullptr;
void Refresh(Window* w){auto doc=w->document();auto issues=Analyze(doc,w->properties->GetObjectTree()->GetObjects());lv_obj_clean(w->list);size_t errors=0;for(const auto& issue:issues)if(issue.severity==Severity::Error)++errors;const auto summary=std::to_string(errors)+" errors, "+std::to_string(issues.size()-errors)+" warnings. Click a finding to select its widget.";lv_label_set_text(w->summary,summary.c_str());if(issues.empty()){auto* label=lv_label_create(w->list);lv_label_set_text(label,"No issues found on this screen.");}
    for(const auto& issue:issues){auto* button=lv_button_create(w->list);lv_obj_set_width(button,LV_PCT(100));lv_obj_set_height(button,LV_SIZE_CONTENT);auto* label=lv_label_create(button);lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);const auto text=std::string(issue.severity==Severity::Error?"Error":"Warning")+" / "+issue.group+" / Screen "+std::to_string(issue.screen+1)+"\n"+issue.message;lv_label_set_text(label,text.c_str());auto* action=new std::function<void()>([w,issue]{auto doc=w->document();if(issue.screen!=doc.value("editorScreen",doc.value("activeScreen",size_t(0)))){lv_label_set_text(w->summary,"Switch to the indicated screen, then run validation again to select this widget.");return;}auto records=doc.contains("screens")?doc["screens"][issue.screen].value("obj",json::array()):doc.value("obj",json::array());auto objects=w->properties->GetObjectTree()->GetObjects();for(size_t i=0;i<records.size()&&i<objects.size();++i)if(records[i].value("oid",int(i))==issue.oid){w->properties->SetSelectedObject(objects[i]);lv_obj_scroll_to_view_recursive(objects[i],LV_ANIM_ON);break;}});lv_obj_add_event_cb(button,[](lv_event_t* e){auto* action=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete action;else if(lv_event_get_code(e)==LV_EVENT_CLICKED)(*action)();},LV_EVENT_ALL,action);}
}
}
void Close(){if(active)lv_obj_delete(active->root);}
void Open(PropertyWindow* properties,std::function<json()> document){if(active){lv_obj_move_foreground(active->root);Refresh(active);return;}auto* w=new Window{properties,std::move(document),nullptr,nullptr,nullptr};w->root=EditorWindow::Create(lv_layer_top());active=w;EditorWindow::SetTitle(w->root,"Validate screens");EditorWindow::SetCloseHandler(w->root,[]{Close();});lv_obj_set_size(w->root,std::min(820,lv_display_get_horizontal_resolution(nullptr)-40),std::min(820,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);auto* body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,12,0);w->summary=lv_label_create(body);lv_obj_set_width(w->summary,LV_PCT(100));lv_label_set_long_mode(w->summary,LV_LABEL_LONG_WRAP);auto* rerun=lv_button_create(body);auto* l=lv_label_create(rerun);lv_label_set_text(l,"Run validation again");lv_obj_add_event_cb(rerun,[](lv_event_t* e){Refresh(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_CLICKED,w);w->list=lv_obj_create(body);lv_obj_set_width(w->list,LV_PCT(100));lv_obj_set_flex_grow(w->list,1);lv_obj_set_flex_flow(w->list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(w->list,0,0);lv_obj_add_event_cb(w->root,[](lv_event_t* e){delete static_cast<Window*>(lv_event_get_user_data(e));active=nullptr;},LV_EVENT_DELETE,w);Refresh(w);}
}
