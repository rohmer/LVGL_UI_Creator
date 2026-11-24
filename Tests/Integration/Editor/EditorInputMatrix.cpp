#include "EditorInputMatrix.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorControls.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/CommandPalette.h"
#include "../../../UICreator/LanguageModels/LanguageModelWindow.h"
#include "../../../UICreator/LanguageModels/ModelPlayground.h"
#include "../../../UICreator/Devices/DeviceWindow.h"
#include "../../../UICreator/Performance/PerformanceWindow.h"
#include "../../../UICreator/Screens/ScreenFlowWindow.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace EditorInputTest {
void Require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
void Drain(SdlSimulator& simulator){for(int frame=0;frame<12;++frame)Require(simulator.ProcessEvents(),"SDL stopped during editor input test");}
namespace {
bool Hidden(lv_obj_t* object){for(auto* parent=object;parent;parent=lv_obj_get_parent(parent))if(lv_obj_is_hidden(parent))return true;return false;}
std::string Description(lv_obj_t* object){
    if(!object||!lv_obj_is_valid(object))return "missing field";
    if(lv_obj_has_class(object,&lv_textarea_class)){const auto* placeholder=lv_textarea_get_placeholder_text(object);if(placeholder&&*placeholder)return placeholder;}
    for(int depth=0;object&&depth<3;++depth){auto* parent=lv_obj_get_parent(object);if(!parent)break;
        for(int32_t index=lv_obj_get_index(object)-1;index>=0;--index){auto* label=lv_obj_get_child(parent,index);if(lv_obj_has_class(label,&lv_label_class)&&*lv_label_get_text(label))return lv_label_get_text(label);}
        object=parent;
    }
    return "editor field";
}
void QueueKey(SDL_Keycode key,SDL_Scancode scan,SDL_Keymod modifiers){
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;event.key.scancode=scan;event.key.mod=modifiers;event.key.down=true;
    event.key.windowID=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    Require(SDL_PushEvent(&event),"Cannot queue editor key press");event.type=SDL_EVENT_KEY_UP;event.key.down=false;
    Require(SDL_PushEvent(&event),"Cannot queue editor key release");
}
}
void Click(lv_obj_t* object,SdlSimulator& simulator){
    Require(object&&lv_obj_is_valid(object)&&!Hidden(object),"Editor pointer target is missing or hidden");
    lv_obj_scroll_to_view_recursive(object,LV_ANIM_OFF);lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());
    lv_area_t area{};lv_obj_get_coords(object,&area);lv_obj_get_transformed_area(object,&area,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
    const float x=float((area.x1+area.x2)/2),y=float((area.y1+area.y2)/2);
    const auto id=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=id;motion.motion.x=x;motion.motion.y=y;
    Require(SDL_PushEvent(&motion),"Cannot queue editor pointer motion");
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.windowID=id;event.button.button=SDL_BUTTON_LEFT;
    event.button.down=true;event.button.x=x;event.button.y=y;Require(SDL_PushEvent(&event),"Cannot queue editor pointer press");Drain(simulator);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;Require(SDL_PushEvent(&event),"Cannot queue editor pointer release");Drain(simulator);
}
void Key(SDL_Keycode key,SDL_Scancode scan,SdlSimulator& simulator,SDL_Keymod modifiers){
    const auto previous=SDL_GetModState();SDL_SetModState(modifiers);QueueKey(key,scan,modifiers);Drain(simulator);SDL_SetModState(previous);
}
void Text(const char* text,SdlSimulator& simulator){
    SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=text;
    event.text.windowID=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    Require(SDL_PushEvent(&event),"Cannot queue editor text input");Drain(simulator);
}
void AssertFocused(lv_obj_t* field,SdlSimulator& simulator){
    auto* group=lv_indev_get_group(simulator.GetKeyboard());Require(group&&lv_group_get_focused(group)==field,"Clicked editor control did not receive physical keyboard focus: "+Description(field));
}
void Replace(lv_obj_t* field,const std::string& value,SdlSimulator& simulator){
    Require(field&&lv_obj_has_class(field,&lv_textarea_class)&&!lv_obj_has_class(field,&lv_spinbox_class),"Replace requires an editable textarea");
    const auto description=Description(field);
    Require(!lv_obj_has_state(field,LV_STATE_DISABLED),"Cannot type into a disabled editor field: "+description);
    Click(field,simulator);AssertFocused(field,simulator);
    const auto previous=std::string(lv_textarea_get_text(field));QueueKey(SDLK_HOME,SDL_SCANCODE_HOME,SDL_KMOD_NONE);
    for(size_t index=0;index<previous.size();++index)QueueKey(SDLK_DELETE,SDL_SCANCODE_DELETE,SDL_KMOD_NONE);
    Drain(simulator);Require(lv_obj_is_valid(field)&&std::string(lv_textarea_get_text(field)).empty(),"Physical Home/Delete did not clear the editor field: "+description);
    if(!value.empty())Text(value.c_str(),simulator);
    Require(lv_obj_is_valid(field)&&std::string(lv_textarea_get_text(field))==value,"Physical SDL text input did not update the editor field: "+description);
}
lv_obj_t* FindLabel(lv_obj_t* root,const char* caption){
    if(!root)return nullptr;if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root))==caption)return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=FindLabel(lv_obj_get_child(root,i),caption))return found;return nullptr;
}
lv_obj_t* FindPlaceholder(lv_obj_t* root,const char* placeholder){
    if(!root)return nullptr;if(lv_obj_has_class(root,&lv_textarea_class)){const char* text=lv_textarea_get_placeholder_text(root);if(text&&std::string(text)==placeholder)return root;}
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=FindPlaceholder(lv_obj_get_child(root,i),placeholder))return found;return nullptr;
}
lv_obj_t* FindClass(lv_obj_t* root,const lv_obj_class_t* type){
    if(!root)return nullptr;if(lv_obj_has_class(root,type))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=FindClass(lv_obj_get_child(root,i),type))return found;return nullptr;
}
lv_obj_t* FindField(lv_obj_t* root,const char* caption,const lv_obj_class_t* type){
    auto* label=FindLabel(root,caption);if(!label)return nullptr;auto* row=lv_obj_get_parent(label);
    for(uint32_t i=lv_obj_get_index(label)+1;i<lv_obj_get_child_count(row);++i)if(auto* found=FindClass(lv_obj_get_child(row,i),type))return found;
    return FindClass(row,type);
}
std::vector<lv_obj_t*> Fields(lv_obj_t* root,const lv_obj_class_t* type){
    std::vector<lv_obj_t*> result;if(!root)return result;if(lv_obj_has_class(root,type))result.push_back(root);
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i){auto nested=Fields(lv_obj_get_child(root,i),type);result.insert(result.end(),nested.begin(),nested.end());}return result;
}
lv_obj_t* ParentButton(lv_obj_t* label){for(auto* object=label;object;object=lv_obj_get_parent(object))if(lv_obj_has_class(object,&lv_button_class))return object;return nullptr;}
lv_obj_t* WindowFromTitle(const char* title){
    auto find=[&](auto&& self,lv_obj_t* root)->lv_obj_t* {
        if(EditorWindow::CloseData(root)){
            auto* header=EditorWindow::Header(root);auto* label=lv_obj_get_child(header,0);
            if(label&&lv_obj_has_class(label,&lv_label_class)&&std::string(lv_label_get_text(label))==title)return root;
        }
        for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=self(self,lv_obj_get_child(root,i)))return found;
        return nullptr;
    };
    if(auto* window=find(find,lv_layer_top()))return window;return find(find,lv_screen_active());
}
}

namespace {
namespace Input=EditorInputTest;
#ifdef _WIN32
LONG WINAPI InputFault(EXCEPTION_POINTERS* fault){
        void* frames[24]{};const auto count=CaptureStackBackTrace(0,24,frames,nullptr);
        std::cerr<<"Editor input native fault: code="<<std::hex<<fault->ExceptionRecord->ExceptionCode
            <<" address="<<fault->ExceptionRecord->ExceptionAddress<<" image="<<GetModuleHandleW(nullptr)<<'\n';
        for(USHORT i=0;i<count;++i)std::cerr<<"  frame "<<std::dec<<i<<" "<<frames[i]<<'\n';
        std::cerr.flush();return EXCEPTION_CONTINUE_SEARCH;
}
#endif
void InstallFaultTrace(){
#ifdef _WIN32
    SetUnhandledExceptionFilter(InputFault);
#endif
}
struct ClipboardRestore {
    std::string previous;
    ClipboardRestore(){char* value=SDL_GetClipboardText();if(value){previous=value;SDL_free(value);}}
    ~ClipboardRestore(){SDL_SetClipboardText(previous.c_str());}
};
struct SessionDirectory {
    std::string old;bool present=false;
    SessionDirectory(){if(const auto* value=std::getenv("UICREATOR_SESSION_DIR")){old=value;present=true;}
        const auto path=(std::filesystem::temp_directory_path()/("uicreator-input-matrix-"+std::to_string(SDL_GetTicksNS()))).u8string();Set(path.c_str());}
    static void Set(const char* value){
#ifdef _WIN32
        _putenv_s("UICREATOR_SESSION_DIR",value);
#else
        if(value)setenv("UICREATOR_SESSION_DIR",value,1);else unsetenv("UICREATOR_SESSION_DIR");
#endif
    }
    ~SessionDirectory(){Set(present?old.c_str():
#ifdef _WIN32
        ""
#else
        nullptr
#endif
    );}
};
json Fixture(){return {{"formatVersion",2},{"canvas",{{"width",320},{"height",240}}},{"variables",json::array()},
    {"obj",json::array({{{"oid",0},{"par",-1},{"base",{{"name","Editable selection"},{"drag",true},{"coords",{{"x",20},{"y",20},{"width",140},{"height",45}}}}},{"button",{{"text","Selected button"}}}}})}};}
lv_obj_t* Open(UI& editor,SdlSimulator& simulator,const char* action,const char* title){std::cout<<"Checking tool editor: "<<title<<std::endl;editor.MenuAction(action);std::cout<<"Tool action returned"<<std::endl;Input::Drain(simulator);auto* window=Input::WindowFromTitle(title);Input::Require(window,std::string("Editor did not open: ")+title);std::cout<<"Tool is ready"<<std::endl;return window;}
void Close(lv_obj_t* window,SdlSimulator& simulator){
    std::cout<<"Closing tool editor"<<std::endl;auto fields=Input::Fields(window);bool deleted=false;
    const lv_event_cb_t observe=[](lv_event_t* event){*static_cast<bool*>(lv_event_get_user_data(event))=true;};
    lv_obj_add_event_cb(window,observe,LV_EVENT_DELETE,&deleted);Input::Click(EditorWindow::CloseButton(window),simulator);
    if(!deleted&&lv_obj_is_valid(window))lv_obj_remove_event_cb_with_user_data(window,observe,&deleted);
    Input::Require(deleted,"Closing editor retained its window");
    auto* group=lv_indev_get_group(simulator.GetKeyboard());auto* focused=group?lv_group_get_focused(group):nullptr;
    Input::Require(std::find(fields.begin(),fields.end(),focused)==fields.end(),"Closed editor retained keyboard focus");std::cout<<"Tool closed"<<std::endl;
}
void Field(lv_obj_t* root,const char* caption,const char* value,SdlSimulator& simulator){
    std::cout<<"Typing tool field: "<<caption<<" ("<<SDL_GetTicks()<<" ms)"<<std::endl;
    auto* field=Input::FindField(root,caption);Input::Require(field,std::string("Missing editor field: ")+caption);Input::Replace(field,value,simulator);
}
void Placeholder(lv_obj_t* root,const char* placeholder,const char* value,SdlSimulator& simulator){
    std::cout<<"Typing tool placeholder: "<<placeholder<<" ("<<SDL_GetTicks()<<" ms)"<<std::endl;
    auto* field=Input::FindPlaceholder(root,placeholder);Input::Require(field,std::string("Missing editor placeholder: ")+placeholder);Input::Replace(field,value,simulator);
}
void FocusScope(SdlSimulator& simulator){
    auto* group=lv_indev_get_group(simulator.GetKeyboard());auto* window=EditorWindow::Create(lv_layer_top());EditorWindow::SetTitle(window,"Editor input scope fixture");lv_obj_set_size(window,500,700);lv_obj_center(window);
    auto* mainDisplay=lv_display_get_default();auto* privateDisplay=lv_display_create(32,32);auto* privateGroup=lv_group_create();auto* privateKeyboard=lv_indev_create();
    lv_indev_set_type(privateKeyboard,LV_INDEV_TYPE_KEYPAD);lv_indev_set_display(privateKeyboard,privateDisplay);lv_indev_set_group(privateKeyboard,privateGroup);lv_indev_set_mode(privateKeyboard,LV_INDEV_MODE_EVENT);lv_display_set_default(mainDisplay);
    const bool chosePhysical=EditorKeyboardGroup()==group;
    lv_indev_delete(privateKeyboard);lv_group_delete(privateGroup);lv_display_delete(privateDisplay);
    Input::Require(chosePhysical,"Editor input resolver selected a private preview keyboard instead of the physical main display");
    auto* body=EditorWindow::Content(window);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);
    auto field=[&](const char* value){auto* object=lv_textarea_create(body);lv_textarea_set_one_line(object,true);lv_textarea_set_text(object,value);lv_obj_set_size(object,350,38);BindEditorControl(group,object);return object;};
    auto* first=field("first");auto* hidden=field("hidden");lv_obj_set_hidden(hidden, true);auto* disabled=field("disabled");lv_obj_add_state(disabled,LV_STATE_DISABLED);auto* second=field("second");
    Input::Replace(first,u8"Typed \u00e9 123",simulator);Input::Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator);Input::AssertFocused(second,simulator);
    auto* dynamic=field("dynamic");Input::Click(second,simulator);Input::Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator);Input::AssertFocused(dynamic,simulator);
    lv_obj_set_hidden(hidden, false);Input::Click(first,simulator);Input::Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator);Input::AssertFocused(hidden,simulator);
    Input::Click(disabled,simulator);Input::Require(lv_group_get_focused(group)!=disabled,"Disabled editor field acquired keyboard focus");
    auto* tabs=lv_tabview_create(body);lv_obj_set_size(tabs,420,180);lv_tabview_set_tab_bar_size(tabs,28);
    auto* activePage=lv_tabview_add_tab(tabs,"Active");auto* inactivePage=lv_tabview_add_tab(tabs,"Inactive");lv_obj_set_flex_flow(activePage,LV_FLEX_FLOW_COLUMN);
    auto tabField=[&](lv_obj_t* page){auto* object=lv_textarea_create(page);lv_obj_set_size(object,300,36);lv_textarea_set_one_line(object,true);BindEditorControl(group,object);return object;};
    auto* tabFirst=tabField(activePage);auto* tabSecond=tabField(activePage);auto* inactive=tabField(inactivePage);lv_tabview_set_active(tabs,0,LV_ANIM_OFF);
    Input::Click(tabFirst,simulator);Input::Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator);Input::AssertFocused(tabSecond,simulator);
    Input::Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator);Input::Require(lv_group_get_focused(group)!=inactive,"Tab entered an inactive editor tab page");Input::AssertFocused(first,simulator);
    Input::Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator,SDL_KMOD_SHIFT);Input::AssertFocused(tabSecond,simulator);
    lv_tabview_set_active(tabs,1,LV_ANIM_OFF);Input::Text("inactive input must be ignored",simulator);
    Input::Require(std::string(lv_textarea_get_text(tabSecond)).empty(),"Typing changed an editor field on an inactive tab page");
    lv_tabview_set_active(tabs,0,LV_ANIM_OFF);Input::Click(first,simulator);
    struct Clipboard {
        std::string previous;
        Clipboard(){char* value=SDL_GetClipboardText();if(value){previous=value;SDL_free(value);}}
        ~Clipboard(){SDL_SetClipboardText(previous.c_str());}
    } clipboard;
    auto clipboardText=[](){char* value=SDL_GetClipboardText();std::string result=value?value:"";SDL_free(value);return result;};
    Input::Replace(first,u8"caf\u00e9 \u6a21\u578b",simulator);Input::Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Input::Key(SDLK_C,SDL_SCANCODE_C,simulator,SDL_KMOD_CTRL);
    Input::Require(clipboardText()==u8"caf\u00e9 \u6a21\u578b","Copy changed the UTF-8 text selection");
    Input::Key(SDLK_X,SDL_SCANCODE_X,simulator,SDL_KMOD_CTRL);Input::Require(std::string(lv_textarea_get_text(first)).empty(),"Cut did not remove selected editor text");
    Input::Key(SDLK_V,SDL_SCANCODE_V,simulator,SDL_KMOD_CTRL);Input::Require(std::string(lv_textarea_get_text(first))==u8"caf\u00e9 \u6a21\u578b","Paste did not restore UTF-8 editor text");
    Input::Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Input::Text(u8"\u66ff\u4ee3\u00e9",simulator);Input::Require(std::string(lv_textarea_get_text(first))==u8"\u66ff\u4ee3\u00e9","Typing did not replace the selected UTF-8 text");
    Input::Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Input::Key(SDLK_DELETE,SDL_SCANCODE_DELETE,simulator);Input::Require(std::string(lv_textarea_get_text(first)).empty(),"Delete did not remove the editor text selection");
    const auto previousModifiers=SDL_GetModState();const auto altGr=SDL_Keymod(SDL_KMOD_CTRL|SDL_KMOD_ALT|SDL_KMOD_RALT);
    SDL_SetModState(altGr);Input::Key(SDLK_Q,SDL_SCANCODE_Q,simulator,altGr);Input::Text(u8"@\u20ac",simulator);SDL_SetModState(previousModifiers);
    Input::Require(std::string(lv_textarea_get_text(first))==u8"@\u20ac","AltGr UTF-8 text was swallowed as an editor shortcut");
    Input::Replace(second,"password field",simulator);lv_textarea_set_password_mode(second,true);SDL_SetClipboardText("password-copy-sentinel");
    Input::Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Input::Key(SDLK_C,SDL_SCANCODE_C,simulator,SDL_KMOD_CTRL);Input::Require(clipboardText()=="password-copy-sentinel","Copy exposed a password editor value");
    auto* oldField=field("old focus");auto* target=field("focus target");Input::Click(oldField,simulator);
    bool targetDeleted=false;
    const lv_event_cb_t observeTarget=[](lv_event_t* event){*static_cast<bool*>(lv_event_get_user_data(event))=true;};
    const lv_event_cb_t deleteTarget=[](lv_event_t* event){lv_obj_delete(static_cast<lv_obj_t*>(lv_event_get_user_data(event)));};
    lv_obj_add_event_cb(target,observeTarget,LV_EVENT_DELETE,&targetDeleted);
    lv_obj_add_event_cb(oldField,deleteTarget,LV_EVENT_DEFOCUSED,target);
    FocusEditorControl(group,target);
    lv_obj_remove_event_cb_with_user_data(oldField,deleteTarget,target);
    const bool observedDeletion=targetDeleted;
    if(!targetDeleted){lv_obj_remove_event_cb_with_user_data(target,observeTarget,&targetDeleted);lv_obj_delete(target);}
    Input::Require(observedDeletion,"Defocusing the old editor did not execute the focus-target deletion fixture");
    Input::Require(lv_group_get_focused(group)!=target,"Focus retained an input deleted by its predecessor's DEFOCUSED callback");
    lv_obj_delete(oldField);
    EditorWindow::ReleaseFocus(window);lv_obj_delete(window);Input::Drain(simulator);
}
void ToolEditors(UI& editor,SdlSimulator& simulator){
    const auto original=editor.ProjectDocument();
    auto* window=Open(editor,simulator,"language-model-generator","Language model generator");
    Placeholder(window,"Add folders containing corpus .txt files","C:/corpus one\nD:/corpus two",simulator);Field(window,"Worker threads","3",simulator);Field(window,"Trie output file","C:/typed/model.ngtr",simulator);Close(window,simulator);
    window=Open(editor,simulator,"model-playground","Language model playground");std::cout<<"Typing playground sample"<<std::endl;Input::Replace(Input::FindClass(window,&lv_textarea_class),"hel",simulator);std::cout<<"Playground sample typed"<<std::endl;Close(window,simulator);
    window=Open(editor,simulator,"device-tools","Build, flash and serial monitor");Field(window,"Target name","Typed target",simulator);Field(window,"Project folder","C:/typed/project",simulator);Field(window,"Build workers","3",simulator);Close(window,simulator);
    window=Open(editor,simulator,"theme-gallery","Theme gallery");Placeholder(window,"#RRGGBB or size","#245789",simulator);Close(window,simulator);
    window=Open(editor,simulator,"component-library","Component library");Placeholder(window,"Component name","Typed reusable component",simulator);Placeholder(window,"Search saved components","typed",simulator);Close(window,simulator);
    window=Open(editor,simulator,"layout-designer","Flex and Grid layout designer");Placeholder(window,"Gap (px)","12",simulator);Placeholder(window,"Columns","1fr,2fr,120",simulator);Close(window,simulator);
    window=Open(editor,simulator,"device-previews","Device preview comparison");Placeholder(window,"Custom profile name","Typed device",simulator);Placeholder(window,"Width","640",simulator);Placeholder(window,"Height","360",simulator);Close(window,simulator);
    window=Open(editor,simulator,"events-editor","Events and actions");Field(window,"Screen number (navigate, starts at 1)","1",simulator);Field(window,"Value (JSON value, or plain text)","17",simulator);
    auto* action=Input::FindField(window,"Action",&lv_dropdown_class);Input::Click(action,simulator);Input::AssertFocused(action,simulator);Input::Key(SDLK_DOWN,SDL_SCANCODE_DOWN,simulator);Input::Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);Input::Require(lv_dropdown_get_selected(action)==1,"Interaction action dropdown ignored physical keyboard");Close(window,simulator);
    const auto beforeScreen=editor.ProjectDocument();window=Open(editor,simulator,"screen-flow","Screens and navigation");
    auto* screenName=Input::FindField(window,"Screen name");Input::Replace(screenName,"Typed screen name",simulator);
    Input::Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);SDL_SetClipboardText("Typed pasted screen");Input::Key(SDLK_V,SDL_SCANCODE_V,simulator,SDL_KMOD_CTRL);
    Input::Require(std::string(lv_textarea_get_text(screenName))=="Typed pasted screen","Pasting a screen-name draft reloaded the saved screen name");
    auto* duration=Input::FindField(window,"Transition duration (ms)",&lv_spinbox_class);Input::Require(duration,"Screen duration spinbox is missing");Input::Click(duration,simulator);Input::AssertFocused(duration,simulator);lv_spinbox_set_value(duration,0);lv_spinbox_set_step(duration,1);Input::Text("7",simulator);
    Input::Require(lv_spinbox_get_value(duration)==7,"Physical digits did not update the numeric editor's actual spinbox value");
    Input::Require(std::string(lv_textarea_get_text(screenName))=="Typed pasted screen"&&editor.ProjectDocument()==beforeScreen,"Typing a numeric draft reset another field or applied uncommitted screen properties");
    Input::Click(Input::ParentButton(Input::FindLabel(window,"Save screen properties")),simulator);
    const auto savedScreen=editor.ProjectDocument().at("screens").at(0);
    Input::Require(savedScreen.at("name")=="Typed pasted screen"&&savedScreen.at("transition").at("duration")==7,"Apply did not commit the typed screen name and native numeric value");
    Close(window,simulator);editor.ApplyDocument(beforeScreen);editor.SelectObjectIds({0});Input::Drain(simulator);
    window=Open(editor,simulator,"performance","Performance dashboard");Field(window,"Prediction prefix","test",simulator);Field(window,"Frame CPU P95 budget (ms)","16.667",simulator);Close(window,simulator);
    editor.MenuAction("problems");Input::Drain(simulator);window=Input::WindowFromTitle("Problems");Input::Require(window,"Problems editor is missing");
    auto* tabs=Input::FindClass(window,&lv_tabview_class);Input::Require(tabs,"Problems editor tabs are missing");lv_tabview_set_active(tabs,2,LV_ANIM_OFF);Input::Drain(simulator);
    Field(window,"Text key","settings.title",simulator);Field(window,"Locale","fr",simulator);Field(window,"Translation",u8"R\u00e9glages",simulator);
    Input::Require(editor.ProjectDocument()==original,"Typing uncommitted tool fields changed the authored document");
}
}

void CheckEditorInputMatrix(UI& editor,SdlSimulator& simulator){
    InstallFaultTrace();
    ClipboardRestore clipboard;
    SessionDirectory session;SDL_SetModState(SDL_KMOD_NONE);
    const auto started=SDL_GetTicks();
    const auto phase=[&](const char* caption,auto&& action){
        const auto before=SDL_GetTicks();std::cout<<"Checking "<<caption<<" (total "<<before-started<<" ms)"<<std::endl;
        action();std::cout<<"Completed "<<caption<<" in "<<SDL_GetTicks()-before<<" ms (total "<<SDL_GetTicks()-started<<" ms)"<<std::endl;
    };
    phase("dynamic focus, inactive tabs, clipboard and AltGr",[&]{FocusScope(simulator);});
    editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});phase("property and legacy editors",[&]{CheckPropertyEditorInput(editor,simulator);});
    editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});phase("asset converters, managers and project creator",[&]{CheckAssetEditorInput(editor,simulator);});
    editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});phase("tool editors",[&]{ToolEditors(editor,simulator);});
    editor.MenuAction("commands");Input::Drain(simulator);auto* palette=CommandPalette::ActiveWindow();Input::Require(palette,"Command palette did not open");
    Input::Replace(Input::FindPlaceholder(palette,"Type to search..."),"fit",simulator);Input::Require(CommandPalette::ResultCount()>0,"Command palette ignored typed query");Input::Key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE,simulator);Input::Require(!CommandPalette::ActiveWindow(),"Command palette did not release its input state");
    std::cout<<"Real SDL input passed across property/legacy editors, asset converters/managers, firmware creator, language models, devices, themes, components, layout/previews, events, screens, budgets, translations and dynamic focus scopes\n";
}

void CheckEditorInputAssets(UI& editor,SdlSimulator& simulator){
    InstallFaultTrace();
    ClipboardRestore clipboard;SessionDirectory session;SDL_SetModState(SDL_KMOD_NONE);
    editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});CheckAssetEditorInput(editor,simulator);
}
void CheckEditorInputTools(UI& editor,SdlSimulator& simulator){
    InstallFaultTrace();
    ClipboardRestore clipboard;SessionDirectory session;SDL_SetModState(SDL_KMOD_NONE);
    editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});ToolEditors(editor,simulator);
}
void CheckEditorInputProperties(UI& editor,SdlSimulator& simulator){
    InstallFaultTrace();
    ClipboardRestore clipboard;SessionDirectory session;SDL_SetModState(SDL_KMOD_NONE);
    editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});CheckPropertyEditorInput(editor,simulator);
}
