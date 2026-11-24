#include <RuntimeCompat.h>
#include "lvgl_runtime.h"
#include <Serialization/Protobuf.h>
#include <Serialization/CustomWidgets.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error(std::string("Line ")+std::to_string(__LINE__)+": " #x);}while(0)
using namespace Serialization;
#if LVGL_VERSION_MAJOR == 8
static void Flush(lv_disp_drv_t* d,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(d);}
#else
static void Flush(lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);}
#endif
void CheckChartCursorCoordinates() {
    using Coordinate = decltype(lv_point_t{}.x);
    auto* chart = lv_chart_create(lv_screen_active());
    json data{{"cursors", json::array({{{"positionSet", true},
        {"x", (std::numeric_limits<int64_t>::lowest)()}, {"y", (std::numeric_limits<uint64_t>::max)()}}})}};
    const auto position = [chart] {
        auto* cursor = static_cast<lv_chart_cursor_t*>(lv_ll_get_head(&reinterpret_cast<lv_chart_t*>(chart)->cursor_ll));
        CHECK(cursor); return lv_chart_get_cursor_point(chart, cursor);
    };
    ExtendedWidgets::Apply(chart, data, false);
    auto saved = position();
    CHECK(saved.x == (std::numeric_limits<Coordinate>::lowest)());
    CHECK(saved.y == (std::numeric_limits<Coordinate>::max)());
    data["cursors"][0]["x"] = 12.75; data["cursors"][0]["y"] = -9.5;
    ExtendedWidgets::Apply(chart, data, false);
    saved = position();
    CHECK(saved.x == 12); CHECK(saved.y == -9);
    data["cursors"][0].erase("x"); data["cursors"][0].erase("y");
    ExtendedWidgets::Apply(chart, data, false);
    saved = position();
    CHECK(saved.x == 0); CHECK(saved.y == 0);
    lv_obj_delete(chart);
}
void Inspect(lvgl_runtime_t* runtime,const json& doc) {
    auto screens=doc.contains("screens")?doc.at("screens"):json::array({doc});
    CHECK(lvgl_runtime_screen_count(runtime)==screens.size());
    for(size_t s=0;s<screens.size();++s) {
        CHECK(lvgl_runtime_show_screen(runtime,s));
        int next=0;
        for(auto& record:screens[s].at("obj")) {
            auto* o=lvgl_runtime_object(runtime,s,record.value("oid",next++));CHECK(o);
            if(record.contains("arc")) {CHECK(lv_obj_has_class(o,&lv_arc_class));CHECK(lv_arc_get_value(o)==record["arc"].value("value",0));}
            if(record.contains("bar")) {CHECK(lv_obj_has_class(o,&lv_bar_class));int expected=record["bar"].value("value",0);
                for(auto& item:doc.value("variables",json::array())) {auto v=item.at("var");if(v.value("oid",-1)==record.value("oid",-2) && v.value("property",std::string{})=="/bar/value") {lvgl_runtime_value_t value{};CHECK(lvgl_runtime_get_variable(runtime,v.at("name").get<std::string>().c_str(),&value));expected=(int)value.integer_value;}}
                CHECK(lv_bar_get_value(o)==expected);}
            if(record.contains("cb")) {CHECK(lv_obj_has_class(o,&lv_checkbox_class));}
            if(record.contains("text")) CHECK(lv_obj_has_class(o,&lv_label_class));
            if(record.contains("slider")) CHECK(lv_obj_has_class(o,&lv_slider_class));
            if(record.contains("dropdown"))CHECK(lv_obj_has_class(o,&lv_dropdown_class));
            if(record.contains("roller"))CHECK(lv_obj_has_class(o,&lv_roller_class));
            if(record.contains("chart")) {CHECK(lv_obj_has_class(o,&lv_chart_class));CHECK(lv_chart_get_point_count(o)==record["chart"].value("pointCount",10));}
            if(record.contains("table"))CHECK(lv_obj_has_class(o,&lv_table_class));
            if(record.contains("imagebutton")) {
                auto states=record["imagebutton"].value("states",json::array());
                for(size_t state=0;state<states.size();++state) for(size_t side=0;side<states[state].size();++side) {
                    auto source=states[state][side];if(!source.is_object() || !source.contains("asset") || !source["asset"].contains("projectImage"))continue;
                    auto* expected=lvgl_runtime_image(runtime,source["asset"]["projectImage"].get<std::string>().c_str());CHECK(expected);
                    const void* sources[]={lv_imagebutton_get_src_left(o,(lv_imagebutton_state_t)state),lv_imagebutton_get_src_middle(o,(lv_imagebutton_state_t)state),lv_imagebutton_get_src_right(o,(lv_imagebutton_state_t)state)};CHECK(sources[side]==expected);
                }
            }
            if(record.contains("textBox"))CHECK(CustomWidgets::Type(o)!=nullptr);
            if(record.contains("layerView"))CHECK(CustomWidgets::Type(o)!=nullptr);
        }
        lv_obj_update_layout(lvgl_runtime_screen(runtime,s));lv_refr_now(nullptr);
    }
    for(size_t i=0;i<lvgl_runtime_variable_count(runtime);++i) {
        const char* name=lvgl_runtime_variable_name(runtime,i);CHECK(name);
        lvgl_runtime_value_t value{};CHECK(lvgl_runtime_get_variable(runtime,name,&value));
        CHECK(lvgl_runtime_set_variable(runtime,name,&value));
        if(std::string(name)=="level") {
            value.integer_value=55;CHECK(lvgl_runtime_set_variable(runtime,name,&value));
            lv_obj_t* bar=nullptr;
            for(const auto& item:doc.at("variables")) {
                const auto& variable=item.at("var");
                if(variable.at("name")==name)
                    bar=lvgl_runtime_object(runtime,variable.value("screen",size_t(0)),variable.at("oid").get<int>());
            }
            CHECK(bar);CHECK(lv_obj_has_class(bar,&lv_bar_class));CHECK(lv_bar_get_value(bar)==55);
            lv_bar_set_value(bar,56,LV_ANIM_OFF);lv_obj_send_event(bar,LV_EVENT_VALUE_CHANGED,nullptr);
            CHECK(lvgl_runtime_get_variable(runtime,name,&value));CHECK(value.integer_value==56);
        }
    }
}
int main(int argc,char** argv) {
 try {
    CHECK(argc==3);std::ifstream file(argv[1],std::ios::binary);Binary bytes((std::istreambuf_iterator<char>(file)),{});CHECK(!bytes.empty());
    auto doc=Protobuf::Decode(bytes).data;lv_init();
    static uint32_t pixels[800*32];
#if LVGL_VERSION_MAJOR == 8
    static lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,pixels,nullptr,800*32);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=800;driver.ver_res=600;driver.flush_cb=Flush;driver.draw_buf=&draw;
    auto* display=lv_disp_drv_register(&driver);
#else
    auto* display=lv_display_create(800,600);lv_display_set_flush_cb(display,Flush);
    lv_display_set_buffers(display,pixels,nullptr,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    CheckChartCursorCoordinates();
    auto* previous=lv_screen_active();char error[512];
    for(int iteration=0;iteration<10;++iteration) {
        lvgl_runtime_t* runtime=nullptr;bool loaded=lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&runtime,error,sizeof error);
        if(!loaded)throw std::runtime_error(error);CHECK(runtime);Inspect(runtime,doc);
        if(std::string(argv[2])=="assets") {
            CHECK(lvgl_runtime_image(runtime,"image_1"));auto* font=lvgl_runtime_font(runtime,"font_1");CHECK(font);
            lv_font_glyph_dsc_t glyph{};CHECK(lv_font_get_glyph_dsc(font,&glyph,'A','V'));CHECK(glyph.adv_w>0);
        }
        lvgl_runtime_destroy(runtime);CHECK(lv_screen_active()==previous);
    }
#if LVGL_VERSION_MAJOR == 8
    lv_disp_remove(display);
#else
    lv_display_delete(display);lv_deinit();
#endif
    std::cout<<"Version runtime loaded, rendered and destroyed project ten times\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
