#include "RunPreview.h"
#include "EditorWindow.h"
#include "../../CodeGeneration/ProjectAssets.h"
#include <Serialization/Protobuf.h>
#include <Serialization/ProjectDesign.h>
#include <algorithm>
#include <deque>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <array>
#include <Serialization/AdvancedKeyboard.h>
#include <Serialization/FocusOrder.h>
#include "../../LVSerial/Widgets/AdvancedKeyboard/Utf8Fold.h"
#include <lv_advanced_keyboard_prediction.h>

namespace {
uint64_t NowUs(){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
}
struct RunPreview::Impl {
    lv_display_t* display = nullptr;
    lvgl_runtime_t* runtime = nullptr;
    lv_obj_t *overlay = nullptr, *image = nullptr, *active = nullptr;
    lv_group_t* group = nullptr;
    lv_indev_t *pointer = nullptr, *keyboard = nullptr;
    lv_timer_t* timer = nullptr;
    lv_image_dsc_t descriptor{};
    std::vector<uint32_t> pixels;
    std::vector<lv_obj_t*> focusable;
    std::vector<std::vector<int>> authoredIds;
    std::deque<lv_indev_data_t> touches;
    std::deque<uint32_t> keys;
    lv_indev_data_t touch{};
    uint32_t key = 0;
    bool keyDown = false;
    int width = 0, height = 0;
    std::function<void()> close;
    bool profiling=false;
    uint64_t startedUs=0,frameStartedUs=0,renderStartedUs=0,flushStartedUs=0,renderUs=0,flushUs=0,frames=0,pixelsDrawn=0;
    Performance::Series frameSeries,renderSeries,flushSeries,keySeries,predictionSeries;
    void ResetMetrics(){startedUs=NowUs();frameStartedUs=renderStartedUs=flushStartedUs=renderUs=flushUs=frames=pixelsDrawn=0;frameSeries.Clear();renderSeries.Clear();flushSeries.Clear();keySeries.Clear();predictionSeries.Clear();}
    void Remember(lv_obj_t* object) {
        if(!object||std::find(focusable.begin(),focusable.end(),object)!=focusable.end())return;
        focusable.push_back(object);
        lv_obj_add_event_cb(object,[](lv_event_t* event){
            auto* self=static_cast<Impl*>(lv_event_get_user_data(event));
            for(auto& item:self->focusable)if(item==lv_event_get_current_target_obj(event))item=nullptr;
            self->active=nullptr;
        },LV_EVENT_DELETE,this);
    }


    void SyncFocus() {
        auto* screen = lv_display_get_screen_active(display);
        // New lazy screens join the runtime's default group when constructed.
        // Remember those controls before rebuilding the active-screen group.
        for(uint32_t i=0;i<lv_group_get_obj_count(group);++i)Remember(lv_group_get_obj_by_index(group,i));
        if (active == screen) return;
        auto* focused=lv_group_get_focused(group);active = screen; lv_group_remove_all_objs(group);
        focusable.erase(std::remove(focusable.begin(),focusable.end(),nullptr),focusable.end());
        for (auto* object : focusable) if (lv_obj_get_screen(object) == active) lv_group_add_obj(group, object);
        std::vector<lv_obj_t*> authored;const auto si=lvgl_runtime_active_screen(runtime);
        if(si<authoredIds.size())for(int id:authoredIds[si])if(auto* object=lvgl_runtime_object(runtime,si,id))authored.push_back(object);
        Serialization::Accessibility::ApplyFocusOrder(group,authored);
        if(focused&&std::find(focusable.begin(),focusable.end(),focused)!=focusable.end()&&lv_obj_get_screen(focused)==active&&lv_obj_get_group(focused)==group)lv_group_focus_obj(focused);
    }
    void Pump() {
        SyncFocus();
        while (!touches.empty()) {
            touch = touches.front(); touches.pop_front();const auto begin=profiling?NowUs():0;
            lv_indev_read(pointer);if(profiling)keySeries.Add(double(NowUs()-begin)/1000.0);SyncFocus();
        }
        while (!keys.empty()) {
            key = keys.front(); keys.pop_front();keyDown=true;const auto begin=profiling?NowUs():0;
            lv_indev_read(keyboard);keyDown=false;lv_indev_read(keyboard);
            if(profiling)keySeries.Add(double(NowUs()-begin)/1000.0);SyncFocus();
        }
    }
};
RunPreview::RunPreview():state(std::make_unique<Impl>()) {}
RunPreview::~RunPreview() { Stop(); }
bool RunPreview::Running() const { return state->runtime != nullptr; }
lvgl_runtime_t* RunPreview::Runtime() const { return state->runtime; }
lv_obj_t* RunPreview::Image() const { return state->image; }
void RunPreview::Stop() {
    auto& s = *state;
    if (s.timer) { lv_timer_delete(s.timer); s.timer = nullptr; }
    if (s.pointer) { lv_indev_delete(s.pointer); s.pointer = nullptr; }
    if (s.keyboard) { lv_indev_delete(s.keyboard); s.keyboard = nullptr; }
    if (s.overlay) { EditorWindow::ReleaseFocus(s.overlay); lv_obj_delete(s.overlay); s.overlay = nullptr; s.image = nullptr; }
    if (s.runtime) { lvgl_runtime_destroy(s.runtime); s.runtime = nullptr; }
    if (s.group) { lv_group_delete(s.group); s.group = nullptr; }
    if (s.display) { lv_display_delete(s.display); s.display = nullptr; }
    s.pixels.clear(); s.focusable.clear(); s.authoredIds.clear(); s.touches.clear(); s.keys.clear(); s.active = nullptr; s.close = {};
}
void RunPreview::Start(nlohmann::json document, const std::string& sourceDirectory, std::function<void()> close) {
    Stop(); auto& s = *state;s.ResetMetrics();
    Serialization::ProjectDesign::Materialize(document, true);
    CGeneration::EmbedLocalImages(document, std::filesystem::u8path(sourceDirectory));
    document["runtimeTarget"] = "9.6";
    const auto screenIndex=document.value("activeScreen",size_t(0));
    const auto canvas = document.contains("screens")&&screenIndex<document["screens"].size()?
        document["screens"][screenIndex].value("canvas",nlohmann::json::object()):document.value("canvas", nlohmann::json::object());
    s.width = canvas.value("width", 320); s.height = canvas.value("height", 240);
    if (s.width < 1 || s.height < 1 || s.width > 8192 || s.height > 8192 || uint64_t(s.width)*s.height > 16777216)
        throw std::invalid_argument("Run screen dimensions must fit within 8192 pixels per side and 16 million pixels.");
    const auto screens=document.contains("screens")?document.at("screens"):nlohmann::json::array({document});
    for(const auto& screen:screens){std::vector<int> ids;int ordinal=0;for(const auto& record:screen.at("obj"))ids.push_back(record.value("oid",ordinal++));s.authoredIds.push_back(std::move(ids));}
    const auto bytes = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", document);
    auto* editorDisplay = lv_display_get_default(); auto* editorGroup = lv_group_get_default();
    try {
        s.pixels.resize(size_t(s.width)*s.height);
        s.display = lv_display_create(s.width, s.height);
        if (!s.display) throw std::runtime_error("Cannot allocate Run display");
        lv_display_set_default(editorDisplay);
        lv_display_set_color_format(s.display, LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_user_data(s.display, &s);
        lv_display_add_event_cb(s.display,[](lv_event_t* event){
            auto* self=static_cast<Impl*>(lv_event_get_user_data(event));if(!self->profiling)return;
            const auto now=NowUs();switch(lv_event_get_code(event)) {
            case LV_EVENT_RENDER_START:self->renderStartedUs=now;if(!self->frameStartedUs)self->frameStartedUs=now;break;
            case LV_EVENT_RENDER_READY:if(self->renderStartedUs)self->renderUs+=now-self->renderStartedUs;self->renderStartedUs=0;break;
            case LV_EVENT_FLUSH_START:self->flushStartedUs=now;break;
            case LV_EVENT_FLUSH_FINISH:if(self->flushStartedUs)self->flushUs+=now-self->flushStartedUs;self->flushStartedUs=0;break;
            case LV_EVENT_REFR_READY:if(self->frameStartedUs){++self->frames;self->frameSeries.Add(double(now-self->frameStartedUs)/1000.0);self->renderSeries.Add(double(self->renderUs)/1000.0);self->flushSeries.Add(double(self->flushUs)/1000.0);self->frameStartedUs=self->renderUs=self->flushUs=0;}break;
            default:break;
            }
        },LV_EVENT_ALL,&s);
        lv_display_set_flush_cb(s.display, [](lv_display_t* display, const lv_area_t* area, uint8_t*) {
            auto* self = static_cast<Impl*>(lv_display_get_user_data(display));
            if(self->profiling)self->pixelsDrawn+=uint64_t(lv_area_get_width(area))*lv_area_get_height(area);
            if (self->image) lv_obj_invalidate(self->image);
            lv_display_flush_ready(display);
        });
        lv_display_set_buffers(s.display, s.pixels.data(), nullptr, uint32_t(s.pixels.size()*4), LV_DISPLAY_RENDER_MODE_DIRECT);
        s.group = lv_group_create(); lv_group_set_default(s.group);
        lvgl_runtime_options_t options{s.display, 0, true}; char error[1024]{};
        const bool loaded = lvgl_runtime_load_memory_ex(bytes.data(), bytes.size(), &options,
            LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE,&s.runtime,error,sizeof(error));
        lv_group_set_default(editorGroup);
        if (!loaded) throw std::runtime_error(error);
        for(uint32_t i=0;i<lv_group_get_obj_count(s.group);++i)s.Remember(lv_group_get_obj_by_index(s.group,i));
        s.pointer = lv_indev_create(); lv_indev_set_type(s.pointer, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(s.pointer, s.display); lv_indev_set_user_data(s.pointer, &s);
        lv_indev_set_mode(s.pointer, LV_INDEV_MODE_EVENT);
        lv_indev_set_read_cb(s.pointer, [](lv_indev_t* input, lv_indev_data_t* data) { *data = static_cast<Impl*>(lv_indev_get_user_data(input))->touch; });
        s.keyboard = lv_indev_create(); lv_indev_set_type(s.keyboard, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_display(s.keyboard, s.display); lv_indev_set_group(s.keyboard, s.group);
        lv_indev_set_user_data(s.keyboard, &s); lv_indev_set_mode(s.keyboard, LV_INDEV_MODE_EVENT);
        lv_indev_set_read_cb(s.keyboard, [](lv_indev_t* input, lv_indev_data_t* data) {
            auto* self = static_cast<Impl*>(lv_indev_get_user_data(input)); data->key = self->key;
            data->state = self->keyDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        });
        s.close = std::move(close);
        // Keep the menu bar available while the modal blocks editing the design.
        s.overlay = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(s.overlay);
        lv_obj_set_pos(s.overlay, 0, 36); lv_obj_set_size(s.overlay, LV_PCT(100), lv_display_get_vertical_resolution(editorDisplay)-36);
        lv_obj_set_style_bg_color(s.overlay, lv_color_hex(0x10151e), 0); lv_obj_set_style_bg_opa(s.overlay, LV_OPA_90, 0);
        lv_obj_set_scrollable(s.overlay, false);
        auto* window = EditorWindow::Create(s.overlay);
        lv_obj_set_size(window, (std::min)(s.width+36, lv_display_get_horizontal_resolution(editorDisplay)-48),
            (std::min)(s.height+EditorWindow::HeaderHeight+36, lv_display_get_vertical_resolution(editorDisplay)-84));
        lv_obj_center(window);
        EditorWindow::SetTitle(window, "Run screen  |  F6 or Escape returns to Design");
        EditorWindow::SetCloseHandler(window, [&s]{ if (s.close) { auto action=s.close; action(); } });
        auto* content = EditorWindow::Content(window); lv_obj_set_scroll_dir(content, LV_DIR_ALL);
        lv_obj_set_style_pad_all(content, 16, 0); lv_obj_set_style_bg_color(content, lv_color_hex(0x111820), 0);
        s.descriptor = {}; s.descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        s.descriptor.header.cf = LV_COLOR_FORMAT_XRGB8888; s.descriptor.header.w = s.width; s.descriptor.header.h = s.height;
        s.descriptor.header.stride = s.width*4; s.descriptor.data_size = uint32_t(s.pixels.size()*4);
        s.descriptor.data = reinterpret_cast<const uint8_t*>(s.pixels.data());
        s.image = lv_image_create(content); lv_image_set_src(s.image, &s.descriptor);
        lv_obj_set_clickable(s.image, true); lv_obj_set_click_focusable(s.image, false);
        lv_obj_set_scrollable(s.image, false); lv_obj_set_press_lock(s.image, true);
        lv_obj_add_event_cb(s.image, [](lv_event_t* event) {
            auto* self = static_cast<Impl*>(lv_event_get_user_data(event)); const auto code = lv_event_get_code(event);
            if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
            auto* input = lv_indev_active(); if (!input) return;
            lv_point_t point{}; lv_indev_get_point(input, &point);
            lv_obj_transform_point(self->image, &point, LV_OBJ_POINT_TRANSFORM_FLAG_INVERSE_RECURSIVE);
            lv_area_t area{}; lv_obj_get_coords(self->image, &area);
            lv_indev_data_t data{}; data.point = {std::clamp(point.x-area.x1, 0, self->width-1), std::clamp(point.y-area.y1, 0, self->height-1)};
            data.state = code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
            if (self->touches.size() < 128) self->touches.push_back(data);
        }, LV_EVENT_ALL, &s);
        s.SyncFocus(); lv_refr_now(s.display);
        s.timer = lv_timer_create([](lv_timer_t* timer){ static_cast<Impl*>(lv_timer_get_user_data(timer))->Pump(); }, 5, &s);
    } catch (...) { lv_group_set_default(editorGroup); lv_display_set_default(editorDisplay); Stop(); throw; }
}
bool RunPreview::HandleEvent(const SDL_Event& event) {
    if (!Running()) return false;
    auto& s = *state;
    // Floating tools above Run retain native text input in their own fields.
    auto* layer=lv_obj_get_parent(s.overlay);
    for(uint32_t i=uint32_t(lv_obj_get_index(s.overlay)+1);i<lv_obj_get_child_count(layer);++i)
        if(!lv_obj_is_hidden(lv_obj_get_child(layer,i)))return false;
    if (event.type == SDL_EVENT_TEXT_INPUT) {
        const auto* p = reinterpret_cast<const unsigned char*>(event.text.text);
        while (p && *p && s.keys.size() < 1024) {
            uint32_t cp = *p++; int following = 0;
            if (cp >= 0xf0) { cp &= 7; following=3; } else if (cp >= 0xe0) {cp &= 15; following=2;} else if (cp >= 0xc0) {cp &= 31; following=1;}
            while (following-- && *p) cp=(cp<<6)|(*p++&63);
            if (cp >= 32 && cp <= 0x10ffff) s.keys.push_back(cp);
        }
        return true;
    }
    if (event.type == SDL_EVENT_TEXT_EDITING || event.type == SDL_EVENT_KEY_UP) return true;
    if (event.type != SDL_EVENT_KEY_DOWN) return false;
    if (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) return true;
    uint32_t key = 0;
    switch (event.key.key) {
    case SDLK_BACKSPACE: key=LV_KEY_BACKSPACE; break; case SDLK_DELETE: key=LV_KEY_DEL; break;
    case SDLK_RETURN: case SDLK_KP_ENTER: key=LV_KEY_ENTER; break;
    case SDLK_LEFT: key=LV_KEY_LEFT; break; case SDLK_RIGHT: key=LV_KEY_RIGHT; break;
    case SDLK_UP: key=LV_KEY_UP; break; case SDLK_DOWN: key=LV_KEY_DOWN; break;
    case SDLK_HOME: key=LV_KEY_HOME; break; case SDLK_END: key=LV_KEY_END; break;
    case SDLK_TAB: key=(event.key.mod&SDL_KMOD_SHIFT)?LV_KEY_PREV:LV_KEY_NEXT; break;
    default: break;
    }
    if (key && s.keys.size()<1024) s.keys.push_back(key);
    return true;
}

Performance::Snapshot RunPreview::Metrics() const {
    const auto& s=*state;Performance::Snapshot out;out.running=Running();out.enabled=s.profiling;
    if(!out.running)return out;
    out.frames=s.frames;out.redrawPixels=s.pixelsDrawn;out.framebufferBytes=s.pixels.size()*sizeof(uint32_t);
    out.elapsedSeconds=s.startedUs?double(NowUs()-s.startedUs)/1000000.0:0;
    out.fpsAvailable=s.profiling&&out.elapsedSeconds>0;out.fps=out.fpsAvailable?double(out.frames)/out.elapsedSeconds:0;
    out.frame=s.frameSeries.Summary();out.render=s.renderSeries.Summary();out.flush=s.flushSeries.Summary();
    out.key=s.keySeries.Summary();out.prediction=s.predictionSeries.Summary();
    lvgl_runtime_stats_t stats{};if(lvgl_runtime_get_stats(s.runtime,&stats)) {
        out.inputBytes=stats.input_bytes;out.totalScreens=stats.screen_count;out.loadedScreens=stats.loaded_screens;out.loadedObjects=stats.loaded_objects;
        out.loadMs=double(stats.load_us)/1000.0;out.lastScreenLoadMs=double(stats.last_screen_load_us)/1000.0;
        out.heapAvailable=stats.lvgl_heap_available;out.heapUsed=stats.lvgl_heap_used;out.heapPeak=stats.lvgl_heap_peak;out.largestFree=stats.lvgl_heap_largest_free;
    }
    return out;
}
void RunPreview::SetProfilingEnabled(bool enabled){auto& s=*state;if(s.profiling==enabled)return;s.profiling=enabled;s.ResetMetrics();}
Performance::Distribution RunPreview::BenchmarkPrediction(const std::string& input,unsigned iterations) {
    auto& s=*state;if(!s.runtime)throw std::invalid_argument("Start Run mode before measuring its predictor");
    if(input.size()>512||!iterations||iterations>1000)throw std::invalid_argument("Use a prefix up to 512 UTF-8 bytes and 1-1000 iterations");
    lv_advanced_keyboard_predictor_t* predictor=nullptr;
    std::function<void(lv_obj_t*)> visit=[&](lv_obj_t* object){
        if(predictor)return;
        if(Serialization::AdvancedKeyboard::Type(object))predictor=lv_advanced_keyboard_get_predictor(object);
        for(uint32_t i=0;!predictor&&i<lv_obj_get_child_count(object);++i)visit(lv_obj_get_child(object,i));
    };
    visit(lv_display_get_screen_active(s.display));if(!predictor)throw std::invalid_argument("The current screen has no Advanced Keyboard with a prediction model");
    std::array<char,2048> buffer{};std::string_view folded;
    if(!espos::keyboard::prediction::FoldUtf8(input,buffer.data(),buffer.size(),folded))throw std::invalid_argument("Prefix cannot be Unicode-folded");
    Performance::Series result;lv_advanced_keyboard_suggestion_t words[3]{};
    for(unsigned i=0;i<iterations;++i){const auto begin=NowUs();lv_advanced_keyboard_predictor_suggest_folded(predictor,folded.data(),folded.size(),nullptr,0,nullptr,0,words,3);const double elapsed=double(NowUs()-begin)/1000.0;result.Add(elapsed);if(s.profiling)s.predictionSeries.Add(elapsed);}
    return result.Summary();
}
