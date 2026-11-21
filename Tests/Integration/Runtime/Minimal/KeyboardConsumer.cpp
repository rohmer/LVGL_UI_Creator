#include <lvgl_pb_esp32.h>
#include <lv_advanced_keyboard.h>
#include <lv_advanced_keyboard_prediction.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#if !LVSERIAL_USE_ADVANCED_KEYBOARD || !LV_USE_BUTTON || !LV_USE_LABEL || !LV_USE_TEXTAREA
#error Advanced Keyboard requires its implementation, button, label and textarea
#endif
#if LV_USE_KEYBOARD || LV_USE_BUTTONMATRIX || LV_USE_DROPDOWN || LV_USE_FLEX || LV_USE_CHART || LV_USE_CANVAS || LV_USE_IMAGE
#error Unrelated widgets and layouts must remain excluded from this profile
#endif

namespace {
void Require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
size_t flushes=0, changes=0;
void Flush(lv_display_t* display, const lv_area_t*, uint8_t*) { ++flushes;lv_display_flush_ready(display); }
void Changed(lv_event_t*) { ++changes; }
}

int main()
{
    try {
        const auto types=Serialization::ObjectSerializer::SupportedTypes();
        Require(std::find(types.begin(),types.end(),"advancedKeyboard")!=types.end(),"Advanced Keyboard missing from the minimal registry");
        Require(std::find(types.begin(),types.end(),"keyboard")==types.end(),"Built-in keyboard entered the minimal registry");
        Require(lvgl_pb_embedded_project_count==1,"Expected one keyboard fixture");
        const auto& input=lvgl_pb_embedded_projects[0];
        lv_init();
        auto* display=lv_display_create(640,400);Require(display,"Cannot create display");
        lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
        const size_t bufferBytes=640u*16u*2u;
        auto* pixels=std::malloc(bufferBytes);Require(pixels,"Cannot allocate display buffer");
        lv_display_set_buffers(display,pixels,nullptr,static_cast<uint32_t>(bufferBytes),LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display,Flush);
        auto* previous=lv_screen_active();
        const auto unsupported=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",
            {{"obj",{{{"oid",0},{"par",-1},{"base",nlohmann::json::object()},{"chart",nlohmann::json::object()}}}}});
        for(int cycle=0;cycle<3;++cycle) {
            char error[512]{};lvgl_runtime_t* runtime=nullptr;
            Require(lvgl_runtime_load_memory(input.data,input.size,nullptr,&runtime,error,sizeof error),error);
            auto* keyboard=lvgl_runtime_object(runtime,0,7);
            Require(keyboard && lv_advanced_keyboard_is_instance(keyboard),"Runtime did not create the native typing view");
            auto* textarea=lvgl_runtime_object(runtime,0,8);
            Require(textarea && lv_obj_get_class(textarea)==&lv_textarea_class,"External textarea missing");
            Require(lv_advanced_keyboard_bound_textarea_count(keyboard)==1,"External textarea was not registered");
            Require(!lv_advanced_keyboard_get_textarea(keyboard) && lv_obj_has_flag(keyboard,LV_OBJ_FLAG_HIDDEN),"Keyboard appeared before a textarea received focus");
            lv_obj_add_state(textarea,LV_STATE_FOCUSED);lv_obj_send_event(textarea,LV_EVENT_FOCUSED,nullptr);
            Require(lv_advanced_keyboard_get_textarea(keyboard)==textarea && !lv_obj_has_flag(keyboard,LV_OBJ_FLAG_HIDDEN),"Focus did not activate the keyboard popup");
            Require(lv_advanced_keyboard_get_keyboard(keyboard),"Native keys missing");
            auto* predictor=lv_advanced_keyboard_get_predictor(keyboard);
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
            Require(predictor && lv_advanced_keyboard_predictor_node_count(predictor)>0,"Built-in trie predictor missing");
            lv_advanced_keyboard_suggestion_t suggestions[3]{};
            Require(lv_advanced_keyboard_predictor_suggest_folded(predictor,"th",2,nullptr,0,nullptr,0,suggestions,3)>0,"Embedded trie did not provide real suggestions");
#else
            Require(!predictor,"A predictor entered the no-model profile");
#endif
            Require(std::strcmp(lv_advanced_keyboard_get_text(keyboard),"Hello")==0,"Initial variable did not reach the editor");
            Require(std::strcmp(lv_textarea_get_placeholder_text(textarea),"Type a message")==0,"Placeholder was not restored");
            Require(lv_textarea_get_max_length(textarea)==64,"Maximum length was not restored");
            lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_STRING;value.string_value="Bound text";
            Require(lvgl_runtime_set_variable(runtime,"message",&value),"Cannot set the bound text variable");
            Require(std::strcmp(lv_advanced_keyboard_get_text(keyboard),"Bound text")==0,"Variable setter missed the typing view");
            changes=0;lv_obj_add_event_cb(keyboard,Changed,LV_EVENT_VALUE_CHANGED,nullptr);
            lv_textarea_set_text(textarea,"Typed text");
            Require(changes>0,"Editor changes were not propagated to the widget root");
            Require(lvgl_runtime_get_variable(runtime,"message",&value) && value.type==LVGL_RUNTIME_STRING &&
                    std::strcmp(value.string_value,"Typed text")==0,"Editor changes did not update the bound variable");
            const auto saved=Serialization::ObjectSerializer::SerializeObject(keyboard);
            Require(saved.contains("advancedKeyboard") && !saved["advancedKeyboard"].contains("text") &&
                    saved["advancedKeyboard"]["targetOid"]==8,"External textarea association serialization changed");
            auto* root=lvgl_runtime_screen(runtime,0);Require(root,"Loaded screen missing");
            lv_obj_update_layout(root);const auto previousFlushes=flushes;
            lv_obj_invalidate(root);lv_tick_inc(160);lv_timer_handler();lv_refr_now(display);
            Require(flushes>previousFlushes,"Advanced Keyboard did not render to the display");
            lvgl_runtime_t* rejected=reinterpret_cast<lvgl_runtime_t*>(1);
            Require(!lvgl_runtime_load_memory(unsupported.data(),unsupported.size(),nullptr,&rejected,error,sizeof error) && !rejected,"Disabled chart was accepted");
            Require(lv_screen_active()==root,"Rejected data changed the active keyboard screen");
            lvgl_runtime_destroy(runtime);
            Require(lv_screen_active()==previous,"Destroy did not restore the previous screen");
        }
        lv_display_delete(display);lv_deinit();std::free(pixels);
        std::puts("Minimal Advanced Keyboard link/render, Protobuf settings, two-way text binding, root events and lifetimes passed");
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
