#include "MenuToolbarIcons.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Utils/EditorIcons.h"
#include "../../../UICreator/Utils/ObjectIcons.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/CommandPalette.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {
namespace Input=EditorInputTest;
struct Entry {const char* menu;const char* caption;const char* action;};
constexpr Entry entries[]={
    {"File","New project","new"},{"File","Open project...","open"},{"File","Recent projects","submenu:Recent projects"},
    {"File","Save project","save"},{"File","Save project as...","save-as"},{"File","Exit","exit"},
    {"Edit","Undo","undo"},{"Edit","Redo","redo"},{"Edit","Duplicate selection","duplicate"},{"Edit","Delete selection","delete"},
    {"Edit","Align","submenu:Align"},{"Edit","Distribute","submenu:Distribute"},{"Edit","Edit widget events...","events-editor"},
    {"Edit","Layout designer...","layout-designer"},{"Edit","Find property...","property-search"},
    {"Align","Left","align-left"},{"Align","Horizontal centers","align-hcenter"},{"Align","Right","align-right"},
    {"Align","Top","align-top"},{"Align","Vertical centers","align-vcenter"},{"Align","Bottom","align-bottom"},
    {"Distribute","Horizontally","distribute-horizontal"},{"Distribute","Vertically","distribute-vertical"},
    {"View","Commands...","commands"},{"View","Device previews...","device-previews"},{"View","Screens and navigation...","screen-flow"},
    {"View","Problems","problems"},{"View","Fit canvas","fit-canvas"},{"View","Actual size (100%)","zoom-reset"},
    {"View","Zoom in","zoom-in"},{"View","Zoom out","zoom-out"},{"View","Show panel","submenu:Show panel"},
    {"View","Restore all panels","view:all"},{"View","Workspace preferences...","preferences"},
    {"Show panel","Screens","view:screens"},{"Show panel","Tool Tray","view:widgets"},{"Show panel","Hierarchy","view:hierarchy"},
    {"Show panel","Properties","view:properties"},{"Show panel","Tools","view:tools"},{"Show panel","Simulator preview","view:simulator"},
    {"Assets","Theme gallery...","theme-gallery"},{"Assets","Reusable components...","component-library"},{"Assets","Project fonts...","font-manager"},
    {"Assets","Project images...","image-manager"},{"Assets","Add font...","font-converter"},{"Assets","Convert images...","image-converter"},
    {"Language Models","Model playground...","model-playground"},{"Language Models","Generate language model...","language-model-generator"},
    {"Language Models","Load model into project...","language-model-load"},
    {"Export","Device Protobuf (.pb)...","export-protobuf"},{"Export","LVGL source project...","export-c"},
    {"Export","MicroPython project...","export-micropython"},
    {"Export","UI Creator XML (.xml)...","export-xml"},
    {"Export","Create firmware project...","project-creator"},{"Export","Build, flash and serial monitor...","device-tools"},
    {"Debug","Run screen in editor","run-toggle"},{"Debug","Run application from start","run-application"},
    {"Debug","Performance dashboard...","performance"},{"Debug","Validate screen...","validate-screen"},
    {"Debug","Debug current screen","debug-screen"},{"Debug","Stop debug viewer","debug-stop"},
    {"Help","Welcome and examples...","welcome"},{"Help","Keyboard shortcuts","shortcuts"},
    {"Help","Diagnostics report","diagnostics"},{"Help","About LVGL UI Creator","about"}
};
constexpr const char* menuNames[]={"File","Edit","View","Assets","Language Models","Export","Debug","Help"};
bool Hidden(lv_obj_t* object) {for(auto* p=object;p;p=lv_obj_get_parent(p))if(lv_obj_is_hidden(p))return true;return false;}
lv_obj_t* Decorated(lv_obj_t* parent,const char* action) {
    const auto* key=EditorIcons::Action(parent);
    if(key&&std::string(key)==action)return parent;
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i)if(auto* result=Decorated(lv_obj_get_child(parent,i),action))return result;
    return nullptr;
}
lv_obj_t* IconChild(lv_obj_t* row,const ObjectIcons::Icon* expected) {
    lv_obj_t* result=nullptr;
    for(uint32_t i=0;i<lv_obj_get_child_count(row);++i)if(auto* descriptor=ObjectIcons::Get(lv_obj_get_child(row,i))) {
        Input::Require(!result&&descriptor==expected,"Command row has an incorrect or duplicate icon");result=lv_obj_get_child(row,i);
    }
    Input::Require(result,"Command row is missing its vector icon");
    Input::Require(!lv_obj_is_clickable(result)&&!lv_obj_is_click_focusable(result)&&!lv_obj_get_group(result),"Decorative command icon intercepts input or keyboard focus");
    return result;
}
void Bounds(lv_obj_t* child,lv_obj_t* parent,const char* message) {
    lv_area_t a{},b{};lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);
    Input::Require(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2,message);
}
void CheckButton(lv_obj_t* button,const char* action) {
    Input::Require(button,std::string("Toolbar button missing: ")+action);
    auto* icon=IconChild(button,ObjectIcons::Action(action));Bounds(icon,button,"Toolbar icon is clipped by its button");
    auto* label=lv_obj_get_child(button,0);Input::Require(lv_obj_has_class(label,&lv_label_class),"Toolbar caption child0 compatibility was lost");
    if(!lv_obj_is_hidden(label)) {
        Bounds(label,button,"Toolbar caption is clipped by its button");lv_area_t a{},b{};lv_obj_get_coords(icon,&a);lv_obj_get_coords(label,&b);
        Input::Require(a.x2<b.x1,"Toolbar icon overlaps its caption");
    }
    Input::Require(EditorIcons::Hint(button)&&*EditorIcons::Hint(button),"Icon-only toolbar action lacks a descriptive hint");
}
void Capture(SdlSimulator& simulator,const std::filesystem::path& path) {
    Input::Drain(simulator);auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
    auto* bitmap=SDL_RenderReadPixels(renderer,nullptr);Input::Require(bitmap,"Cannot capture menu icons");
    const bool saved=SDL_SaveBMP(bitmap,path.u8string().c_str());SDL_DestroySurface(bitmap);Input::Require(saved,"Cannot save menu icon capture");
}
struct Framebuffer {
    lv_display_t *display=nullptr,*previous=lv_display_get_default();std::vector<uint32_t> pixels;
    Framebuffer():pixels(64*64){
        display=lv_display_create(64,64);Input::Require(display,"Cannot allocate command icon display");lv_display_set_default(previous);
        lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);lv_display_set_buffers(display,pixels.data(),nullptr,uint32_t(pixels.size()*4),LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(display,[](lv_display_t* target,const lv_area_t*,uint8_t*){lv_display_flush_ready(target);});
        auto* root=lv_display_get_screen_active(display);lv_obj_remove_style_all(root);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);lv_obj_set_style_bg_color(root,lv_color_hex(0x252d38),0);
    }
    ~Framebuffer(){lv_display_delete(display);lv_display_set_default(previous);}
};
void Catalog() {
    Input::Require(ObjectIcons::Count()==89,"Command icons changed the add-object catalog");
    Input::Require(!ObjectIcons::Action(nullptr)&&!ObjectIcons::Action("")&&!ObjectIcons::Action("unknown-action"),"Unknown command silently uses an unrelated icon");
    Input::Require(ObjectIcons::Action("recent:project.json")&&!ObjectIcons::Action("recent:"),"Recent-project icon resolution is incomplete");
    for(const auto& entry:entries)Input::Require(ObjectIcons::Action(entry.action),std::string("Missing action binding: ")+entry.action);
    for(const auto* action:{"run-stop","screen-add","screen-home","select-screen","close","minimize","restore","settings","copy","paste","move-up","move-down","grid-decrease","grid-increase","variable-add","variable-delete"})
        Input::Require(ObjectIcons::Action(action),std::string("Missing toolbar binding: ")+action);
    Framebuffer frame;std::set<std::string> keys;
    for(size_t i=0;i<ObjectIcons::ActionCount();++i) {
        const auto* descriptor=ObjectIcons::ActionAt(i);Input::Require(descriptor&&descriptor->name&&*descriptor->name&&keys.insert(descriptor->key).second,"Invalid command icon catalog entry");
        for(int size:{16,18,20,24}) {
            auto* icon=ObjectIcons::Create(lv_display_get_screen_active(frame.display),descriptor,size);lv_obj_center(icon);
            lv_obj_update_layout(lv_display_get_screen_active(frame.display));lv_obj_invalidate(icon);lv_refr_now(frame.display);
            size_t ink=0;for(auto pixel:frame.pixels)if((pixel&0xffffffu)!=0x252d38)++ink;
            Input::Require(ink>=8&&ink<size_t(size*size),std::string(descriptor->key)+": blank or opaque command icon");lv_obj_delete(icon);
        }
    }
    Input::Require(!keys.empty()&&!ObjectIcons::ActionAt(keys.size()),"Command catalog bounds are inconsistent");
}
void Menus(UI& editor,SdlSimulator& simulator,const std::filesystem::path& output) {
    for(const auto* menu:{"File","Edit","View","Assets","Language Models","Export","Debug","Help","Align","Distribute","Show panel"}) {
        editor.MenuAction(std::string("menu:")+menu);lv_obj_update_layout(lv_layer_top());
        for(const auto& entry:entries)if(std::string(entry.menu)==menu) {
            auto* label=Input::FindLabel(lv_layer_top(),entry.caption);Input::Require(label,std::string("Menu caption disappeared or was truncated: ")+entry.caption);
            auto* row=lv_obj_get_parent(label);Input::Require(lv_obj_has_class(row,&lv_button_class)&&lv_obj_get_child(row,0)==label,"Menu caption lost its direct first-child layout");
            auto* icon=IconChild(row,ObjectIcons::Action(entry.action));Bounds(icon,row,"Menu icon is clipped");Bounds(label,row,"Menu caption is clipped");
            lv_area_t a{},b{};lv_obj_get_coords(icon,&a);lv_obj_get_coords(label,&b);Input::Require(a.x2<b.x1,"Menu icon overlaps its caption");
            for(uint32_t i=1;i<lv_obj_get_child_count(row);++i) {auto* hint=lv_obj_get_child(row,i);if(lv_obj_has_class(hint,&lv_label_class)) {
                Bounds(hint,row,"Menu shortcut or submenu hint is clipped");lv_area_t hb{};lv_obj_get_coords(hint,&hb);
                Input::Require(hb.x1>b.x2||hb.y1>b.y2,"Menu shortcut or detail overlaps the command caption");
            }}
        }
        if(std::string(menu)=="File"||std::string(menu)=="Edit")Capture(simulator,output/(std::string("menu-")+menu+".bmp"));
        editor.MenuAction("dismiss");
    }
    auto* display=lv_display_get_default();const int width=lv_display_get_horizontal_resolution(display),height=lv_display_get_vertical_resolution(display);
    auto resize=[&](int value){lv_display_set_resolution(display,value,height);for(int i=0;i<45;++i){SDL_Delay(5);simulator.ProcessEvents();}lv_obj_update_layout(lv_screen_active());};
    for(int resized:{1024,800}) {
        resize(resized);
        int right=-1;
        for(const auto* menu:menuNames) {
            const auto action=std::string("menu:")+menu;auto* button=Decorated(lv_screen_active(),action.c_str());CheckButton(button,action.c_str());
            lv_area_t bounds{};lv_obj_get_coords(button,&bounds);Input::Require(bounds.x1>right&&bounds.x2<resized,"Top menu headings overlap or extend past the display");right=bounds.x2;
        }
        auto* exit=Decorated(lv_screen_active(),"exit");CheckButton(exit,"exit");lv_area_t exitBounds{};lv_obj_get_coords(exit,&exitBounds);
        Input::Require(exitBounds.x1>right&&exitBounds.x2<resized,"Application exit button is clipped by compact menu headings");
        editor.MenuAction("menu:File");Capture(simulator,output/("menu-"+std::to_string(resized)+".bmp"));editor.MenuAction("dismiss");
    }
    resize(width);
    auto* file=Decorated(lv_screen_active(),"menu:File");Input::Click(EditorIcons::Icon(file),simulator);
    auto* open=Input::FindLabel(lv_layer_top(),"Open project...");Input::Require(open,"Clicking a top-menu icon did not open its menu");
    Input::Click(IconChild(lv_obj_get_parent(open),ObjectIcons::Action("open")),simulator);
    auto* dialog=Input::WindowFromTitle("Open project");Input::Require(dialog,"Clicking a menu row icon did not invoke Open project");
    Input::Click(EditorIcons::Icon(EditorWindow::CloseButton(dialog)),simulator);
    Input::Require(!Input::WindowFromTitle("Open project"),"Dialog close icon did not preserve its action");
}
void Toolbar(UI& editor,SdlSimulator& simulator,const std::filesystem::path& output) {
    for(const auto* action:{"undo","redo","run-toggle","zoom-out","zoom-reset","zoom-in","fit-canvas","grid-decrease","grid-increase","screen-add","screen-flow","settings","image-manager","font-manager","minimize"})
        CheckButton(Decorated(lv_screen_active(),action),action);
    Input::Require(std::string(lv_label_get_text(lv_obj_get_child(Decorated(lv_screen_active(),"image-manager"),0)))=="Project images","Asset shortcut truncates its caption beside the icon");
    auto* undo=Decorated(lv_screen_active(),"undo");Input::Require(lv_obj_is_disabled(undo),"Undo unexpectedly enabled in a clean fixture");
    auto* settings=Decorated(lv_screen_active(),"settings");Input::Click(EditorIcons::Icon(settings),simulator);
    auto* dialog=Input::WindowFromTitle("TFT Settings");Input::Require(dialog,"Clicking the settings icon did not open TFT Settings");
    auto* close=EditorWindow::CloseButton(dialog);CheckButton(close,"close");Input::Click(EditorIcons::Icon(close),simulator);
    Input::Require(Hidden(dialog),"Clicking the close icon did not close its dialog");
    auto* run=Decorated(lv_screen_active(),"run-toggle");Input::Click(EditorIcons::Icon(run),simulator);Input::Require(editor.Running(),"Clicking Run icon did not enter run mode");
    auto wait=[&](const char* expected){for(int i=0;i<80&&!Decorated(lv_screen_active(),expected);++i){SDL_Delay(5);simulator.ProcessEvents();}CheckButton(Decorated(lv_screen_active(),expected),expected);};
    wait("run-stop");
    // Run deliberately covers the design toolbar; the menu bar remains usable.
    auto* debug=Decorated(lv_screen_active(),"menu:Debug");Input::Click(EditorIcons::Icon(debug),simulator);
    auto* design=Input::FindLabel(lv_layer_top(),"Return to Design");Input::Require(design,"Run mode lost its Return to Design command");
    Input::Click(IconChild(lv_obj_get_parent(design),ObjectIcons::Action("run-stop")),simulator);
    Input::Require(!editor.Running(),"Clicking Return to Design menu icon did not stop run mode");wait("run-toggle");
    auto* button=lv_button_create(lv_screen_active());lv_obj_set_size(button,40,32);auto* caption=lv_label_create(button);lv_label_set_text(caption,"-");
    Input::Require(EditorIcons::Set(button,"zoom-out",true),"Cannot decorate an icon-only button");
    auto* system=lv_display_get_layer_sys(lv_display_get_default());const auto count=lv_obj_get_child_count(system);
    lv_obj_send_event(button,LV_EVENT_HOVER_OVER,nullptr);Input::Require(lv_obj_get_child_count(system)==count+1,"Icon-only button has no descriptive hover hint");
    lv_obj_delete(button);Input::Require(lv_obj_get_child_count(system)==count,"Deleting a hovered button leaked its hint");
    Capture(simulator,output/"toolbar.bmp");
    editor.MenuAction("commands");auto* commands=CommandPalette::ActiveWindow();Input::Require(commands,"Command search did not open");
    std::function<void(lv_obj_t*)> inspect=[&](lv_obj_t* parent) {for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i){auto* child=lv_obj_get_child(parent,i);if(lv_obj_has_class(child,&lv_button_class)&&lv_obj_get_child_count(child)>1) {
        bool icon=false;for(uint32_t j=0;j<lv_obj_get_child_count(child);++j)icon=icon||ObjectIcons::Get(lv_obj_get_child(child,j));Input::Require(icon,"Command search row is missing its matching icon");
    }inspect(child);}};inspect(EditorWindow::Content(commands));Capture(simulator,output/"commands.bmp");CommandPalette::Close();
}
}
void CheckMenuToolbarIcons(UI& editor,SdlSimulator& simulator,const std::filesystem::path& output) {
    std::filesystem::create_directories(output);const auto original=editor.ProjectDocument();Catalog();Menus(editor,simulator,output);Toolbar(editor,simulator,output);
    Input::Require(editor.ProjectDocument()==original&&!editor.Dirty(),"Menu/toolbar decoration changed the authored project");
    std::cout<<"Menu/toolbar icons: complete action catalog, native 16/18/20/24px renders, all menus, compact headings, toolbars, hover cleanup and physical icon dispatch passed\n";
}
