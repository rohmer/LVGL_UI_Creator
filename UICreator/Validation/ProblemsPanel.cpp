#include "ProblemsPanel.h"
#include "AccessibilityChecks.h"
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
namespace ProblemsPanel {
namespace {
using json=nlohmann::json;
struct Panel {
    PropertyWindow* properties=nullptr;Context context;
    lv_obj_t *root=nullptr,*tabs=nullptr,*summary=nullptr,*list=nullptr,*filter=nullptr,*focusList=nullptr,*focusSummary=nullptr,*focusIndex=nullptr,*textKey=nullptr,*locale=nullptr,*translation=nullptr,*translationStatus=nullptr;
    lv_timer_t* timer=nullptr;std::vector<ScreenValidation::Issue> issues;
    size_t issueLimit=200,editorScreen=SIZE_MAX;uint64_t observed=UINT64_MAX,analyzed=UINT64_MAX,analyses=0;uint32_t pendingSince=0;lv_obj_t* lastSelected=nullptr;
    ~Panel(){if(timer)lv_timer_delete(timer);}
};
std::map<lv_obj_t*,Panel*> panels;
json* Page(json& document,size_t screen){if(!document.contains("screens"))return &document;return document["screens"].is_array()&&screen<document["screens"].size()?&document["screens"][screen]:nullptr;}
size_t Current(const json& document){return document.value("editorScreen",document.value("activeScreen",size_t(0)));}
void SyncAlias(json& doc){if(doc.contains("screens")&&doc.contains("obj")&&Current(doc)<doc["screens"].size())doc["obj"]=doc["screens"][Current(doc)].value("obj",json::array());}
json* Selected(Panel* p,json& doc,int* saved=nullptr){auto* selected=p->properties->GetSelectedObject();const auto objects=p->properties->GetObjectTree()->GetObjects();const auto found=std::find(objects.begin(),objects.end(),selected);if(found==objects.end())return nullptr;auto* page=Page(doc,Current(doc));const auto index=size_t(std::distance(objects.begin(),found));if(!page||!page->contains("obj")||index>=(*page)["obj"].size())return nullptr;auto& record=(*page)["obj"][index];if(saved)*saved=record.value("oid",int(index));return &record;}
std::string Caption(const json& record,int id){return record.value("base",json::object()).value("name",std::string("Widget ")+std::to_string(id));}
void Focus(Panel* p,lv_obj_t* field){lv_obj_add_event_cb(field,[](lv_event_t* event){auto* panel=static_cast<Panel*>(lv_event_get_user_data(event));FocusEditorControl(panel->properties->GetKBGroup(),lv_event_get_target_obj(event));},LV_EVENT_CLICKED,p);}
lv_obj_t* Row(lv_obj_t* parent){auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_pad_column(row,8,0);lv_obj_set_scrollable(row,false);return row;}
lv_obj_t* Button(Panel* p,lv_obj_t* parent,const char* text,std::function<void()> action){auto* button=lv_button_create(parent);lv_obj_set_height(button,32);EditorStyle::Button(button);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);Focus(p,button);lv_obj_add_event_cb(button,[](lv_event_t* event){auto* function=static_cast<std::function<void()>*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE)delete function;else if(lv_event_get_code(event)==LV_EVENT_CLICKED){auto copy=*function;copy();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));return button;}
lv_obj_t* Field(Panel* p,lv_obj_t* parent,const char* label,const char* hint){auto* row=Row(parent);auto* caption=lv_label_create(row);lv_label_set_text(caption,label);lv_obj_set_width(caption,110);auto* field=lv_textarea_create(row);lv_obj_set_height(field,32);lv_obj_set_flex_grow(field,1);lv_textarea_set_one_line(field,true);lv_textarea_set_placeholder_text(field,hint);EditorStyle::Control(field);Focus(p,field);return field;}
void Analyze(Panel* p);
void SelectIssue(Panel* p,const ScreenValidation::Issue& issue){if(p->context.revision&&p->analyzed!=p->context.revision()){Analyze(p);lv_label_set_text(p->summary,"The document changed; findings were refreshed before selecting a widget.");return;}if(issue.oid<0){if(p->context.selectScreen)p->context.selectScreen(issue.screen);return;}if(p->context.selectObject){p->context.selectObject(issue.screen,issue.oid);return;}auto doc=p->context.document();if(Current(doc)!=issue.screen){if(p->context.selectScreen)p->context.selectScreen(issue.screen);else return;}const auto objects=p->properties->GetObjectTree()->GetObjects();auto* page=Page(doc,issue.screen);if(!page)return;size_t index=0;for(const auto& record:(*page)["obj"]){if(record.value("oid",int(index))==issue.oid&&index<objects.size()){p->properties->SetSelectedObject(objects[index]);lv_obj_scroll_to_view_recursive(objects[index],LV_ANIM_OFF);break;}++index;}}
void Analyze(Panel* p);
void DrawIssues(Panel* p){
    lv_obj_clean(p->list);size_t errors=0;for(const auto& issue:p->issues)if(issue.severity==ScreenValidation::Severity::Error)++errors;
    lv_label_set_text(p->summary,(std::to_string(errors)+" errors, "+std::to_string(p->issues.size()-errors)+" warnings. Changes are checked after a short pause.").c_str());
    const auto filter=lv_dropdown_get_selected(p->filter);if(p->issues.empty())EditorStyle::Help(p->list,"No problems found. Export and build validation still check the current document.");
    auto document=p->context.document();size_t shown=0,total=0;
    for(const auto& issue:p->issues){if(filter==1&&issue.severity!=ScreenValidation::Severity::Error)continue;if(filter==2&&issue.severity!=ScreenValidation::Severity::Warning)continue;++total;if(shown++>=p->issueLimit)continue;
        auto* row=lv_obj_create(p->list);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_style_pad_all(row,6,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_COLUMN);lv_obj_set_scrollable(row,false);
        auto* select=lv_button_create(row);lv_obj_set_size(select,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_style_bg_opa(select,LV_OPA_TRANSP,0);lv_obj_set_style_shadow_width(select,0,0);auto* text=lv_label_create(select);lv_obj_set_width(text,LV_PCT(100));lv_label_set_long_mode(text,LV_LABEL_LONG_MODE_WRAP);const auto caption=std::string(issue.severity==ScreenValidation::Severity::Error?"Error":"Warning")+" / "+issue.group+" / Screen "+std::to_string(issue.screen+1)+"\n"+issue.message;lv_label_set_text(text,caption.c_str());
        lv_obj_add_event_cb(select,[](lv_event_t* event){auto* action=static_cast<std::function<void()>*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE)delete action;else if(lv_event_get_code(event)==LV_EVENT_CLICKED){auto copy=*action;copy();}},LV_EVENT_ALL,new std::function<void()>([p,issue]{SelectIssue(p,issue);}));
        const auto fix=ScreenValidation::FixCaption(document,issue);if(!fix.empty()&&p->context.apply)Button(p,row,fix.c_str(),[p,issue]{try{if(p->context.revision&&p->analyzed!=p->context.revision()){Analyze(p);lv_label_set_text(p->summary,"The document changed; findings were refreshed. Review the quick fix again.");return;}auto changed=p->context.document();if(ScreenValidation::ApplyFix(changed,issue))p->context.apply(std::move(changed));Analyze(p);}catch(const std::exception& error){lv_label_set_text(p->summary,error.what());}});
    }
    if(total>p->issueLimit)Button(p,p->list,"Show 200 more findings",[p]{p->issueLimit+=200;DrawIssues(p);});
}
void SelectionChanged(Panel* p){
    p->lastSelected=p->properties->GetSelectedObject();auto doc=p->context.document();int id=-1;const auto* selected=Selected(p,doc,&id);
    if(!selected){lv_label_set_text(p->focusSummary,"Select an authored widget to edit its keyboard focus index or translation key.");return;}
    const auto base=selected->value("base",json::object());const auto accessibility=base.value("accessibility",json::object());const auto index=accessibility.is_object()&&accessibility.contains("tabIndex")&&accessibility["tabIndex"].is_number_integer()?accessibility["tabIndex"].get<int>():-2;
    lv_label_set_text(p->focusSummary,(Caption(*selected,id)+" / natural order unless an explicit index is set").c_str());lv_textarea_set_text(p->focusIndex,index==-2?"":std::to_string(index).c_str());lv_textarea_set_text(p->textKey,base.value("translationKey",std::string{}).c_str());
}
void FocusList(Panel* p){
    lv_obj_clean(p->focusList);auto doc=p->context.document();auto* page=Page(doc,Current(doc));if(!page||!page->contains("obj"))return;
    struct Entry{int oid,index;std::string name;};std::vector<Entry> entries;int next=0;
    for(const auto& record:(*page)["obj"]){const int oid=record.value("oid",next++);bool control=false;for(auto key:{"button","textarea","textBox","cb","dropdown","roller","slider","spinbox","switch","btnm","cal","advancedKeyboard"})if(record.contains(key))control=true;if(!control)continue;const auto a=record.value("base",json::object()).value("accessibility",json::object());entries.push_back({oid,a.is_object()&&a.contains("tabIndex")&&a["tabIndex"].is_number_integer()?a["tabIndex"].get<int>():INT32_MAX,Caption(record,oid)});}
    std::stable_sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.index<b.index;});
    size_t shown=0;for(const auto& entry:entries){if(shown++>=200){EditorStyle::Help(p->focusList,"Select additional controls from Hierarchy to edit their focus index.");break;}const auto caption=(entry.index==-1?"Skip":entry.index==INT32_MAX?"Natural":std::to_string(entry.index))+std::string(" / ")+entry.name;Button(p,p->focusList,caption.c_str(),[p,entry]{const auto doc=p->context.document();SelectIssue(p,{ScreenValidation::Severity::Warning,"Focus order",Current(doc),entry.oid,{}});SelectionChanged(p);});}
}
void Analyze(Panel* p){try{
    auto document=p->context.document();p->editorScreen=Current(document);p->issues=ScreenValidation::Analyze(document,p->properties->GetObjectTree()->GetObjects());p->analyzed=p->context.revision?p->context.revision():p->analyses;p->observed=p->analyzed;p->pendingSince=0;++p->analyses;DrawIssues(p);FocusList(p);SelectionChanged(p);
    }catch(const std::exception& error){p->pendingSince=0;lv_label_set_text(p->summary,error.what());}
}
void CommitFocus(Panel* p,bool natural){try{auto doc=p->context.document();auto* record=Selected(p,doc);if(!record)throw std::runtime_error("Select a widget first");auto& a=(*record)["base"]["accessibility"];if(!a.is_object())a=json::object();if(natural)a.erase("tabIndex");else{const auto text=std::string(lv_textarea_get_text(p->focusIndex));size_t used=0;const int index=std::stoi(text,&used);if(used!=text.size()||index < -1||index>1000000)throw std::runtime_error("Use -1 to skip or a focus index from 0 to 1000000");a["tabIndex"]=index;}SyncAlias(doc);p->context.apply(std::move(doc));Analyze(p);}catch(const std::exception& error){lv_label_set_text(p->focusSummary,error.what());}}
void LoadTranslation(Panel* p){try{const auto doc=p->context.document();const auto locale=std::string(lv_textarea_get_text(p->locale)),key=std::string(lv_textarea_get_text(p->textKey));const auto table=doc.value("translations",json::object()).value("locales",json::object());if(table.contains(locale)&&table[locale].is_object()&&table[locale].contains(key)&&table[locale][key].is_string()){lv_textarea_set_text(p->translation,table[locale][key].get<std::string>().c_str());lv_label_set_text(p->translationStatus,"Loaded the saved text. Editing this field does not change the project until Save.");}else lv_label_set_text(p->translationStatus,"No saved value for that key and locale.");}catch(const std::exception& error){lv_label_set_text(p->translationStatus,error.what());}}
void Translation(Panel* p,bool assign){try{
    auto doc=p->context.document();const auto key=std::string(lv_textarea_get_text(p->textKey));const auto locale=std::string(lv_textarea_get_text(p->locale));if(key.empty()||key.size()>160||locale.empty()||locale.size()>32)throw std::runtime_error("Enter a text key and locale code");
    auto& translations=doc["translations"];if(!translations.is_object())translations=json::object();if(!translations.contains("defaultLocale"))translations["defaultLocale"]="en";
    translations["locales"][locale][key]=lv_textarea_get_text(p->translation);
    if(assign){auto* record=Selected(p,doc);if(!record)throw std::runtime_error("Select a widget to assign this text key");(*record)["base"]["translationKey"]=key;SyncAlias(doc);p->context.apply(std::move(doc));}
    else if(p->context.applyMetadata)p->context.applyMetadata({{"translations",translations}});else p->context.apply(std::move(doc));
    Analyze(p);lv_label_set_text(p->translationStatus,"Translation saved. Problems checks missing values and live text fit; authored text stays unchanged.");
    }catch(const std::exception& error){lv_label_set_text(p->translationStatus,error.what());}}
}
lv_obj_t* Create(lv_obj_t* parent,PropertyWindow* properties,Context context){
    auto* p=new Panel;p->properties=properties;p->context=std::move(context);p->root=EditorWindow::Create(parent);EditorWindow::SetTitle(p->root,"Problems");lv_obj_set_size(p->root,600,240);panels[p->root]=p;EditorWindow::SetCloseHandler(p->root,[p]{lv_obj_set_hidden(p->root,true);});
    lv_obj_add_event_cb(p->root,[](lv_event_t* event){auto* p=static_cast<Panel*>(lv_event_get_user_data(event));panels.erase(p->root);delete p;},LV_EVENT_DELETE,p);
    auto* content=EditorWindow::Content(p->root);lv_obj_set_scrollable(content,false);p->tabs=lv_tabview_create(content);lv_obj_set_size(p->tabs,LV_PCT(100),LV_PCT(100));lv_tabview_set_tab_bar_size(p->tabs,32);
    auto* problems=lv_tabview_add_tab(p->tabs,"Problems");auto* focus=lv_tabview_add_tab(p->tabs,"Focus order");auto* translations=lv_tabview_add_tab(p->tabs,"Translations");for(auto* tab:{problems,focus,translations}){lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(tab,8,0);lv_obj_set_style_pad_row(tab,6,0);}
    auto* toolbar=Row(problems);p->summary=lv_label_create(toolbar);lv_obj_set_flex_grow(p->summary,1);lv_label_set_long_mode(p->summary,LV_LABEL_LONG_MODE_WRAP);p->filter=lv_dropdown_create(toolbar);lv_dropdown_set_options(p->filter,"All findings\nErrors\nWarnings");BindEditorControl(properties->GetKBGroup(),p->filter);lv_obj_add_event_cb(p->filter,[](lv_event_t* event){DrawIssues(static_cast<Panel*>(lv_event_get_user_data(event)));},LV_EVENT_VALUE_CHANGED,p);Button(p,toolbar,"Check now",[p]{Analyze(p);});
    p->list=lv_obj_create(problems);lv_obj_set_size(p->list,LV_PCT(100),0);lv_obj_set_flex_grow(p->list,1);lv_obj_set_flex_flow(p->list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(p->list,0,0);lv_obj_set_style_border_width(p->list,0,0);
    p->focusSummary=EditorStyle::Help(focus,"Select a widget");p->focusIndex=Field(p,focus,"Focus index","-1 skips; 0 and above set explicit order");auto* actions=Row(focus);Button(p,actions,"Set focus index",[p]{CommitFocus(p,false);});Button(p,actions,"Natural order",[p]{CommitFocus(p,true);});p->focusList=lv_obj_create(focus);lv_obj_set_size(p->focusList,LV_PCT(100),0);lv_obj_set_flex_grow(p->focusList,1);lv_obj_set_flex_flow(p->focusList,LV_FLEX_FLOW_COLUMN);
    EditorStyle::Help(translations,"Store text variants and check fit without replacing authored text. Select a widget, enter its key, locale and translated text.");p->textKey=Field(p,translations,"Text key","For example: settings.title");p->locale=Field(p,translations,"Locale","For example: en, fr, es, ar");lv_textarea_set_text(p->locale,"en");p->translation=Field(p,translations,"Translation","Text to check at this widget's actual font and dimensions");auto* translationActions=Row(translations);Button(p,translationActions,"Load saved text",[p]{LoadTranslation(p);});Button(p,translationActions,"Assign key and save",[p]{Translation(p,true);});Button(p,translationActions,"Save translation",[p]{Translation(p,false);});p->translationStatus=EditorStyle::Help(translations,"Translations are included in authoring JSON. Existing text is used as the fallback.");
    Analyze(p);p->timer=lv_timer_create([](lv_timer_t* timer){auto* p=static_cast<Panel*>(lv_timer_get_user_data(timer));if(lv_obj_is_hidden(p->root))return;if(p->properties->GetSelectedObject()!=p->lastSelected)SelectionChanged(p);if(p->context.activeScreen&&p->editorScreen!=p->context.activeScreen()){p->editorScreen=p->context.activeScreen();p->pendingSince=lv_tick_get();}if(!p->context.revision)return;const auto revision=p->context.revision();if(revision!=p->observed){p->observed=revision;p->pendingSince=lv_tick_get();}if(p->pendingSince&&uint32_t(lv_tick_get()-p->pendingSince)>=250)Analyze(p);},100,p);return p->root;
}
void Close(lv_obj_t* panel){if(panel){if(panels.count(panel))lv_obj_delete(panel);return;}while(!panels.empty())lv_obj_delete(panels.begin()->first);}
void Refresh(lv_obj_t* panel){if(panel){if(panels.count(panel))Analyze(panels.at(panel));return;}for(const auto& p:panels)Analyze(p.second);}
size_t IssueCount(lv_obj_t* panel){if(panel)return panels.count(panel)?panels.at(panel)->issues.size():0;size_t count=0;for(const auto& p:panels)count+=p.second->issues.size();return count;}
uint64_t AnalysisCount(lv_obj_t* panel){if(panel)return panels.count(panel)?panels.at(panel)->analyses:0;uint64_t count=0;for(const auto& p:panels)count+=p.second->analyses;return count;}
}
