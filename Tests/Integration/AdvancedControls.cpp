#include <Serialization/AdvancedControls.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Project.h>
#include <Serialization/Xml.h>
#include <lvgl_runtime.h>
#include "../../CodeGeneration/CCodeGenerator.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace Serialization;
namespace {
void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void Flush(lv_display_t* display, const lv_area_t*, uint8_t*) { lv_display_flush_ready(display); }
void Click(lv_obj_t* object) {
    Check(object, "Missing clickable native part");
    lv_obj_send_event(object, LV_EVENT_CLICKED, nullptr);
}
json Section(lv_obj_t* object) {
    const auto* type = AdvancedControls::Type(object);
    Check(type, "Advanced control type was lost");
    return ObjectSerializer::SerializeObject(object).at(type);
}
template<class Action> void Reject(lv_obj_t* object, Action action) {
    const auto before = ObjectSerializer::SerializeObject(object);
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "Invalid control settings were accepted");
    Check(ObjectSerializer::SerializeObject(object) == before, "Rejected settings mutated the live control");
}
void Changed(lv_event_t* event) { ++*static_cast<unsigned*>(lv_event_get_user_data(event)); }
json Widget(int id, const char* type, json settings, int x, int y, int width, int height) {
    return {{"oid", id}, {"par", -1}, {"base", {{"coords", {{"x", x}, {"y", y}, {"width", width}, {"height", height}}}}},
            {type, std::move(settings)}};
}
json Fixture() {
    const auto variable=[](const char* name,int type,json value,int oid,const char* property) {
        return json{{"var",{{"name",name},{"dataType",type},{"default",std::move(value)},{"history",1},
            {"bindings",{{{"screen",0},{"oid",oid},{"property",property},{"direction","two-way"}}}}}}};
    };
    return {{"formatVersion", 2}, {"runtimeTarget", "9.6"}, {"canvas", {{"width", 640}, {"height", 400}}},
        {"variables", {variable("radioSelection",1,0,1,"/radioGroup/selected"),
            variable("checkboxValue",2,false,2,"/checkboxButton/checked"),
            variable("nestedValue",2,true,3,"/labeledCheckbox/checked"),
            variable("chosenDate",0,"2028-02-29",4,"/datePicker/date"),
            variable("menuSelection",1,1,5,"/menu/selected")}},
        {"obj", {
            Widget(1, "radioGroup", {{"title", "Location"}, {"horizontal", true}, {"selected", 0}, {"options", {
                {{"text", "New York"}, {"disabled", false}}, {{"text", "Paris"}, {"disabled", false}}, {{"text", "London"}, {"disabled", true}}}}}, 12, 12, 400, 100),
            Widget(2, "checkboxButton", {{"text", "4 Star"}, {"checked", false}}, 12, 130, 170, 48),
            Widget(3, "labeledCheckbox", {{"text", "2 Queen"}, {"description", "Breakfast included"}, {"checked", true}}, 200, 130, 210, 82),
            Widget(4, "datePicker", {{"date", "2028-02-29"}, {"format", "dd/mm/yyyy"}, {"minDate", "2028-01-01"}, {"maxDate", "2028-12-31"}}, 12, 240, 270, 48),
            Widget(5, "menu", {{"title", "Menu"}, {"selected", 1}, {"items", {
                {{"id", 1}, {"parent", 0}, {"text", "Books"}}, {{"id", 2}, {"parent", 0}, {"text", "Music"}},
                {{"id", 3}, {"parent", 2}, {"text", "Jazz"}}, {{"id", 4}, {"parent", 2}, {"text", "Unavailable"}, {"disabled", true}}}}}, 430, 12, 198, 350)
        }}};
}
void Bindings(lvgl_runtime_t* runtime,const std::vector<lv_obj_t*>& objects) {
    lvgl_runtime_value_t value{};
    value.type=LVGL_RUNTIME_INTEGER; value.integer_value=1;
    Check(lvgl_runtime_set_variable(runtime,"radioSelection",&value),"Application radio binding failed");
    Check(Section(objects[0]).at("selected")==1,"Application radio value did not reach its widget");
    value.integer_value=2;
    Check(!lvgl_runtime_set_variable(runtime,"radioSelection",&value),"Disabled radio variable value was accepted");
    Check(Section(objects[0]).at("selected")==1,"Rejected radio binding mutated the widget");
    Click(AdvancedControls::Part(objects[0],"Option0"));
    Check(lvgl_runtime_get_variable(runtime,"radioSelection",&value) && value.integer_value==0,"Radio click did not update its variable");
    for(size_t index=1;index<=2;++index) {
        const char* name=index==1 ? "checkboxValue" : "nestedValue";
        value={}; value.type=LVGL_RUNTIME_BOOLEAN; value.boolean_value=index==1;
        Check(lvgl_runtime_set_variable(runtime,name,&value),"Application checkbox binding failed");
        Check(Section(objects[index]).at("checked")==value.boolean_value,"Application checkbox value did not reach its indicator");
        Click(objects[index]);
        Check(lvgl_runtime_get_variable(runtime,name,&value) && value.boolean_value==(index==2),"Checkbox click did not update its variable");
    }
    value={}; value.type=LVGL_RUNTIME_STRING; value.string_value="2028-03-15";
    Check(lvgl_runtime_set_variable(runtime,"chosenDate",&value),"Application date binding failed");
    value.string_value="2029-01-01";
    Check(!lvgl_runtime_set_variable(runtime,"chosenDate",&value),"Out-of-range date variable was accepted");
    Check(Section(objects[3]).at("date")=="2028-03-15","Rejected date binding mutated the date display");
    Check(lvgl_runtime_get_variable(runtime,"chosenDate",&value) && std::string(value.string_value)=="2028-03-15","Rejected date binding changed variable storage");
    value.string_value="2028-02-29";
    Check(lvgl_runtime_set_variable(runtime,"chosenDate",&value),"Date binding reset failed");
    value={}; value.type=LVGL_RUNTIME_INTEGER; value.integer_value=3;
    Check(lvgl_runtime_set_variable(runtime,"menuSelection",&value),"Application menu binding failed");
    value.integer_value=2;
    Check(!lvgl_runtime_set_variable(runtime,"menuSelection",&value),"Non-leaf menu variable was accepted");
    value.integer_value=1;
    Check(lvgl_runtime_set_variable(runtime,"menuSelection",&value),"Menu binding reset failed");
}
void Exercise(lv_obj_t* radio, lv_obj_t* checkbox, lv_obj_t* label, lv_obj_t* date, lv_obj_t* menu) {
    unsigned changes = 0;
    lv_obj_add_event_cb(radio, Changed, LV_EVENT_VALUE_CHANGED, &changes);
    Click(AdvancedControls::Part(radio, "Option1"));
    Check(Section(radio).at("selected") == 1, "Radio click did not select Paris");
    Click(AdvancedControls::Part(radio, "Option2"));
    Check(Section(radio).at("selected") == 1 && changes == 1, "Disabled radio changed the selection or emitted an extra event");
    auto radioSettings = Section(radio);
    radioSettings["horizontal"] = false;
    radioSettings["options"][1]["text"] = "Updated Paris";
    AdvancedControls::Apply(radio, radioSettings);
    Check(Section(radio).at("options")[1].at("text") == "Updated Paris", "Radio properties were not editable");
    lv_obj_remove_event_cb_with_user_data(radio, Changed, &changes);

    for (auto* object : {checkbox, label}) {
        unsigned checkboxChanges=0;
        lv_obj_add_event_cb(object,Changed,LV_EVENT_VALUE_CHANGED,&checkboxChanges);
        const auto initial=Section(object).at("checked").get<bool>();
        Click(object);
        Check(Section(object).at("checked")==!initial && checkboxChanges==1,"Clicking the checkbox label/card did not toggle exactly once");
        auto* indicator=AdvancedControls::Part(object,"Checkbox");
        lv_obj_send_event(indicator,LV_EVENT_PRESSED,nullptr);
        lv_obj_send_event(indicator,LV_EVENT_RELEASED,nullptr);
        Click(indicator);
        Check(Section(object).at("checked")==initial && checkboxChanges==2,"Clicking the native checkbox indicator did not toggle exactly once");
        lv_obj_remove_event_cb_with_user_data(object,Changed,&checkboxChanges);
        auto settings = Section(object);
        const bool wasChecked = settings.at("checked").get<bool>();
        settings["checked"] = !wasChecked;
        AdvancedControls::Apply(object, settings);
        Check(Section(object).at("checked") == !wasChecked, "Checkbox state was not applied");
        settings["disabled"] = true;
        AdvancedControls::Apply(object, settings);
        Check(lv_obj_has_state(object, LV_STATE_DISABLED), "Disabled checkbox presentation is missing");
    }

    auto* navigationGroup=lv_group_create();
    lv_group_add_obj(navigationGroup,AdvancedControls::Part(date,"Display"));
    lv_group_add_obj(navigationGroup,AdvancedControls::Part(menu,"Item1"));
    lv_group_add_obj(navigationGroup,AdvancedControls::Part(menu,"Item2"));
    Reject(date, [&] { AdvancedControls::Apply(date, {{"date", "2027-02-29"}}); });
    Reject(date, [&] { AdvancedControls::Apply(date, {{"minDate", "2029-01-01"}, {"maxDate", "2028-12-31"}}); });
    Reject(date, [&] { AdvancedControls::Apply(date, {{"date", "2029-01-01"}}); });
    Click(AdvancedControls::Part(date, "Display"));
    Check(AdvancedControls::Part(date, "Calendar"), "Date click did not open its native calendar");
    Check(!lv_obj_is_hidden(AdvancedControls::Part(date, "Popup")), "Date calendar popup is hidden");
    Check(lv_group_get_focused(navigationGroup)==lv_calendar_get_btnmatrix(AdvancedControls::Part(date,"Calendar")),"Calendar keyboard focus did not move into the date grid");
    const uint32_t escape = LV_KEY_ESC;
    lv_obj_send_event(date, LV_EVENT_KEY, const_cast<uint32_t*>(&escape));
    Check(lv_obj_is_hidden(AdvancedControls::Part(date,"Popup")),"Escape did not dismiss the calendar");
    Check(lv_group_get_focused(navigationGroup)==AdvancedControls::Part(date,"Display"),"Closing the calendar did not restore focus to its input");
    for(int firstDay=0;firstDay<7;++firstDay) {
        AdvancedControls::Apply(date,{{"firstDay",firstDay},{"date","2028-02-29"},{"minDate","2028-02-05"},{"maxDate","2028-03-20"}});
        Click(AdvancedControls::Part(date,"Display"));
        auto* calendar=AdvancedControls::Part(date,"Calendar");
        auto* matrix=lv_calendar_get_btnmatrix(calendar);
        // 1 February 2028 is Tuesday. Every per-control week start must
        // display the actual date in its correct column, independent of
        // LVGL's compile-time default week start.
        const unsigned offset=(2u+7u-static_cast<unsigned>(firstDay))%7u;
        Check(std::string(lv_buttonmatrix_get_button_text(matrix,7+offset))=="1","Calendar first weekday shifted the date labels incorrectly");
        Check(lv_buttonmatrix_has_button_ctrl(matrix,7+offset,LV_BUTTONMATRIX_CTRL_DISABLED),"Calendar did not disable a date before minDate");
        const unsigned leapDay=7+offset+28;
        Check(std::string(lv_buttonmatrix_get_button_text(matrix,leapDay))=="29","Leap day is missing");
        lv_buttonmatrix_set_selected_button(matrix,leapDay);
        lv_obj_send_event(calendar,LV_EVENT_VALUE_CHANGED,nullptr);
        Check(Section(date).at("date")=="2028-02-29" && lv_obj_is_hidden(AdvancedControls::Part(date,"Popup")),"Calendar date selection did not update and dismiss");
        Click(AdvancedControls::Part(date,"Display"));
        lv_calendar_set_month_shown(calendar,2028,3);
        // Native header navigation must refresh both our map and date limits.
        const unsigned marchOffset=(3u+7u-static_cast<unsigned>(firstDay))%7u;
        Check(std::string(lv_buttonmatrix_get_button_text(matrix,7+marchOffset))=="1","Month navigation did not refresh the calendar grid");
        Check(lv_buttonmatrix_has_button_ctrl(matrix,7+marchOffset+20,LV_BUTTONMATRIX_CTRL_DISABLED),"Calendar did not disable a date after maxDate");
        Click(AdvancedControls::Part(date,"Popup"));
        Check(lv_obj_is_hidden(AdvancedControls::Part(date,"Popup")),"Clicking outside did not dismiss the calendar");
    }

    Click(AdvancedControls::Part(menu, "Item2"));
    Check(!lv_obj_is_hidden(AdvancedControls::Part(menu, "Item3")), "Menu did not open its nested page");
    Click(AdvancedControls::Part(menu, "Item4"));
    Check(Section(menu).at("selected") == 1, "Disabled menu item changed the selection");
    Click(AdvancedControls::Part(menu, "Item3"));
    Check(Section(menu).at("selected") == 3, "Menu leaf click did not select Jazz");
    Click(AdvancedControls::Part(menu, "Back"));
    Check(!lv_obj_is_hidden(AdvancedControls::Part(menu, "Item1")), "Menu back navigation failed");
    uint32_t key=LV_KEY_RIGHT;
    lv_obj_send_event(AdvancedControls::Part(menu,"Item2"),LV_EVENT_KEY,&key);
    Check(lv_group_get_focused(navigationGroup)==AdvancedControls::Part(menu,"Item3"),"Right arrow did not enter the submenu and skip its disabled item");
    key=LV_KEY_LEFT;
    lv_obj_send_event(AdvancedControls::Part(menu,"Item3"),LV_EVENT_KEY,&key);
    Check(lv_group_get_focused(navigationGroup)==AdvancedControls::Part(menu,"Item2"),"Left arrow did not return to the parent menu item");
    key=LV_KEY_UP;
    lv_obj_send_event(AdvancedControls::Part(menu,"Item2"),LV_EVENT_KEY,&key);
    Check(lv_group_get_focused(navigationGroup)==AdvancedControls::Part(menu,"Item1"),"Up arrow did not focus the previous menu item");
    lv_obj_send_event(AdvancedControls::Part(menu,"Item1"),LV_EVENT_KEY,&key);
    Check(lv_group_get_focused(navigationGroup)==AdvancedControls::Part(menu,"Item2"),"Menu keyboard navigation did not wrap");
    Reject(menu, [&] { AdvancedControls::Apply(menu, {{"items", {{{"id", 1}, {"parent", 2}}, {{"id", 2}, {"parent", 1}}}}}); });
    lv_group_delete(navigationGroup);
}
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Pass the generated fixture directory");
        lv_init();
        auto* display = lv_display_create(640, 400);
        std::vector<uint8_t> pixels(640 * 16 * 4);
        lv_display_set_buffers(display, pixels.data(), nullptr, static_cast<uint32_t>(pixels.size()), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, Flush);
        const auto document = Fixture();
        std::vector<lv_obj_t*> objects;
        const auto supported = ObjectSerializer::SupportedTypes();
        for (const auto& record : document.at("obj")) {
            auto* object = ObjectSerializer::DeserializeObject(record);
            Check(object, "Advanced control restoration failed");
            objects.push_back(object);
            const std::string type = AdvancedControls::Type(object);
            Check(std::find(supported.begin(), supported.end(), type) != supported.end(), "Registry omits " + type);
            const auto canonical = ObjectSerializer::SerializeObject(object);
            auto section = canonical.at(type);
            section["futureExtension"] = {{"retained", true}};
            AdvancedControls::Apply(object, section);
            Check(Section(object).at("futureExtension").at("retained") == true, "Unknown properties were lost");
            for (bool protobuf : {false, true}) {
                auto* copy = protobuf ? ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(object)) : ObjectSerializer::DeserializeObject(ObjectSerializer::SerializeObject(object));
                Check(copy && Section(copy) == Section(object), "Control JSON/Protobuf round trip changed settings");
                lv_obj_delete(copy);
            }
            // Generated decoration belongs to the composite, never extra saved objects.
            const auto saved = Project::ToJSON({object}, lv_screen_active(), json::array(), 640, 400);
            Check(saved.at("obj").size() == 1, "Composite leaked native internal parts into the project");
        }
        Exercise(objects[0], objects[1], objects[2], objects[3], objects[4]);
        for (auto* object : objects) lv_obj_delete(object);
        const auto binary = Protobuf::Encode(EntityKind::Project, "project", document);
        const auto xml = Xml::Encode(EntityKind::Project, "project", document);
        Check(Xml::Decode(xml).data == document, "XML typed widget settings did not round trip");
        for (const auto& input : {binary, Binary(xml.begin(), xml.end())}) {
            for (bool borrowed : {false, true}) {
                lvgl_runtime_t* raw = nullptr;
                char error[512]{};
                const bool loaded = borrowed ? lvgl_runtime_load_memory_borrowed_assets(input.data(), input.size(), nullptr, &raw, error, sizeof error) :
                    lvgl_runtime_load_memory(input.data(), input.size(), nullptr, &raw, error, sizeof error);
                Check(loaded, error);
                std::unique_ptr<lvgl_runtime_t, decltype(&lvgl_runtime_destroy)> runtime(raw, lvgl_runtime_destroy);
                std::vector<lv_obj_t*> native;
                for (int id = 1; id <= 5; ++id) {
                    auto* object = lvgl_runtime_object(raw, 0, id);
                    Check(object && AdvancedControls::Type(object), "Public runtime lost an advanced control");
                    native.push_back(object);
                }
                Bindings(raw,native);
                Exercise(native[0], native[1], native[2], native[3], native[4]);
                lv_tick_inc(20); lv_timer_handler();
            }
        }
        const auto output = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(output);
        std::ofstream(output / "fixture.json") << document.dump(2);
        std::ofstream(output / "fixture.xml") << xml;
        std::ofstream pb(output / "fixture.pb", std::ios::binary);
        pb.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        CCodeGenerator generator;
        Check(generator.LoadProject(document), generator.GetLastError());
        Check(generator.ExportProject(output.u8string()), generator.GetLastError());
        lv_display_delete(display); lv_deinit();
        std::cout << "PASS: five native controls, interaction/validation, JSON/Protobuf/XML, owned/borrowed runtime and C export\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
