#include "ObjectIcons.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Utils/ObjectIcons.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../3rdParty/LVGL/lvgl/include/lvgl/widgets/lv_bootstrap.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace Input=EditorInputTest;
constexpr const char* widgetNames[]={
    "Arc","Bar","Button","Button matrix","Calendar","Canvas","Check box","Chart","Container",
    "Drop down list","Gauge","Image","Image button","Keyboard","Label","LED","Line","Line meter",
    "Message box","Page","Preloader","Roller","Slider","Spinbox","Switch","Table","Tabview","Text area",
    "Tile view","Window","Text box","Layer view","Barcode","QR code","Advanced Keyboard"};
constexpr const char* templateNames[]={"Admin dashboard","Analytics dashboard","Commerce dashboard","User management",
    "Settings page","Sign in","Checkout","Pricing","Album","Blog","Product","Cover","Component gallery"};
constexpr const char* templateKeys[]={"template.admin-dashboard","template.analytics-dashboard","template.commerce-dashboard",
    "template.user-management","template.settings-page","template.sign-in","template.checkout","template.pricing",
    "template.album","template.blog","template.product","template.cover","template.component-gallery"};
constexpr uint32_t Background=0x252d38;

struct Framebuffer {
    lv_display_t* display=nullptr;
    lv_display_t* previous=lv_display_get_default();
    std::vector<uint32_t> pixels;
    int width,height;
    Framebuffer(int w,int h):pixels(size_t(w)*h),width(w),height(h){
        display=lv_display_create(w,h);Input::Require(display,"Cannot allocate icon-test display");
        lv_display_set_default(previous);lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_flush_cb(display,[](lv_display_t* target,const lv_area_t*,uint8_t*){lv_display_flush_ready(target);});
        lv_display_set_buffers(display,pixels.data(),nullptr,uint32_t(pixels.size()*sizeof(uint32_t)),LV_DISPLAY_RENDER_MODE_DIRECT);
        auto* root=Root();lv_obj_remove_style_all(root);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
        lv_obj_set_style_bg_color(root,lv_color_hex(Background),0);lv_obj_set_style_text_color(root,lv_color_hex(0xe4eaf2),0);
        lv_obj_set_scrollable(root,false);
    }
    ~Framebuffer(){if(display)lv_display_delete(display);lv_display_set_default(previous);}
    lv_obj_t* Root()const{return lv_display_get_screen_active(display);}
    void Render(){lv_obj_update_layout(Root());lv_obj_invalidate(Root());lv_refr_now(display);}
    void Save(const std::filesystem::path& path){
        Render();auto* image=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_XRGB8888,pixels.data(),width*4);
        Input::Require(image,"Cannot allocate icon contact sheet");const bool saved=SDL_SaveBMP(image,path.u8string().c_str());
        SDL_DestroySurface(image);Input::Require(saved,"Cannot save icon contact sheet");
    }
};
uint64_t PixelHash(const std::vector<uint32_t>& pixels){
    uint64_t hash=1469598103934665603ull;for(auto pixel:pixels){hash^=pixel&0xffffffu;hash*=1099511628211ull;}return hash;
}
void CheckDecoration(lv_obj_t* object,const ObjectIcons::Icon* expected){
    Input::Require(object&&ObjectIcons::Get(object)==expected,"Icon object lost its explicit catalog descriptor");
    Input::Require(!lv_obj_has_flag(object,LV_OBJ_FLAG_CLICKABLE)&&!lv_obj_has_flag(object,LV_OBJ_FLAG_CLICK_FOCUSABLE),
        std::string(expected->key)+": decorative icon intercepts pointer or keyboard focus");
}
std::vector<const ObjectIcons::Icon*> CheckCatalog(){
    Input::Require(ObjectIcons::Count()==84,"Every addable object must have one explicit icon: expected 84 catalog entries");
    std::set<const ObjectIcons::Icon*> expected;std::set<std::string> keys;
    for(int i=0;i<35;++i){const auto* icon=ObjectIcons::Widget(i);Input::Require(icon&&std::string(icon->name)==widgetNames[i],"Widget icon is missing or mapped to another widget");expected.insert(icon);}
    for(int i=0;i<LV_BOOTSTRAP_PRESET_COUNT;++i){
        const auto* icon=ObjectIcons::Preset(i);Input::Require(icon&&std::string(icon->name)==lv_bootstrap_preset_name(static_cast<lv_bootstrap_preset_t>(i)),"Bootstrap/template icon name differs from the native recipe");
        if(i>=LV_BOOTSTRAP_COMPONENT_COUNT){const int index=i-LV_BOOTSTRAP_COMPONENT_COUNT;Input::Require(std::string(icon->name)==templateNames[index]&&std::string(icon->key)==templateKeys[index],"Page template icon lacks its intentional namespaced identity");}
        expected.insert(icon);
    }
    std::vector<const ObjectIcons::Icon*> icons;
    for(size_t i=0;i<ObjectIcons::Count();++i){
        const auto* icon=ObjectIcons::At(i);Input::Require(icon&&icon->key&&*icon->key&&icon->name&&*icon->name&&icon->commands&&icon->commandCount,
            "Icon catalog contains an empty or fallback-only entry");
        Input::Require(expected.erase(icon)==1,"Icon catalog duplicates an entry or includes a nonaddable object");
        Input::Require(keys.insert(icon->key).second,"Two addable objects share one icon identity");icons.push_back(icon);
    }
    Input::Require(expected.empty(),"An addable object is absent from the enumerable icon catalog");return icons;
}
json CheckRendered(const std::vector<const ObjectIcons::Icon*>& icons){
    Framebuffer frame(64,64);std::map<std::vector<uint32_t>,std::string> rendered;json report=json::array();
    for(const auto* descriptor:icons){
        json entry={{"key",descriptor->key},{"name",descriptor->name},{"sizes",json::array()}};
        for(int size:{24,32,48}){
            auto* object=ObjectIcons::Create(frame.Root(),descriptor,size);CheckDecoration(object,descriptor);lv_obj_set_pos(object,(64-size)/2,(64-size)/2);frame.Render();
            std::vector<uint32_t> foreground;size_t ink=0;
            for(int y=0;y<size;++y)for(int x=0;x<size;++x){const auto pixel=frame.pixels[size_t(y+(64-size)/2)*64+x+(64-size)/2]&0xffffffu;foreground.push_back(pixel);if(pixel!=Background)++ink;}
            Input::Require(ink>=20&&ink<foreground.size()*9/10,std::string(descriptor->key)+": icon is blank or an opaque placeholder");
            if(size==32){auto inserted=rendered.emplace(foreground,descriptor->key);Input::Require(inserted.second,std::string(descriptor->key)+": icon repeats the rendered geometry of "+inserted.first->second);}
            entry["sizes"].push_back({{"size",size},{"inkPixels",ink},{"pixelHash",PixelHash(foreground)}});lv_obj_delete(object);
        }
        report.push_back(std::move(entry));
    }
    return report;
}
void ContactSheet(const std::vector<const ObjectIcons::Icon*>& icons,const std::filesystem::path& path){
    constexpr int columns=10,cellWidth=120,cellHeight=88;const int rows=int((icons.size()+columns-1)/columns);
    Framebuffer sheet(columns*cellWidth,rows*cellHeight);
    for(size_t i=0;i<icons.size();++i){const int x=int(i%columns)*cellWidth,y=int(i/columns)*cellHeight;
        auto* icon=ObjectIcons::Create(sheet.Root(),icons[i],40);lv_obj_set_pos(icon,x+(cellWidth-40)/2,y+5);
        auto* label=lv_label_create(sheet.Root());lv_obj_set_style_text_font(label,LV_FONT_DEFAULT,0);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_text(label,icons[i]->name);lv_obj_set_width(label,cellWidth-8);lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_WRAP);lv_obj_set_pos(label,x+4,y+49);
    }
    sheet.Save(path);
}
int Category(int id){
    if(id==30||id==31)return 3;if(id==18||id==29)return 2;
    const std::set<int> advanced={3,4,5,7,10,12,13,17,19,25,26,28,32,33,34};return advanced.count(id)?1:0;
}
lv_obj_t* IconChild(lv_obj_t* button,const ObjectIcons::Icon* expected){
    lv_obj_t* result=nullptr;for(uint32_t i=0;i<lv_obj_get_child_count(button);++i){auto* child=lv_obj_get_child(button,i);if(ObjectIcons::Get(child)){Input::Require(!result,"Palette card contains multiple decorative icons");CheckDecoration(child,expected);result=child;}}
    Input::Require(result,std::string(expected->name)+": palette card has no icon");return result;
}
lv_obj_t* PaletteCard(lv_obj_t* page,const ObjectIcons::Icon* descriptor){
    auto* caption=Input::FindLabel(page,descriptor->name);Input::Require(caption,std::string(descriptor->name)+": palette caption is missing");
    auto* button=lv_obj_get_parent(caption);Input::Require(lv_obj_has_class(button,&lv_button_class),"Palette caption must remain a direct child of its creation button");
    auto* icon=IconChild(button,descriptor);lv_area_t ib{},cb{},bb{};lv_obj_get_coords(icon,&ib);lv_obj_get_coords(caption,&cb);lv_obj_get_coords(button,&bb);
    Input::Require(ib.x1>=bb.x1&&ib.x2<=bb.x2&&ib.y1>=bb.y1&&ib.y2<=bb.y2,std::string(descriptor->name)+": icon is clipped by its card");
    Input::Require(cb.x1>=bb.x1&&cb.x2<=bb.x2&&cb.y1>=bb.y1&&cb.y2<=bb.y2,std::string(descriptor->name)+": caption is clipped by its card");
    const bool separated=ib.y2<cb.y1||cb.y2<ib.y1||ib.x2<cb.x1||cb.x2<ib.x1;Input::Require(separated,std::string(descriptor->name)+": icon overlaps its caption");return button;
}
void CheckPalette(UI& editor,SdlSimulator& simulator,const std::filesystem::path& output){
    auto* window=Input::WindowFromTitle("Tool Tray");Input::Require(window,"Tool Tray is missing");
    auto* tabs=Input::FindClass(EditorWindow::Content(window),&lv_tabview_class);Input::Require(tabs,"Tool Tray categories are missing");
    auto* content=lv_tabview_get_content(tabs);Input::Require(lv_obj_get_child_count(content)==6,"An add-object category is missing");
    const int originalWidth=lv_obj_get_width(window);const auto originalTab=lv_tabview_get_tab_active(tabs);
    try{
        for(int width:{180,300,460}){lv_obj_set_width(window,width);lv_obj_update_layout(lv_screen_active());
            for(unsigned category=0;category<6;++category){lv_tabview_set_active(tabs,category,LV_ANIM_OFF);lv_obj_update_layout(lv_screen_active());auto* page=lv_obj_get_child(content,category);
                unsigned count=0;for(int id=0;id<35;++id)if(Category(id)==int(category)){PaletteCard(page,ObjectIcons::Widget(id));++count;}
                if(category>=4)for(int id=category==4?0:LV_BOOTSTRAP_COMPONENT_COUNT;id<(category==4?LV_BOOTSTRAP_COMPONENT_COUNT:LV_BOOTSTRAP_PRESET_COUNT);++id){PaletteCard(page,ObjectIcons::Preset(id));++count;}
                const unsigned expected[]={16,15,2,2,36,13};Input::Require(count==expected[category],"Palette category coverage changed without an icon inventory update");
            }
        }
    }catch(...){lv_obj_set_width(window,originalWidth);lv_tabview_set_active(tabs,originalTab,LV_ANIM_OFF);throw;}
    lv_obj_set_width(window,originalWidth);lv_obj_update_layout(lv_screen_active());
    const auto clean=editor.ProjectDocument();
    auto insert=[&](unsigned category,const ObjectIcons::Icon* descriptor){
        lv_tabview_set_active(tabs,category,LV_ANIM_OFF);lv_obj_update_layout(lv_screen_active());auto* card=PaletteCard(lv_obj_get_child(content,category),descriptor);
        editor.MenuAction("select-screen");Input::Click(IconChild(card,descriptor),simulator);
        Input::Require(editor.ProjectDocument()!=clean,std::string(descriptor->name)+": clicking its icon did not add an object");
        Input::Require(editor.Undo()&&editor.ProjectDocument()==clean,std::string(descriptor->name)+": icon insertion did not remain one undo step");
    };
    for(int id:{0,30,31,32,33,34})insert(unsigned(Category(id)),ObjectIcons::Widget(id));
    insert(4,ObjectIcons::Preset(LV_BOOTSTRAP_CARD));insert(5,ObjectIcons::Preset(LV_BOOTSTRAP_DASHBOARD));
    for(const auto& alias:std::vector<std::pair<const char*,int>>{{"Admin dashboard",LV_BOOTSTRAP_DASHBOARD},{"Sign-in form",LV_BOOTSTRAP_SIGN_IN},{"Component gallery",LV_BOOTSTRAP_COMPONENT_GALLERY}}){
        editor.MenuAction("welcome");auto* dialog=Input::WindowFromTitle("Welcome to LVGL UI Creator");Input::Require(dialog,"Welcome dialog is missing");
        auto* caption=Input::FindLabel(dialog,alias.first);Input::Require(caption,"Welcome template shortcut is missing");
        Input::Click(IconChild(lv_obj_get_parent(caption),ObjectIcons::Preset(alias.second)),simulator);
        Input::Require(editor.ProjectDocument()!=clean,"Welcome template icon did not route to its native recipe");Input::Require(editor.Undo()&&editor.ProjectDocument()==clean,"Welcome icon insertion lost its undo transaction");
    }
    lv_tabview_set_active(tabs,originalTab,LV_ANIM_OFF);lv_obj_update_layout(lv_screen_active());
    if(!output.empty()){
        auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));Input::Drain(simulator);
        auto* bitmap=SDL_RenderReadPixels(renderer,nullptr);Input::Require(bitmap,"Cannot capture decorated widget palette");const bool saved=SDL_SaveBMP(bitmap,(output/"palette.bmp").u8string().c_str());SDL_DestroySurface(bitmap);Input::Require(saved,"Cannot save decorated widget palette");
    }
}
}

void CheckObjectIcons(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory){
    const auto original=editor.ProjectDocument();const auto icons=CheckCatalog();const auto report=CheckRendered(icons);
    Input::Require(editor.ProjectDocument()==original,"Rendering the icon catalog changed the authored project");
    if(!outputDirectory.empty()){
        std::filesystem::create_directories(outputDirectory);ContactSheet(icons,outputDirectory/"object-icons.bmp");
        std::ofstream output(outputDirectory/"manifest.json");output<<json{{"format","uicreator-object-icons"},{"version",1},{"count",icons.size()},{"icons",report}}.dump(2);Input::Require(bool(output),"Cannot write icon verification manifest");
    }
    CheckPalette(editor,simulator,outputDirectory);Input::Require(editor.ProjectDocument()==original,"Icon interaction tests changed the authored project");
    std::cout<<"84 intentional object icons rendered distinctly at24/32/48px; six palette categories, narrow/default/wide layouts, real icon clicks, Welcome shortcuts and undo passed\n";
}
