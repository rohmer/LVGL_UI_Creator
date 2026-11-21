#include "Performance.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/RunPreview.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Performance/PerformanceWindow.h"
#include <stdexcept>
#include <iostream>
namespace {
void Require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void Pump(SdlSimulator& simulator){for(int i=0;i<10;++i){SDL_Delay(6);simulator.ProcessEvents();}}
}
void CheckPerformanceEditor(UI& editor,SdlSimulator& simulator) {
    const auto authored=editor.ProjectDocument();
    json document={{"canvas",{{"width",480},{"height",320}}},{"variables",json::array()},
        {"obj",json::array({
            {{"oid",0},{"par",-1},{"base",{{"coords",{{"x",16},{"y",16},{"width",320},{"height",48}}}}},{"textarea",{{"text",""},{"oneLine",true}}}},
            {{"oid",1},{"par",-1},{"base",{{"coords",{{"x",16},{"y",150},{"width",448},{"height",154}}}}},{"advancedKeyboard",{{"targetOid",0},{"autoFocus",true},{"trieId","builtin-en-us"}}}}
        })}};
    RunPreview preview;preview.Start(document,{},[]{});Require(preview.Running(),"Performance test runtime did not start");
    auto* root=lvgl_runtime_screen(preview.Runtime(),0);auto* display=lv_obj_get_display(root);
    lv_obj_invalidate(root);lv_refr_now(display);Require(preview.Metrics().frames==0,"Disabled profiling performed frame collection");
    preview.SetProfilingEnabled(true);lv_obj_invalidate(root);lv_refr_now(display);const auto measured=preview.Metrics();
    Require(measured.frames>0&&measured.frame.samples>0,"Actual display rendering was not measured");
    Require(measured.inputBytes>0&&measured.framebufferBytes==480u*320u*4u,"Performance byte counts are not the real input/framebuffer sizes");
    Require(measured.loadedScreens==1&&measured.loadedObjects==2,"Runtime object/screen metrics are incorrect");
    SDL_Event text{};text.type=SDL_EVENT_TEXT_INPUT;text.text.text="hel";Require(preview.HandleEvent(text),"Run preview did not accept its test typing");Pump(simulator);
    Require(preview.Metrics().key.samples>=3,"Input processing timings were not collected");
    const auto before=std::string(lv_textarea_get_text(lvgl_runtime_object(preview.Runtime(),0,0)));
    const auto prediction=preview.BenchmarkPrediction("hel",100);Require(prediction.samples==100,"Predictor benchmark did not run real bounded lookups");
    Require(std::string(lv_textarea_get_text(lvgl_runtime_object(preview.Runtime(),0,0)))==before,"Prediction measurement modified typed content");
    PerformanceWindow::Open({[&]{return preview.Metrics();},[&](bool value){preview.SetProfilingEnabled(value);},[&](const std::string& prefix,unsigned count){return preview.BenchmarkPrediction(prefix,count);},nullptr});
    Require(PerformanceWindow::ActiveWindow(),"Performance dashboard did not open");Pump(simulator);PerformanceWindow::Close();
    Require(!preview.Metrics().enabled,"Closing the dashboard left profiling enabled");
    preview.Stop();Require(!preview.Metrics().running,"Performance report retained a destroyed runtime");
    Require(editor.ProjectDocument()==authored,"Performance preview or dashboard changed the authored document");
    std::cout<<"Measured render/input/prediction timings, allocator scope, disabled overhead, dashboard lifecycle and authoring isolation passed\n";
}
