#include "catch.hpp"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/BasicWidgets.h>
#include <Serialization/CustomWidgets.h>
#include <Serialization/CodeWidgets.h>
#include <Serialization/ImageAsset.h>
#include <Serialization/Style.h>
#include <Serialization/Area.h>
#include <set>

namespace
{
    json Semantic(json value)
    {
        if (value.is_object()) { value.erase("nativeStyles"); for (auto& field : value.items()) field.value() = Semantic(field.value()); }
        else if (value.is_array()) for (auto& item : value) item = Semantic(item);
        return value;
    }
    void Populate(const std::string& type, lv_obj_t* object)
    {
        using namespace Serialization;
        lv_obj_set_size(object, 200, 120); lv_obj_set_pos(object, 7, 11);
        lv_obj_set_style_border_color(object, lv_color_hex(0x3172a4), LV_PART_MAIN);
        lv_obj_set_style_border_width(object, 3, LV_PART_MAIN);
        if (type == "arc") { lv_arc_set_range(object, -20, 120); lv_arc_set_value(object, 42); }
        if (type == "bar") { lv_bar_set_range(object, -10, 110); lv_bar_set_value(object, 37, LV_ANIM_OFF); }
        if (type == "button") LVButton::SetText(object, "Save");
        if (type == "checkbox") { lv_checkbox_set_text(object, "Checked item"); lv_obj_add_state(object, LV_STATE_CHECKED); }
        if (type == "label") lv_label_set_text(object, "Round-trip text");
        if (type == "image") lv_image_set_src(object, LV_SYMBOL_IMAGE);
        if (type == "canvas") LVCanvas::Apply(object, {{"width", 8}, {"height", 6}, {"format", LV_COLOR_FORMAT_ARGB8888}, {"fillColor", 0xff336699u}});
        if (type == "calendar") { lv_calendar_set_today_date(object, 2026, 11, 18); lv_calendar_set_month_shown(object, 2027, 2); }
        if (type == "dropdown") { lv_dropdown_set_options(object, "First\nSecond\nThird"); lv_dropdown_set_selected(object, 2); }
        if (type == "roller") { lv_roller_set_options(object, "One\nTwo\nThree", LV_ROLLER_MODE_NORMAL); lv_roller_set_selected(object, 1, LV_ANIM_OFF); }
        if (type == "slider") { lv_slider_set_range(object, -20, 200); lv_slider_set_value(object, 75, LV_ANIM_OFF); }
        if (type == "spinbox") { lv_spinbox_set_range(object, -1000, 1000); lv_spinbox_set_value(object, 123); }
        if (type == "switch") lv_obj_add_state(object, LV_STATE_CHECKED);
        if (type == "textarea") { lv_textarea_set_text(object, "Editor text"); lv_textarea_set_placeholder_text(object, "Type here"); }
        if (type == "led") { lv_led_set_brightness(object, 127); lv_led_set_color(object, lv_color_hex(0x9955ee)); }
        if (type == "line") BasicWidgets::Apply(object, {{"points", {{0, 0}, {30, 20}, {60, 10}}}, {"invertY", true}});
        if (type == "chart") ExtendedWidgets::Apply(object, {{"type", LV_CHART_TYPE_LINE}, {"pointCount", 4},
            {"series", {{{"color", 0xff4466aau}, {"axis", 0}, {"y", {10, 40, 20, 80}}}, {{"color", 0xffaa6644u}, {"axis", 1}, {"y", {80, 20, 40, 10}}}}}});
        if (type == "table") { lv_table_set_cell_value(object, 1, 1, "Cell"); lv_table_set_column_width(object, 0, 90); }
        if (type == "scale") ExtendedWidgets::Apply(object, {{"variant", "gauge"}, {"min", 0}, {"max", 100}, {"mode", LV_SCALE_MODE_ROUND_INNER}, {"value", 64}});
        if (type == "tabview") { lv_tabview_add_tab(object, "First"); lv_tabview_add_tab(object, "Second"); lv_tabview_set_active(object, 1, LV_ANIM_OFF); }
        if (type == "tileview") { lv_tileview_add_tile(object, 0, 0, LV_DIR_RIGHT); auto* tile = lv_tileview_add_tile(object, 1, 0, LV_DIR_LEFT); lv_tileview_set_tile(object, tile, LV_ANIM_OFF); }
        if (type == "window") ExtendedWidgets::Apply(object, {{"title", "Settings"}, {"headerHeight", 32}});
        if (type == "messagebox") { lv_msgbox_add_title(object, "Question"); lv_msgbox_add_text(object, "Continue?"); lv_msgbox_add_close_button(object); lv_msgbox_add_footer_button(object, "OK"); }
        if (type == "page") ExtendedWidgets::Apply(object, {{"scrollDirection", LV_DIR_VER}, {"scrollbarMode", LV_SCROLLBAR_MODE_AUTO}});
        if (type == "textBox") CustomWidgets::Apply(object, {{"title", "Level"}, {"value", 65}, {"step", 5}});
        if (type == "layerView") CustomWidgets::Apply(object, {{"title", "Hierarchy"}, {"selected", 2}, {"indent", 20}});
        if (type == "qrcode") CodeWidgets::Apply(object, {{"size", 200}});
        lv_obj_update_layout(object);
    }
}
TEST_CASE("Every registered object serializes and deserializes in JSON and Protobuf")
{
    using namespace Serialization;
    const auto types = ObjectSerializer::SupportedTypes();
    size_t expected = 31;
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6
#if LV_USE_BARCODE
    ++expected;
#endif
#if LV_USE_QRCODE
    ++expected;
#endif
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    ++expected;
#endif
#endif
    CHECK(types.size() == expected);
    CHECK(std::set<std::string>(types.begin(), types.end()).size() == types.size());
    for (const auto& type : types)
    {
        DYNAMIC_SECTION(type)
        {
            auto* object = ObjectSerializer::Create(type);
            REQUIRE(object != nullptr); REQUIRE(ObjectSerializer::TypeName(object) == type);
            Populate(type, object);
            const auto original = ObjectSerializer::SerializeObject(object);
            for (bool binary : {false, true})
            {
                CAPTURE(binary);
                auto* restored = binary ? ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(object)) : ObjectSerializer::DeserializeObject(original);
                REQUIRE(restored != nullptr);
                CHECK(ObjectSerializer::TypeName(restored) == type);
                const auto result = ObjectSerializer::SerializeObject(restored);
                INFO(json::diff(Semantic(original), Semantic(result)).dump(2));
                CHECK(Semantic(result) == Semantic(original));
                CHECK(lv_color_eq(lv_obj_get_style_border_color(restored, LV_PART_MAIN), lv_color_hex(0x3172a4)));
                CHECK(lv_obj_get_style_border_width(restored, LV_PART_MAIN) == 3);
                lv_obj_delete(restored);
            }
            lv_obj_delete(object);
        }
    }
}
TEST_CASE("Geometry styles and fonts have complete JSON and Protobuf round-trips")
{
    using namespace Serialization;
    const auto area = Area::ToJSON(-12, 7, 320, 240);
    const auto geometry = Area::FromJSON(Protobuf::Decode(Protobuf::Encode(EntityKind::Asset, "area", area)).data);
    CHECK(Area::ToJSON(geometry.x, geometry.y, geometry.width, geometry.height) == area);
    lv_style_t style; lv_style_init(&style); lv_style_set_bg_color(&style, lv_color_hex(0x224466)); lv_style_set_radius(&style, 9);
    const auto styleRecord = Style::ToJSON(style);
    const auto& styleCopy = Style::FromJSON(Protobuf::Decode(Protobuf::Encode(EntityKind::Style, "style", styleRecord)).data);
    CHECK(Style::ToJSON(const_cast<lv_style_t&>(styleCopy)) == styleRecord);
    const auto fontRecord = LVFont::ToJSON(*LV_FONT_DEFAULT);
    const auto& fontCopy = LVFont::FromJSON(Protobuf::Decode(Protobuf::Encode(EntityKind::Font, "font", fontRecord)).data);
    CHECK(LVFont::ToJSON(fontCopy) == fontRecord);
    lv_style_reset(&style);
}


TEST_CASE("Default empty object records round-trip without inventing widget data")
{
    using namespace Serialization;
    for (const auto& type : ObjectSerializer::SupportedTypes())
    {
        DYNAMIC_SECTION(type)
        {
            auto* object = ObjectSerializer::Create(type);
            REQUIRE(object != nullptr);
            lv_obj_update_layout(object);
            const auto record = ObjectSerializer::SerializeObject(object);
            for (bool binary : {false, true})
            {
                CAPTURE(binary);
                auto* restored = binary ? ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(object)) : ObjectSerializer::DeserializeObject(record);
                REQUIRE(restored != nullptr);
                const auto result = ObjectSerializer::SerializeObject(restored);
                INFO(json::diff(Semantic(record), Semantic(result)).dump(2));
                CHECK(Semantic(result) == Semantic(record));
                lv_obj_delete(restored);
            }
            lv_obj_delete(object);
        }
    }
}
