#include "VisualRegression.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/RunPreview.h"
#include "../../../UICreator/PreviewProfiles/PreviewSurface.h"
#include "../../../UICreator/Performance/Telemetry.h"
#include <Serialization/ProjectDesign.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void Write(const std::filesystem::path& path,const json& value){std::ofstream out(path);out<<value.dump(2);Require(bool(out),"Could not write regression artifact");}
json Record(int id,int x,int y,int width,int height){return {{"oid",id},{"par",-1},{"base",{{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}}};}
json Scene(const std::string& theme,int width,int height) {
    auto title=Record(0,16,12,width-32,24);title["text"]="Device control";
    auto status=Record(1,16,42,width-32,24);status["text"]="Connected  |  Ready";
    auto button=Record(2,16,76,(width-48)/2,44);button["button"]={{"text","Apply changes"}};
    auto dropdown=Record(3,32+(width-48)/2,76,(width-48)/2,44);dropdown["dropdown"]={{"options","Automatic\nManual\nOffline"},{"selected",0}};
    auto slider=Record(4,20,140,width-44,14);slider["slider"]={{"value",62},{"min",0},{"max",100}};
    auto label=Record(5,16,166,width-112,24);label["text"]="Brightness  62%";
    auto qr=Record(6,width-88,height-88,72,72);qr["qrcode"]={{"data","LVGL UI Creator"},{"size",72},{"dark",0},{"light",0xffffff}};
    json document={{"canvas",{{"width",width},{"height",height}}},{"obj",json::array({title,status,button,dropdown,slider,label,qr})},{"variables",json::array()}};
    Serialization::ProjectDesign::ApplyTheme(document,theme);return document;
}
void StaticStyle(lv_obj_t* object) {
    // Built-in LVGL fonts and no timer-driven state make fixtures independent of
    // installed Windows/Linux fonts, caret phase, and wall-clock time.
    lv_obj_set_style_text_font(object,LV_FONT_DEFAULT,LV_PART_MAIN);
    lv_obj_remove_state(object,static_cast<lv_state_t>(LV_STATE_FOCUSED|LV_STATE_FOCUS_KEY|LV_STATE_PRESSED));
    lv_anim_delete(object,nullptr);
    for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)StaticStyle(lv_obj_get_child(object,i));
}
void SavePixels(PreviewProfiles::PreviewSurface& surface,const std::filesystem::path& path) {
    std::vector<uint32_t> pixels(size_t(surface.Width())*surface.Height());
    for(int y=0;y<surface.Height();++y)for(int x=0;x<surface.Width();++x)pixels[size_t(y)*surface.Width()+x]=surface.Pixel(x,y);
    auto* bitmap=SDL_CreateSurfaceFrom(surface.Width(),surface.Height(),SDL_PIXELFORMAT_XRGB8888,pixels.data(),surface.Width()*4);
    Require(bitmap,"Could not create regression framebuffer surface");const bool saved=SDL_SaveBMP(bitmap,path.u8string().c_str());SDL_DestroySurface(bitmap);Require(saved,"Could not save regression framebuffer");
}
void Pump(SdlSimulator& simulator,unsigned milliseconds=20) {
    const auto end=SDL_GetTicks()+milliseconds;while(SDL_GetTicks()<end){SDL_Delay(2);simulator.ProcessEvents();}
}
json ReplayScene() {
    auto first=Record(0,16,16,430,48);first["textarea"]={{"text",""},{"oneLine",true}};
    auto second=Record(1,16,78,430,48);second["textarea"]={{"text",""},{"oneLine",true}};
    auto keyboard=Record(2,0,150,480,170);keyboard["advancedKeyboard"]={{"targetOid",0},{"autoFocus",true},{"trieId","builtin-en-us"},{"autocorrect",false}};
    return {{"canvas",{{"width",480},{"height",320}}},{"variables",json::array()},{"obj",json::array({first,second,keyboard})}};
}
json DefaultReplay() {
    return {{"format","lvgl-input-replay"},{"version",1},{"actions",json::array({
        {{"type","focus"},{"oid",0}},{{"type","text"},{"text","hello professional interface"},{"targetOid",0},{"expectedText","hello professional interface"}},
        {{"type","focus"},{"oid",1}},{{"type","text"},{"text","second target!"},{"targetOid",1},{"expectedText","second target!"}},
        {{"type","key"},{"key","backspace"},{"targetOid",1},{"expectedText","second target"}},
        {{"type","prediction"},{"prefix","hel"},{"iterations",100}},{{"type","frames"},{"count",30}}
    })}};
}
json Replay(SdlSimulator& simulator,const std::filesystem::path& output) {
    auto actions=DefaultReplay();if(std::filesystem::exists(output/"replay-input.json")){std::ifstream in(output/"replay-input.json");actions=json::parse(in);}
    Require(actions.value("format",std::string{})=="lvgl-input-replay"&&actions.value("version",0)==1&&actions.contains("actions")&&actions["actions"].is_array(),"Expected an lvgl-input-replay version 1 action file");
    Require(actions["actions"].size()<=1024,"Replay action count exceeds 1024");Write(output/"replay-input.json",actions);
    RunPreview preview;preview.Start(ReplayScene(),{},[]{});preview.SetProfilingEnabled(true);auto* root=lvgl_runtime_screen(preview.Runtime(),0);auto* display=lv_obj_get_display(root);json trace=json::array();
    for(const auto& action:actions["actions"]) {
        const auto type=action.at("type").get<std::string>();
        if(type=="focus") {auto* target=lvgl_runtime_object(preview.Runtime(),0,action.at("oid").get<int>());Require(target&&lv_obj_get_group(target),"Replay focus target is not a keyboard control");lv_group_focus_obj(target);}
        else if(type=="text") {const auto text=action.at("text").get<std::string>();Require(text.size()<=512,"Replay text exceeds 512 bytes");SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=text.c_str();Require(preview.HandleEvent(event),"Replay text was not routed to the runtime");}
        else if(type=="key") {const auto key=action.at("key").get<std::string>();SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;if(key=="backspace")event.key.key=SDLK_BACKSPACE;else if(key=="enter")event.key.key=SDLK_RETURN;else if(key=="tab")event.key.key=SDLK_TAB;else throw std::runtime_error("Unsupported replay key");Require(preview.HandleEvent(event),"Replay key was not routed to the runtime");}
        else if(type=="prediction") {preview.BenchmarkPrediction(action.at("prefix").get<std::string>(),action.value("iterations",100u));}
        else if(type=="frames") {const auto count=action.value("count",30u);Require(count>0&&count<=1000,"Replay frame count must be 1-1000");for(unsigned i=0;i<count;++i){lv_obj_invalidate(root);lv_refr_now(display);}}
        else throw std::runtime_error("Unsupported replay action: "+type);
        Pump(simulator);lv_refr_now(display);auto result=action;
        if(action.contains("expectedText")){auto* target=lvgl_runtime_object(preview.Runtime(),0,action.at("targetOid").get<int>());Require(target&&lv_obj_has_class(target,&lv_textarea_class),"Replay assertion target is not a textarea");const auto actual=std::string(lv_textarea_get_text(target));result["actualText"]=actual;Require(actual==action.at("expectedText").get<std::string>(),"Input replay produced unexpected target text");}
        result["passed"]=true;trace.push_back(std::move(result));
    }
    const auto measured=preview.Metrics();Require(measured.frame.samples>0&&measured.key.samples>0&&measured.prediction.samples>0,"Input replay did not collect real rendering/input/prediction measurements");
    auto report=Performance::Report(measured);report["replayAssertionsPassed"]=true;report["replayActions"]=trace.size();Write(output/"performance.json",report);Write(output/"replay-trace.json",trace);preview.Stop();return report;
}
}

void ExportVisualRegressionFixtures(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory) {
    const auto output=std::filesystem::absolute(outputDirectory);std::filesystem::create_directories(output);const auto authored=editor.ProjectDocument();
    json manifest={{"format","lvgl-visual-fixtures"},{"version",1},{"environment",{{"platform",SDL_GetPlatform()},{"sdlVersion",SDL_GetVersion()},{"lvglVersion",std::to_string(LVGL_VERSION_MAJOR)+"."+std::to_string(LVGL_VERSION_MINOR)+"."+std::to_string(LVGL_VERSION_PATCH)},{"font","LV_FONT_DEFAULT (built-in)"},{"videoDriver",SDL_GetCurrentVideoDriver()?SDL_GetCurrentVideoDriver():"unknown"}}},{"cases",json::array()}};
    auto* host=lv_obj_create(lv_layer_top());lv_obj_set_size(host,850,520);lv_obj_set_scrollable(host,false);
    try {
        PreviewProfiles::PreviewSurface surface(host);
        for(const auto& theme:Serialization::ProjectDesign::Presets())for(const auto& resolution:std::vector<std::pair<int,int>>{{320,240},{480,320},{800,480}}) {
            const auto id=theme.at("id").get<std::string>();const auto width=resolution.first,height=resolution.second;
            surface.Load(Scene(id,width,height),width,height);auto* root=lvgl_runtime_screen(surface.Runtime(),0);StaticStyle(root);lv_obj_update_layout(root);lv_obj_invalidate(root);lv_refr_now(lv_obj_get_display(root));
            const auto file=id+"-"+std::to_string(width)+"x"+std::to_string(height)+".bmp";SavePixels(surface,output/file);
            manifest["cases"].push_back({{"id",id+"-"+std::to_string(width)+"x"+std::to_string(height)},{"theme",id},{"width",width},{"height",height},{"file",file},{"pixelHash",surface.PixelHash()}});surface.Clear();
        }
    }catch(...){lv_obj_delete(host);throw;}
    lv_obj_delete(host);Write(output/"manifest.json",manifest);Replay(simulator,output);Require(editor.ProjectDocument()==authored,"Regression fixture generation modified the authored project");
    std::cout<<"Exported "<<manifest["cases"].size()<<" deterministic native theme/resolution fixtures and measured input replay to "<<output.u8string()<<"\n";
}
