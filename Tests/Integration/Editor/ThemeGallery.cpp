#include "ThemeGallery.h"
#include "EditorInputTest.h"
#include "../../../UICreator/ProjectThemes/ThemeGallery.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include <Serialization/ProjectDesign.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
namespace Input=EditorInputTest;
namespace Design=Serialization::ProjectDesign;
struct Theme {const char* id;const char* name;};
constexpr Theme additions[]={
    {"contrast-light","Signal Paper"},{"contrast-dark","Signal Night"},
    {"blueprint-light","Blueprint"},{"blueprint-dark","Blueprint Night"},
    {"copper-light","Kiln"},{"copper-dark","Copper Foundry"},
    {"arctic-light","Glacier"},{"arctic-dark","Polar Night"},
    {"citrus-light","Citrus Grove"},{"citrus-dark","Citrus After Dark"},
    {"editorial-light","Editorial"},{"editorial-dark","Edition Night"},
    {"paper-ink","Paper Ink"},{"black-glass","Black Glass"}};
json Record(int id,int parent,int x,int y,int width,int height){
    return {{"oid",id},{"par",parent},{"base",{{"name","Theme widget "+std::to_string(id)},{"drag",true},{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}}};
}
json Fixture(){
    auto card=Record(0,-1,12,12,456,296);auto title=Record(1,0,16,12,400,26);title["text"]="Device workspace";
    auto status=Record(2,0,16,45,320,24);status["text"]="Connected  |  Ready";
    auto button=Record(3,0,16,84,200,44);button["button"]={{"text","Continue"}};
    auto field=Record(4,0,16,146,260,42);field["textarea"]={{"text","Original content"},{"oneLine",true}};
    auto qr=Record(5,0,340,82,96,96);qr["qrcode"]={{"data","LVGL theme preview"},{"size",96},{"dark",0},{"light",0xffffff}};
    auto barcode=Record(6,0,16,218,240,40);barcode["barcode"]={{"data","12345678"},{"scale",1},{"height",40},{"dark",0},{"light",0xffffff}};
    auto toggle=Record(7,0,340,220,56,28);toggle["switch"]={{"checked",true}};
    return {{"canvas",{{"width",480},{"height",320}}},{"variables",json::array()},{"obj",json::array({card,title,status,button,field,qr,barcode,toggle})}};
}
lv_obj_t* Gallery(){auto* window=Input::WindowFromTitle("Theme gallery");Input::Require(window,"Theme gallery is missing");return window;}
lv_obj_t* Card(lv_obj_t* gallery,const char* id){
    auto* cards=lv_obj_get_child(EditorWindow::Content(gallery),0);const auto& preset=Design::Preset(id);
    for(uint32_t i=0;i<lv_obj_get_child_count(cards);++i){auto* tile=lv_obj_get_child(cards,i);auto* title=lv_obj_get_child(tile,0);
        if(lv_obj_has_class(title,&lv_label_class)&&std::string(lv_label_get_text(title))==preset.at("name").get<std::string>())return tile;}
    throw std::runtime_error(std::string("Theme card is missing: ")+id);
}
void Passive(lv_obj_t* object){
    Input::Require(!lv_obj_has_flag(object,LV_OBJ_FLAG_CLICKABLE)&&!lv_obj_get_group(object),"A theme miniature intercepted editor input or focus");
    for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)Passive(lv_obj_get_child(object,i));
}
void Click(lv_obj_t* window,const char* caption,SdlSimulator& simulator){
    auto* label=Input::FindLabel(window,caption);Input::Require(label,std::string("Theme action is missing: ")+caption);Input::Click(Input::ParentButton(label),simulator);
}
uint64_t PreviewHash(lv_obj_t* window){
    auto* image=Input::FindClass(window,&lv_image_class);Input::Require(image,"Actual-screen theme preview is missing");
    Input::Require(!lv_obj_has_flag(image,LV_OBJ_FLAG_CLICKABLE)&&!lv_obj_has_flag(image,LV_OBJ_FLAG_CLICK_FOCUSABLE)&&!lv_obj_get_group(image),"Actual-screen theme preview is interactive");
    const auto* source=static_cast<const lv_image_dsc_t*>(lv_image_get_src(image));
    Input::Require(source&&source->header.magic==LV_IMAGE_HEADER_MAGIC&&source->data&&source->data_size,"Actual-screen theme preview has no rendered framebuffer");
    uint64_t hash=1469598103934665603ull;for(uint32_t i=0;i<source->data_size;++i){hash^=source->data[i];hash*=1099511628211ull;}return hash;
}
void Capture(SdlSimulator& simulator,const std::filesystem::path& path){
    if(path.empty())return;Input::Drain(simulator);auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
    auto* image=SDL_RenderReadPixels(renderer,nullptr);Input::Require(image,"Cannot capture theme gallery");const bool saved=SDL_SaveBMP(image,path.u8string().c_str());SDL_DestroySurface(image);Input::Require(saved,"Cannot save theme gallery screenshot");
}
void Preserved(const json& before,const json& after){
    Input::Require(after.at("obj").size()==before.at("obj").size(),"Applying a theme changed the widget count");
    for(size_t i=0;i<before.at("obj").size();++i){const auto& original=before.at("obj")[i];const auto& applied=after.at("obj")[i];
        Input::Require(original.at("base").at("coords")==applied.at("base").at("coords"),"Applying a theme moved or resized an authored widget");
        for(const auto* key:{"text","button","textarea","qrcode","barcode"})if(original.contains(key))Input::Require(original.at(key)==applied.at(key),"Applying a theme changed content or machine-readable widget settings");
    }
    Input::Require(before.value("fonts",json::array())==after.value("fonts",json::array()),"Applying a theme changed project fonts");
}
}
void CheckThemeGallery(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory){
    if(!outputDirectory.empty())std::filesystem::create_directories(outputDirectory);
    const auto initial=editor.ProjectDocument();editor.ApplyDocument(Fixture());editor.SelectObjectIds({0});const auto original=editor.ProjectDocument();
    const auto revision=editor.DocumentRevision();const auto history=editor.HistoryLogicalBytes();auto* display=lv_display_get_default();auto* group=lv_group_get_default();
    editor.MenuAction("theme-gallery");auto* gallery=Gallery();auto* cards=lv_obj_get_child(EditorWindow::Content(gallery),0);
    Input::Require(Design::Presets().size()==28&&lv_obj_get_child_count(cards)==28,"The gallery must expose all 28 theme cards");
    for(const auto& preset:Design::Presets()){auto* card=Card(gallery,preset.at("id").get_ref<const std::string&>().c_str());Passive(lv_obj_get_child(card,1));}
    const auto overview=std::to_string(Design::Presets().size())+" coordinated palettes, including light, dark and high-contrast styles. Theme changes are included in project files and device exports.";
    Input::Require(Input::FindLabel(gallery,overview.c_str()),"Theme gallery shows an outdated hardcoded theme count");
    std::set<uint64_t> images;json report=json::array();
    for(const auto& theme:additions){
        Input::Require(Design::Preset(theme.id).at("name")==theme.name,"A newly added palette changed its expected identity");Input::Click(Card(gallery,theme.id),simulator);
        const auto hash=PreviewHash(gallery);Input::Require(images.insert(hash).second,std::string(theme.id)+": actual-screen preview repeats another new palette");
        Input::Require(editor.ProjectDocument()==original&&editor.DocumentRevision()==revision&&editor.HistoryLogicalBytes()==history,"Selecting a theme preview changed the authored project or history");
        Input::Require(lv_display_get_default()==display&&lv_group_get_default()==group,"A theme preview replaced the editor display or keyboard group");
        report.push_back({{"id",theme.id},{"name",theme.name},{"previewHash",hash}});
    }
    if(!outputDirectory.empty())Capture(simulator,outputDirectory/"gallery.bmp");Click(gallery,"Cancel preview",simulator);
    Input::Require(!Input::WindowFromTitle("Theme gallery")&&editor.ProjectDocument()==original,"Cancelling the gallery applied a preview theme");
    for(const auto* id:{"arctic-light","black-glass","contrast-dark"}){
        editor.MenuAction("theme-gallery");gallery=Gallery();Input::Click(Card(gallery,id),simulator);Click(gallery,"Preview and apply...",simulator);
        auto* review=Input::WindowFromTitle("Preview project theme");Input::Require(review,"Theme confirmation did not open");PreviewHash(review);
        Input::Require(editor.ProjectDocument()==original,"Opening theme confirmation committed the palette");
        if(!outputDirectory.empty())Capture(simulator,outputDirectory/(std::string("preview-")+id+".bmp"));Click(review,"Cancel preview",simulator);
        Input::Require(editor.ProjectDocument()==original&&!Input::WindowFromTitle("Preview project theme"),"Cancelling theme confirmation changed the design");
        Click(gallery,"Preview and apply...",simulator);review=Input::WindowFromTitle("Preview project theme");Input::Require(review,"Theme confirmation did not reopen");Click(review,"Apply preview",simulator);
        const auto applied=editor.ProjectDocument();Input::Require(Design::ThemeId(applied)==id,"Confirmed palette was not applied to the project");Preserved(original,applied);
        ProjectThemeWindow::Close();Input::Require(editor.Undo()&&editor.ProjectDocument()==original,"Undo did not restore the design before applying a new palette");
    }
    if(!outputDirectory.empty()){std::ofstream output(outputDirectory/"theme-gallery.json");output<<json{{"count",Design::Presets().size()},{"previewed",report}}.dump(2);Input::Require(bool(output),"Cannot write theme preview verification report");}
    editor.ApplyDocument(initial);
    std::cout<<"28 theme cards and 14 distinct passive previews; light/dark/high-contrast confirmation, cancel, Undo and content/code-widget protection passed\n";
}
