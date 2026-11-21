#include "ModelPlayground.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorSession.h"
#include "../../LVSerial/Widgets/AdvancedKeyboard/SuggestionText.h"
#include "../../LVSerial/Widgets/AdvancedKeyboard/Utf8Fold.h"
#include <Serialization/Base64.h>
#include <Serialization/ProjectTries.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace ModelPlayground {
Evaluation Evaluate(lv_obj_t* textarea, lv_advanced_keyboard_predictor_t* predictor, double threshold)
{
    Evaluation result;
    if (!textarea || !predictor) return result;
    const auto text = espos::keyboard::detail::capture_suggestion_text(textarea);
    result.prefix.assign(text.prefix); result.previous.assign(text.previous); result.older.assign(text.older);
    std::array<char, 768> a{}, b{}, c{};
    std::string_view prefix, previous, older;
    if (!espos::keyboard::prediction::FoldUtf8(text.prefix, a.data(), a.size(), prefix)) return result;
    espos::keyboard::prediction::FoldUtf8(text.previous, b.data(), b.size(), previous);
    espos::keyboard::prediction::FoldUtf8(text.older, c.data(), c.size(), older);
    lv_advanced_keyboard_suggestion_t suggestions[3]{};
    const auto count = lv_advanced_keyboard_predictor_suggest_folded(predictor, prefix.data(), prefix.size(),
        previous.data(), previous.size(), older.data(), older.size(), suggestions, 3);
    for (size_t index = 0; index < count; ++index) result.suggestions.emplace_back(suggestions[index].text, suggestions[index].length);
    lv_advanced_keyboard_suggestion_t correction{};
    if (!text.prefix.empty() && lv_advanced_keyboard_predictor_autocorrect_utf8(predictor, text.prefix.data(), text.prefix.size(),
        text.previous.data(), text.previous.size(), text.older.data(), text.older.size(), 0, &correction, &result.confidence)) {
        result.correction.assign(correction.text, correction.length);
        result.accepted = result.confidence >= threshold;
    }
    return result;
}
namespace {
struct DeletePredictor { void operator()(lv_advanced_keyboard_predictor_t* p) const { lv_advanced_keyboard_predictor_destroy(p); } };
struct Model {
    std::string id, name;
    std::vector<uint8_t> bytes, index;
    std::unique_ptr<lv_advanced_keyboard_predictor_t, DeletePredictor> predictor;
    size_t trieBytes = 0, indexBytes = 0;
};
struct Window;
struct Column { Window* window = nullptr; lv_obj_t *choice=nullptr,*result=nullptr,*metrics=nullptr,*load=nullptr; Model model; };
struct Window {
    PropertyWindow* properties = nullptr;
    lv_obj_t *root=nullptr,*sample=nullptr,*threshold=nullptr,*context=nullptr,*status=nullptr;
    lv_timer_t* timer=nullptr;
    json records;
    std::vector<std::string> ids;
    std::array<Column,2> columns;
    bool dirty=true;
    uint32_t caret=0;
    ~Window() { if(timer)lv_timer_delete(timer); }
};
Window* active=nullptr;
bool shutdown=false;
std::string CleanName(std::string text) { for(auto& c:text)if(c=='\r'||c=='\n'||c=='\t')c=' ';return text; }
lv_obj_t* Row(lv_obj_t* parent) {
    auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);
    lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_pad_column(row,10,0);lv_obj_set_scrollable(row,false);return row;
}
void Focus(Window* w,lv_obj_t* field) {
    lv_obj_add_event_cb(field,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));FocusEditorControl(w->properties->GetKBGroup(),lv_event_get_target_obj(e));},LV_EVENT_CLICKED,w);
}
lv_obj_t* Button(Window* w,lv_obj_t* parent,const char* caption,std::function<void()> action,int width=150) {
    auto* b=lv_button_create(parent);lv_obj_set_size(b,width,36);EditorStyle::Button(b);
    auto* t=lv_label_create(b);lv_label_set_text(t,caption);lv_obj_center(t);
    lv_obj_add_event_cb(b,[](lv_event_t* e){auto* f=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete f;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto action=*f;action();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));Focus(w,b);return b;
}
void Status(Window* w,const std::string& text) {lv_label_set_text(w->status,text.c_str());}
std::filesystem::path Pick() {
#ifdef _WIN32
    wchar_t path[32768]{};OPENFILENAMEW d{};d.lStructSize=sizeof d;d.hwndOwner=GetActiveWindow();d.lpstrFile=path;d.nMaxFile=32768;
    d.lpstrFilter=L"Compact language model (*.ngtr)\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0";
    d.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST;d.lpstrTitle=L"Try a language model";
    if(GetOpenFileNameW(&d))return path;
#else
    return NativeFileDialogUI::PickOne(false,false,L"Try a language model",L"Compact language models\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0");
#endif
    return {};
}
std::vector<uint8_t> Bytes(const json& value) {
    if(value.is_binary())return {value.get_binary().begin(),value.get_binary().end()};
    const auto decoded=base64_decode(value.get<std::string>());return {decoded.begin(),decoded.end()};
}
void SetModel(Column& column,const std::string& id,const std::vector<uint8_t>* external=nullptr,const std::string& name={}) {
    Model next;next.id=id;next.name=name;
    if(id=="builtin-en-us") {
        const void *raw=nullptr,*index=nullptr;size_t rawBytes=0,indexBytes=0;
        if(!lv_advanced_keyboard_get_builtin_trie(&raw,&rawBytes,&index,&indexBytes))throw std::runtime_error("Bundled English is unavailable in this build");
        next.predictor.reset(lv_advanced_keyboard_predictor_create_builtin());next.trieBytes=rawBytes;next.indexBytes=indexBytes;next.name="Bundled English";
    } else if(!id.empty()) {
        if(external) {next.bytes=*external;next.name=name;}
        else for(const auto& record:column.window->records)if(record.at("id")==id){next.bytes=Bytes(record.at("data"));if(record.contains("indexData"))next.index=Bytes(record.at("indexData"));next.name=record.value("name",id);break;}
        if(next.bytes.empty())throw std::runtime_error("Choose a valid model");
        if(next.index.empty()) {
            void* data=nullptr;size_t size=0;
            if(!lv_advanced_keyboard_build_trie_index(next.bytes.data(),next.bytes.size(),&data,&size))throw std::runtime_error("Model must be a valid compact NGTR v3 trie");
            try {next.index.assign(static_cast<uint8_t*>(data),static_cast<uint8_t*>(data)+size);}catch(...){lv_advanced_keyboard_free_trie_index(data);throw;}
            lv_advanced_keyboard_free_trie_index(data);
        }
        next.trieBytes=next.bytes.size();next.indexBytes=next.index.size();
        next.predictor.reset(lv_advanced_keyboard_predictor_create_borrowed(next.bytes.data(),next.bytes.size(),next.index.data(),next.index.size()));
    }
    if(!id.empty()&&!next.predictor)throw std::runtime_error("Model could not be loaded");
    column.model.predictor.reset();column.model=std::move(next);
    std::ostringstream metrics;
    if(column.model.predictor)metrics<<column.model.name<<"\nModel "<<column.model.trieBytes<<" B; index "<<column.model.indexBytes<<" B; workspace "
        <<lv_advanced_keyboard_predictor_workspace_bytes()<<" B; nodes "<<lv_advanced_keyboard_predictor_node_count(column.model.predictor.get());
    else metrics<<"No comparison model selected";
    lv_label_set_text(column.metrics,metrics.str().c_str());lv_obj_set_disabled(column.load,!column.model.predictor);column.window->dirty=true;
}
void Load(Column& column) {
    auto* w=column.window;
    try {
        EditorEdit::Scope edit;
        std::string id=column.model.id;
        if(id=="external") {
            auto records=Serialization::ProjectTries::Import(w->properties->Tries().ToJSON(),column.model.name,column.model.bytes.data(),column.model.bytes.size(),&id);
            w->properties->Tries().Load(records);
        }
        auto* selected=w->properties->GetSelectedObject();
        if(selected && lv_obj_is_in_widget_tree(selected) && Serialization::AdvancedKeyboard::Type(selected)) {
            w->properties->Tries().Apply(selected,id);w->properties->SetSelectedObject(selected);
        }
        Status(w,"Model loaded into the project. Choose it in Advanced Keyboard's Prediction trie selector.");
    }catch(const std::exception& e){Status(w,e.what());}
}
void Poll(lv_timer_t* timer) {
    auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));const auto caret=lv_textarea_get_cursor_pos(w->sample);
    if(!w->dirty&&caret==w->caret)return;w->dirty=false;w->caret=caret;
    const double threshold=lv_slider_get_value(w->threshold)/100.0;
    const auto context=espos::keyboard::detail::capture_suggestion_text(w->sample);
    // Empty word views may have a null data pointer. LVGL's built-in printf
    // still inspects %s arguments when the supplied precision is zero.
    lv_label_set_text_fmt(w->context,"Context: [%.*s] [%.*s]   Prefix: [%.*s]   Threshold: %.2f",
        int(context.older.size()),context.older.empty()?"":context.older.data(),
        int(context.previous.size()),context.previous.empty()?"":context.previous.data(),
        int(context.prefix.size()),context.prefix.empty()?"":context.prefix.data(),threshold);
    for(auto& column:w->columns) {
        const auto value=Evaluate(w->sample,column.model.predictor.get(),threshold);
        std::ostringstream text;text<<"Suggestions\n";
        if(value.suggestions.empty())text<<"No completion candidates\n";
        for(size_t i=0;i<value.suggestions.size();++i)text<<i+1<<". "<<value.suggestions[i]<<'\n';
        text<<"\nAutocorrect\n";
        if(value.correction.empty())text<<"Keep typed word";
        else text<<value.prefix<<" → "<<value.correction<<"\nConfidence "<<std::fixed<<std::setprecision(1)<<value.confidence*100<<"% — "<<(value.accepted?"accepted":"below threshold");
        lv_label_set_text(column.result,text.str().c_str());
    }
}
}
void Close() {
    if(!active)return;auto* w=active;auto* properties=w->properties;
    EditorWindow::ReleaseFocus(w->root);lv_obj_delete(w->root);
    if(!shutdown)if(auto* selected=properties->GetSelectedObject())if(lv_obj_is_in_widget_tree(selected))properties->SetSelectedObject(selected);
}
void Shutdown(){shutdown=true;Close();shutdown=false;}
lv_obj_t* ActiveWindow(){return active?active->root:nullptr;}
void Open(PropertyWindow* properties) {
    if(active){lv_obj_move_foreground(active->root);return;}
    auto* w=new Window;w->properties=properties;w->records=properties->Tries().ToJSON();
    w->root=EditorWindow::Create(lv_layer_top());active=w;
    lv_obj_set_size(w->root,(std::min)(1050,lv_display_get_horizontal_resolution(nullptr)-40),(std::min)(780,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);
    EditorWindow::SetTitle(w->root,"Language model playground");EditorWindow::SetCloseHandler(w->root,[]{Close();});
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));if(active==w)active=nullptr;delete w;},LV_EVENT_DELETE,w);
    auto* body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,12,0);
    EditorStyle::Help(body,"Try a model before loading it. Type a prefix or misspelling, move the caret, and compare suggestions and correction confidence. Typing here does not change the project.");
    w->sample=lv_textarea_create(body);lv_obj_set_size(w->sample,LV_PCT(100),85);lv_textarea_set_text(w->sample,"teh");EditorStyle::Control(w->sample);Focus(w,w->sample);
    lv_obj_add_event_cb(w->sample,[](lv_event_t* e){static_cast<Window*>(lv_event_get_user_data(e))->dirty=true;},LV_EVENT_VALUE_CHANGED,w);
    auto* thresholdRow=Row(body);auto* title=lv_label_create(thresholdRow);lv_label_set_text(title,"Autocorrect threshold");
    w->threshold=lv_slider_create(thresholdRow);lv_obj_set_flex_grow(w->threshold,1);lv_slider_set_range(w->threshold,0,100);lv_slider_set_value(w->threshold,30,LV_ANIM_OFF);Focus(w,w->threshold);
    lv_obj_add_event_cb(w->threshold,[](lv_event_t* e){static_cast<Window*>(lv_event_get_user_data(e))->dirty=true;},LV_EVENT_VALUE_CHANGED,w);
    w->context=lv_label_create(body);lv_obj_set_width(w->context,LV_PCT(100));lv_label_set_long_mode(w->context,LV_LABEL_LONG_MODE_WRAP);
    auto* comparisons=Row(body);lv_obj_set_height(comparisons,LV_SIZE_CONTENT);lv_obj_set_flex_align(comparisons,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    w->ids={"","builtin-en-us"};std::string choices="None\nBundled English";
    for(const auto& record:w->records){w->ids.push_back(record.at("id"));choices+='\n'+CleanName(record.value("name",record.at("id").get<std::string>()));}
    for(size_t i=0;i<2;++i) {
        auto& column=w->columns[i];column.window=w;
        auto* panel=lv_obj_create(comparisons);lv_obj_set_width(panel,0);lv_obj_set_flex_grow(panel,1);lv_obj_set_height(panel,LV_SIZE_CONTENT);lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_COLUMN);
        auto* label=lv_label_create(panel);lv_label_set_text(label,i?"Compare model B":"Model A");
        column.choice=lv_dropdown_create(panel);lv_obj_set_width(column.choice,LV_PCT(100));lv_dropdown_set_options(column.choice,choices.c_str());Focus(w,column.choice);
        lv_obj_add_event_cb(column.choice,[](lv_event_t* e){auto* c=static_cast<Column*>(lv_event_get_user_data(e));try{SetModel(*c,c->window->ids.at(lv_dropdown_get_selected(c->choice)));}catch(const std::exception& e){Status(c->window,e.what());}},LV_EVENT_VALUE_CHANGED,&column);
        Button(w,panel,"Open model...",[c=&column]{try{auto path=Pick();if(path.empty())return;std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file||file.tellg()<20||file.tellg()>16*1024*1024)throw std::runtime_error("Choose a model between 20 bytes and 16 MiB");std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg()));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("Cannot read model");SetModel(*c,"external",&bytes,path.stem().u8string());lv_dropdown_set_selected(c->choice,0);}catch(const std::exception& e){Status(c->window,e.what());}});
        column.metrics=lv_label_create(panel);lv_obj_set_width(column.metrics,LV_PCT(100));lv_label_set_long_mode(column.metrics,LV_LABEL_LONG_MODE_WRAP);
        column.result=lv_label_create(panel);lv_obj_set_width(column.result,LV_PCT(100));lv_obj_set_style_min_height(column.result,170,0);lv_label_set_long_mode(column.result,LV_LABEL_LONG_MODE_WRAP);
        column.load=Button(w,panel,"Load into project",[c=&column]{Load(*c);},165);
    }
    w->status=lv_label_create(body);lv_obj_set_width(w->status,LV_PCT(100));lv_label_set_long_mode(w->status,LV_LABEL_LONG_MODE_WRAP);Status(w,"Bundled English is ready. Select a project trie or open an external model to compare.");
    lv_dropdown_set_selected(w->columns[0].choice,1);SetModel(w->columns[0],"builtin-en-us");SetModel(w->columns[1],"");
    w->timer=lv_timer_create(Poll,60,w);FocusEditorControl(properties->GetKBGroup(),w->sample);
}
}
