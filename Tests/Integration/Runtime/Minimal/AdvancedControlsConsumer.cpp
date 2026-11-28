#include <lvgl_pb_esp32.h>
#include <Serialization/AdvancedControls.h>
#include <Serialization/ObjectSerializer.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#if !LVSERIAL_USE_RADIO_GROUP || !LVSERIAL_USE_CHECKBOX_BUTTON || !LVSERIAL_USE_LABELED_CHECKBOX || !LVSERIAL_USE_DATE_PICKER || !LVSERIAL_USE_MENU
#error Selected advanced control implementations must be present
#endif
#if !LV_USE_BUTTON || !LV_USE_LABEL || !LV_USE_CHECKBOX || !LV_USE_CALENDAR || !LV_USE_BUTTONMATRIX || !LV_USE_FLEX
#error Advanced controls require their native primitive widgets
#endif
#if LV_USE_MENU || LV_USE_TEXTAREA || LV_USE_CHART || LV_USE_CANVAS || LV_USE_IMAGE || LVSERIAL_USE_ADVANCED_KEYBOARD
#error Unrelated widget implementations must stay excluded
#endif
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
unsigned flushes = 0;
void Flush(lv_display_t* display, const lv_area_t*, uint8_t*) { ++flushes; lv_display_flush_ready(display); }
}
int main() {
    try {
        using namespace Serialization;
        Check(lvgl_pb_embedded_project_count == 1, "One composite fixture must be embedded");
        lv_init();
        auto* display = lv_display_create(640, 400);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        auto* pixels = std::malloc(640 * 16 * 2);
        Check(pixels, "Display buffer allocation failed");
        lv_display_set_buffers(display, pixels, nullptr, 640 * 16 * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, Flush);
        const auto types = ObjectSerializer::SupportedTypes();
        for (const auto* type : {"radioGroup", "checkboxButton", "labeledCheckbox", "datePicker", "menu"})
            Check(std::find(types.begin(), types.end(), type) != types.end(), "Selected composite is absent from minimal registry");
        Check(std::find(types.begin(), types.end(), "chart") == types.end(), "Chart entered the minimal registry");
        const auto& project = lvgl_pb_embedded_projects[0];
        for (int cycle = 0; cycle < 3; ++cycle) {
            lvgl_runtime_t* runtime = nullptr;
            char error[512]{};
            Check(lvgl_runtime_load_memory(project.data, project.size, nullptr, &runtime, error, sizeof error), error);
            for (int id = 1; id <= 5; ++id)
                Check(AdvancedControls::Type(lvgl_runtime_object(runtime, 0, id)), "Runtime constructed a plain object instead of a composite");
            auto* radio = lvgl_runtime_object(runtime, 0, 1);
            lv_obj_send_event(AdvancedControls::Part(radio, "Option1"), LV_EVENT_CLICKED, nullptr);
            Check(AdvancedControls::ToJSON(radio).at("radioGroup").at("selected") == 1, "Radio selection did not work");
            auto* date = lvgl_runtime_object(runtime, 0, 4);
            lv_obj_send_event(AdvancedControls::Part(date, "Display"), LV_EVENT_CLICKED, nullptr);
            Check(AdvancedControls::Part(date, "Calendar"), "Lazy calendar implementation was not linked");
            lv_tick_inc(20); lv_timer_handler();
            lvgl_runtime_destroy(runtime);
        }
        Check(flushes > 0, "Composite controls were not rendered");
        lv_display_delete(display); std::free(pixels); lv_deinit();
        std::puts("PASS: minimal advanced controls registry, native interactions, rendering and repeated destruction");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
