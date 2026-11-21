#include "catch.hpp"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Page.h>
#include <Serialization/BasicWidgets.h>
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/CustomWidgets.h>

namespace
{
    json ContentWithoutLocalStyleStorage(json value)
    {
        if (value.is_object())
        {
            value.erase("nativeStyles");
            for (auto& entry : value.items()) entry.value() = ContentWithoutLocalStyleStorage(entry.value());
        }
        else if (value.is_array())
            for (auto& entry : value) entry = ContentWithoutLocalStyleStorage(entry);
        return value;
    }
}

TEST_CASE("Project hierarchy restores parents after children")
{
    const auto document = json::parse(R"({"obj":[
        {"oid":2,"par":1,"base":{"name":"Child","coords":{"x":3,"y":4,"width":50,"height":20}},
         "cb":{"text":"Enabled","checked":true,"inact":false}},
        {"oid":1,"par":-1,"base":{"name":"Parent","coords":{"x":20,"y":30,"width":120,"height":90}}}
    ]})");
    auto objects = Serialization::Page::FromJSON(document);
    REQUIRE(objects.size() == 2);
    CHECK(lv_obj_get_parent(objects[0]) == objects[1]);
    CHECK(lv_obj_is_checked(objects[0]));
    CHECK(std::string(lv_checkbox_get_text(objects[0])) == "Enabled");
    const auto saved = Serialization::Page::ToJSON(objects);
    CHECK(saved["obj"][0]["par"] == 1);
    CHECK(saved["obj"][1]["par"] == -1);
    lv_obj_delete(objects[1]);
}

TEST_CASE("A failed project import cleans up partially restored widgets")
{
    auto* screen = lv_screen_active();
    const auto original = lv_obj_get_child_count(screen);
    const auto document = json::parse(R"({"obj":[
        {"oid":0,"par":-1,"base":{"coords":{"width":100,"height":40}},"text":"Created first"},
        {"oid":1,"par":-1,"base":{"coords":{"width":100,"height":40}},"slider":{"min":"invalid"}}
    ]})");
    CHECK_THROWS_AS(Serialization::Page::FromJSON(document), json::exception);
    CHECK(lv_obj_get_child_count(screen) == original);
}

TEST_CASE("Symbol image restores its source and offsets")
{
    auto record = json::parse(R"({
        "base":{"coords":{"x":3,"y":5,"width":35,"height":25}},
        "img":{"srcType":2,"sym":"OK","offX":2,"offY":4}
    })");
    record["img"]["sym"] = LV_SYMBOL_OK;
    auto* image = Serialization::LVImage::FromJSON(record);
    REQUIRE(image != nullptr);
    const auto saved = Serialization::LVImage::ToJSON(image);
    CHECK(saved["img"]["sym"] == LV_SYMBOL_OK);
    CHECK(saved["img"]["offX"] == 2);
    CHECK(saved["img"]["offY"] == 4);
    lv_obj_delete(image);
}

TEST_CASE("Remaining basic widget settings survive repeated saves")
{
    const auto samples = json::parse(R"({
        "dropdown":{"options":"Alpha\nBeta\nGamma","selected":2,"direction":1,"text":"Choose","symbol":"v","highlight":false},
        "roller":{"options":"Alpha\nBeta\nGamma","selected":1,"mode":1,"visibleRows":4},
        "slider":{"min":-20,"max":80,"value":55,"leftValue":-10,"mode":2,"orientation":1},
        "spinbox":{"min":-10000,"max":20000,"value":-1234,"step":10,"digits":5,"decimalPlaces":2,"rollover":true,"stepDirection":1},
        "switch":{"checked":true},
        "textarea":{"text":"secret","placeholder":"Enter text","accepted":null,"oneLine":true,"password":true,"passwordBullet":"*","passwordShowTime":600,"maxLength":20,"cursor":2,"cursorClick":false,"selection":true},
        "led":{"brightness":123,"color":4281558681},
        "line":{"points":[[0,0],[20,40],[60,10]],"invertY":true},
        "spinner":{"duration":1300,"sweep":110}
    })");
    for (const auto& sample : samples.items())
    {
        CAPTURE(sample.key());
        const json document{{"base", {{"coords", {{"x", 5}, {"y", 9}, {"width", 150}, {"height", 90}}}}}, {sample.key(), sample.value()}};
        auto* object = Serialization::ObjectSerializer::DeserializeObject(document);
        REQUIRE(object != nullptr);
        const auto first = Serialization::ObjectSerializer::SerializeObject(object);
        for (const auto& property : sample.value().items())
        {
            CAPTURE(property.key());
            CHECK(first[sample.key()][property.key()] == property.value());
        }
        auto* restored = Serialization::ObjectSerializer::DeserializeObject(first);
        REQUIRE(restored != nullptr);
        const auto second = Serialization::ObjectSerializer::SerializeObject(restored);
        CHECK(second[sample.key()] == first[sample.key()]);
        lv_obj_delete(object);
        lv_obj_delete(restored);
    }
}

TEST_CASE("Native part styles and checked state survive saves")
{
    auto* object = lv_switch_create(lv_screen_active());
    lv_obj_set_state(object, LV_STATE_CHECKED, true);
    lv_obj_set_style_radius(object, 7, LV_PART_KNOB);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x123456), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_scrollable(object, false);
    auto* restored = Serialization::ObjectSerializer::DeserializeObject(Serialization::ObjectSerializer::SerializeObject(object));
    REQUIRE(restored != nullptr);
    CHECK(lv_obj_has_state(restored, LV_STATE_CHECKED));
    CHECK(lv_obj_get_style_radius(restored, LV_PART_KNOB) == 7);
    CHECK(lv_color_to_u32(lv_obj_get_style_bg_color(restored, LV_PART_INDICATOR)) == 0xff123456);
    CHECK_FALSE(lv_obj_is_scrollable(restored));
    lv_obj_delete(object);
    lv_obj_delete(restored);
}

TEST_CASE("Grid tracks gradients and style image sources retain owned storage")
{
    auto* object = lv_obj_create(lv_screen_active());
    const int32_t columns[] = {60, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    const int32_t rows[] = {40, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(object, columns, rows);
    const auto selector = LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_USER_1;
    lv_obj_set_style_bg_image_src(object, LV_SYMBOL_OK, selector);
    lv_obj_set_style_text_font(object, LV_FONT_DEFAULT, selector);
    lv_obj_set_state(object, static_cast<lv_state_t>(LV_STATE_CHECKED | LV_STATE_USER_1), true);
    lv_grad_dsc_t gradient{};
    gradient.dir = LV_GRAD_DIR_VER;
    gradient.stops_count = 2;
    gradient.stops[0] = {lv_color_hex(0x123456), 255, 0};
    gradient.stops[1] = {lv_color_hex(0xabcdef), 128, 255};
    lv_obj_set_style_bg_grad(object, &gradient, LV_PART_MAIN);
    const auto record = Serialization::ObjectSerializer::SerializeObject(object);
    auto* restored = Serialization::ObjectSerializer::DeserializeObject(record);
    REQUIRE(restored != nullptr);
    const auto* savedColumns = lv_obj_get_style_grid_column_dsc_array(restored, LV_PART_MAIN);
    REQUIRE(savedColumns != nullptr);
    CHECK(savedColumns[0] == 60);
    CHECK(savedColumns[1] == LV_GRID_FR(1));
    CHECK(savedColumns[2] == LV_GRID_TEMPLATE_LAST);
    const auto* savedGradient = lv_obj_get_style_bg_grad(restored, LV_PART_MAIN);
    REQUIRE(savedGradient != nullptr);
    CHECK(savedGradient->stops_count == 2);
    CHECK(lv_color_to_u32(savedGradient->stops[1].color) == 0xffabcdef);
    CHECK(savedGradient->stops[1].opa == 128);
    CHECK(std::string(static_cast<const char*>(lv_obj_get_style_bg_image_src(restored, LV_PART_MAIN))) == LV_SYMBOL_OK);
    CHECK(lv_obj_get_style_text_font(restored, LV_PART_MAIN) == LV_FONT_DEFAULT);
    lv_obj_delete(object); lv_obj_delete(restored);
}

TEST_CASE("Chart series axes and table cells survive saves")
{
    auto* chart = lv_chart_create(lv_screen_active());
    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(chart, 3);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_SECONDARY_Y, -50, 150);
    auto* series = lv_chart_add_series(chart, lv_color_hex(0x336699), LV_CHART_AXIS_SECONDARY_Y);
    const int32_t xs[] = {5, 10, 20}, ys[] = {-10, 20, 100};
    lv_chart_set_series_values2(chart, series, xs, ys, 3);
    lv_chart_hide_series(chart, series, true);
    auto record = Serialization::ObjectSerializer::SerializeObject(chart);
    auto* chartCopy = Serialization::ObjectSerializer::DeserializeObject(record);
    REQUIRE(chartCopy != nullptr);
    const auto chartSaved = Serialization::ObjectSerializer::SerializeObject(chartCopy);
    CHECK(chartSaved["chart"] == record["chart"]);
    lv_obj_delete(chart); lv_obj_delete(chartCopy);

    auto* table = lv_table_create(lv_screen_active());
    lv_table_set_cell_value(table, 0, 0, "Name");
    lv_table_set_cell_value(table, 1, 1, "Value");
    lv_table_set_column_width(table, 1, 120);
    lv_table_set_cell_ctrl(table, 1, 1, LV_TABLE_CELL_CTRL_TEXT_CROP);
    lv_table_set_selected_cell(table, 1, 1);
    record = Serialization::ObjectSerializer::SerializeObject(table);
    auto* tableCopy = Serialization::ObjectSerializer::DeserializeObject(record);
    REQUIRE(tableCopy != nullptr);
    const auto tableSaved = Serialization::ObjectSerializer::SerializeObject(tableCopy);
    CHECK(tableSaved["table"] == record["table"]);
    lv_obj_delete(table); lv_obj_delete(tableCopy);
}

TEST_CASE("Tabs tiles and window contents survive saves")
{
    for (const auto* type : {"tabview", "tileview", "window", "messagebox", "scale", "keyboard", "imagebutton"})
    {
        CAPTURE(type);
        auto* object = Serialization::ExtendedWidgets::Create(type);
        REQUIRE(object != nullptr);
        lv_obj_set_size(object, 300, 200);
        lv_obj_t* content = nullptr;
        uint8_t imagePixels[16] = {};
        lv_image_dsc_t image{};
        if (std::string(type) == "tabview")
        {
            lv_tabview_add_tab(object, "First");
            content = lv_tabview_add_tab(object, "Second");
            lv_tabview_set_active(object, 1, LV_ANIM_OFF);
        }
        else if (std::string(type) == "tileview")
        {
            lv_tileview_add_tile(object, 0, 0, LV_DIR_RIGHT);
            content = lv_tileview_add_tile(object, 1, 0, LV_DIR_LEFT);
            lv_tileview_set_tile(object, content, LV_ANIM_OFF);
        }
        else if (std::string(type) == "window") content = Serialization::ExtendedWidgets::WindowContent(object);
        else if (std::string(type) == "messagebox")
        {
            lv_msgbox_add_title(object, "Message");
            lv_msgbox_add_footer_button(object, "Close");
            content = lv_msgbox_get_content(object);
        }
        else if (std::string(type) == "imagebutton")
        {
            image.header.magic = LV_IMAGE_HEADER_MAGIC;
            image.header.cf = LV_COLOR_FORMAT_ARGB8888;
            image.header.w = 2; image.header.h = 2; image.header.stride = 8;
            image.data_size = sizeof(imagePixels); image.data = imagePixels;
            lv_imagebutton_set_src(object, LV_IMAGEBUTTON_STATE_RELEASED, nullptr, &image, nullptr);
        }
        if (content)
        {
            auto* label = lv_label_create(content);
            lv_label_set_text(label, "Persistent text");
        }
        lv_obj_update_layout(object);
        const auto record = Serialization::ObjectSerializer::SerializeObject(object);
        auto* restored = Serialization::ObjectSerializer::DeserializeObject(record);
        REQUIRE(restored != nullptr);
        const auto saved = Serialization::ObjectSerializer::SerializeObject(restored);
        const auto originalContent = ContentWithoutLocalStyleStorage(record[type]);
        const auto restoredContent = ContentWithoutLocalStyleStorage(saved[type]);
        INFO(json::diff(originalContent, restoredContent).dump(2));
        CHECK(restoredContent == originalContent);
        lv_obj_delete(object); lv_obj_delete(restored);
    }
}


TEST_CASE("Composite document children preserve their content hosts without duplication")
{
    for (const auto* type : {"tabview", "tileview", "window", "messagebox", "page"})
    {
        CAPTURE(type);
        auto* parent = Serialization::ExtendedWidgets::Create(type);
        if (std::string(type) == "tabview") { lv_tabview_add_tab(parent, "One"); lv_tabview_add_tab(parent, "Two"); lv_tabview_set_active(parent, 1, LV_ANIM_OFF); }
        if (std::string(type) == "tileview") { lv_tileview_add_tile(parent, 0, 0, LV_DIR_RIGHT); auto* tile = lv_tileview_add_tile(parent, 1, 0, LV_DIR_LEFT); lv_tileview_set_tile(parent, tile, LV_ANIM_OFF); }
        if (std::string(type) == "messagebox") lv_msgbox_add_title(parent, "Title");
        auto* host = Serialization::ExtendedWidgets::ContentParent(parent);
        auto* label = lv_label_create(host);
        lv_label_set_text(label, "Document child");
        auto* embedded = lv_label_create(host);
        lv_label_set_text(embedded, "Owned child");
        const auto saved = Serialization::Page::ToJSON({parent, label});
        auto restored = Serialization::Page::FromJSON(saved);
        REQUIRE(restored.size() == 2);
        auto* restoredHost = Serialization::ExtendedWidgets::ContentParent(restored[0]);
        CHECK(lv_obj_get_parent(restored[1]) == restoredHost);
        CHECK(lv_obj_get_child_count(restoredHost) == 2);
        CHECK(std::string(lv_label_get_text(restored[1])) == "Document child");
        const auto again = Serialization::Page::ToJSON(restored);
        INFO(json::diff(ContentWithoutLocalStyleStorage(saved["obj"][0]), ContentWithoutLocalStyleStorage(again["obj"][0])).dump(2));
        CHECK(ContentWithoutLocalStyleStorage(again["obj"][0]) == ContentWithoutLocalStyleStorage(saved["obj"][0]));
        lv_obj_delete(parent); lv_obj_delete(restored[0]);
    }
}

TEST_CASE("Chart cursors and message box header buttons survive repeated application")
{
    auto* chart = lv_chart_create(lv_screen_active());
    auto* series = lv_chart_add_series(chart, lv_color_hex(0x3377aa), LV_CHART_AXIS_PRIMARY_Y);
    auto* attached = lv_chart_add_cursor(chart, lv_color_hex(0xff0000), LV_DIR_HOR);
    lv_chart_set_cursor_point(chart, attached, series, 2);
    auto* positioned = lv_chart_add_cursor(chart, lv_color_hex(0x00ff00), LV_DIR_VER);
    lv_point_t position{15, 25}; lv_chart_set_cursor_pos(chart, positioned, &position);
    const auto record = Serialization::ObjectSerializer::SerializeObject(chart);
    for (int i = 0; i < 3; ++i)
    {
        Serialization::ExtendedWidgets::Apply(chart, record.at("chart"));
        CHECK(Serialization::ObjectSerializer::SerializeObject(chart)["chart"] == record["chart"]);
    }
    lv_obj_delete(chart);
    auto* box = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(box, "Settings"); lv_msgbox_add_close_button(box);
    lv_msgbox_add_header_button(box, LV_SYMBOL_SETTINGS);
    lv_msgbox_add_footer_button(box, "Apply");
    const auto boxRecord = Serialization::ObjectSerializer::SerializeObject(box);
    auto* copy = Serialization::ObjectSerializer::DeserializeObject(boxRecord);
    REQUIRE(copy != nullptr);
    for (int i = 0; i < 3; ++i)
    {
        Serialization::ExtendedWidgets::Apply(copy, boxRecord.at("messagebox"));
        const auto result = Serialization::ObjectSerializer::SerializeObject(copy);
        INFO(json::diff(ContentWithoutLocalStyleStorage(boxRecord["messagebox"]), ContentWithoutLocalStyleStorage(result["messagebox"])).dump(2));
        CHECK(ContentWithoutLocalStyleStorage(result["messagebox"]) == ContentWithoutLocalStyleStorage(boxRecord["messagebox"]));
    }
    CHECK(lv_obj_get_child_count(lv_msgbox_get_header(copy)) == 3);
    CHECK(lv_obj_get_class(lv_obj_get_child(lv_msgbox_get_header(copy), 1)) == &lv_msgbox_header_button_class);
    lv_obj_delete(box); lv_obj_delete(copy);
}


TEST_CASE("Button matrix edits retain individual flags and update global recoloring")
{
    auto* matrix = lv_buttonmatrix_create(lv_screen_active());
    lv_buttonmatrix_set_button_ctrl(matrix, 1, LV_BUTTONMATRIX_CTRL_RECOLOR);
    auto record = Serialization::ObjectSerializer::SerializeObject(matrix);
    auto* copy = Serialization::ObjectSerializer::DeserializeObject(record);
    REQUIRE(copy != nullptr);
    CHECK_FALSE(lv_buttonmatrix_has_button_ctrl(copy, 0, LV_BUTTONMATRIX_CTRL_RECOLOR));
    CHECK(lv_buttonmatrix_has_button_ctrl(copy, 1, LV_BUTTONMATRIX_CTRL_RECOLOR));
    static_cast<ObjectUserData*>(lv_obj_get_user_data(copy))->objectJson = record;
    record["btnm"]["recolor"] = true;
    Serialization::LVButtonMatrix::Apply(copy, record["btnm"]);
    CHECK(lv_buttonmatrix_has_button_ctrl(copy, 0, LV_BUTTONMATRIX_CTRL_RECOLOR));
    lv_obj_delete(matrix); lv_obj_delete(copy);
}


TEST_CASE("Custom text boxes have independent native controls and numeric behavior")
{
    using Serialization::CustomWidgets;
    auto* first = CustomWidgets::Create("textBox");
    auto* second = CustomWidgets::Create("textBox");
    REQUIRE(lv_obj_get_class(CustomWidgets::Part(first, "Entry")) == &lv_textarea_class);
    CustomWidgets::Apply(first, {{"title", "Temperature"}, {"min", -10}, {"max", 10}, {"value", 9}, {"step", 3}});
    lv_obj_send_event(CustomWidgets::Part(first, "Plus"), LV_EVENT_CLICKED, nullptr);
    CHECK(CustomWidgets::ToJSON(first)["textBox"]["value"] == 10);
    CHECK(std::string(lv_textarea_get_text(CustomWidgets::Part(first, "Entry"))) == "10");
    CHECK(CustomWidgets::ToJSON(second)["textBox"]["value"] == 0);
    lv_obj_send_event(CustomWidgets::Part(first, "Minus"), LV_EVENT_CLICKED, nullptr);
    CHECK(CustomWidgets::ToJSON(first)["textBox"]["value"] == 7);
    CustomWidgets::Apply(first, {{"readOnly", true}});
    lv_obj_send_event(CustomWidgets::Part(first, "Plus"), LV_EVENT_CLICKED, nullptr);
    CHECK(CustomWidgets::ToJSON(first)["textBox"]["value"] == 7);
    CHECK(lv_obj_has_state(CustomWidgets::Part(first, "Entry"), LV_STATE_DISABLED));
    CHECK_THROWS(CustomWidgets::Apply(first, {{"min", 20}, {"max", 0}}));
    CHECK(CustomWidgets::ToJSON(first)["textBox"]["min"] == -10);
    CustomWidgets::Apply(first, {{"numeric", false}, {"readOnly", false}, {"text", "Custom text"}, {"placeholder", "Type here"}, {"password", true}});
    CHECK(std::string(lv_textarea_get_text(CustomWidgets::Part(first, "Entry"))) == "Custom text");
    CHECK(lv_textarea_get_password_mode(CustomWidgets::Part(first, "Entry")));
    CustomWidgets::Apply(second, {{"text", "-"}});
    const auto incomplete = Serialization::ObjectSerializer::SerializeObject(second);
    auto* incompleteCopy = Serialization::ObjectSerializer::DeserializeObject(incomplete);
    REQUIRE(incompleteCopy != nullptr);
    CHECK(std::string(lv_textarea_get_text(CustomWidgets::Part(incompleteCopy, "Entry"))) == "-");
    lv_obj_delete(incompleteCopy);
    lv_obj_delete(first); lv_obj_delete(second);
}

TEST_CASE("Custom layer views retain hierarchy selection expansion and independent instances")
{
    using Serialization::CustomWidgets;
    auto* first = CustomWidgets::Create("layerView");
    auto* second = CustomWidgets::Create("layerView");
    REQUIRE(CustomWidgets::RowObject(first, 1) != nullptr);
    REQUIRE(CustomWidgets::RowObject(first, 2) != nullptr);
    auto* secondRow = CustomWidgets::RowObject(first, 2);
    lv_obj_send_event(secondRow, LV_EVENT_CLICKED, nullptr);
    CHECK(CustomWidgets::ToJSON(first)["layerView"]["selected"] == 2);
    CHECK(CustomWidgets::ToJSON(second)["layerView"]["selected"] == 1);
    lv_obj_send_event(lv_obj_get_child(CustomWidgets::RowObject(first, 1), 0), LV_EVENT_CLICKED, nullptr);
    CHECK(lv_obj_is_hidden(secondRow));
    auto record = CustomWidgets::ToJSON(first)["layerView"];
    CHECK_FALSE(record["nodes"][0]["expanded"].get<bool>());
    record["nodes"][0]["expanded"] = true;
    CustomWidgets::Apply(first, record);
    CHECK_FALSE(lv_obj_is_hidden(secondRow));
    CHECK(CustomWidgets::RowObject(first, 2) == secondRow);
    auto bad = record;
    bad["nodes"][0]["parent"] = 2;
    CHECK_THROWS(CustomWidgets::Apply(first, bad));
    CHECK(CustomWidgets::ToJSON(first)["layerView"]["nodes"][0]["parent"] == 0);
    record["nodes"].erase(record["nodes"].begin() + 1);
    CustomWidgets::Apply(first, record);
    CHECK(CustomWidgets::RowObject(first, 2) == nullptr);
    CHECK(CustomWidgets::ToJSON(first)["layerView"]["selected"] == 0);
    lv_obj_delete(first); lv_obj_delete(second);
}

TEST_CASE("Custom composites serialize all editable settings internal styles and owned content")
{
    using Serialization::CustomWidgets;
    for (const auto* type : {"textBox", "layerView"})
    {
        CAPTURE(type);
        auto* object = CustomWidgets::Create(type);
        if (std::string(type) == "textBox") CustomWidgets::Apply(object, {{"title", "Speed"}, {"min", -25}, {"max", 125},
            {"value", 42}, {"step", 4}, {"titleWidth", 75}, {"buttonWidth", 30}, {"maxLength", 8}, {"spacing", 4}});
        else CustomWidgets::Apply(object, {{"title", "Layers"}, {"indent", 22}, {"rowHeight", 36}, {"selected", 2}});
        auto* title = CustomWidgets::Part(object, "Title");
        lv_obj_set_style_text_color(title, lv_color_hex(0x8844cc), LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(0xaacc44), LV_PART_MAIN | LV_STATE_DISABLED);
        auto* owned = lv_label_create(CustomWidgets::ContentParent(object));
        lv_label_set_text(owned, "Owned content");
        const auto record = Serialization::ObjectSerializer::SerializeObject(object);
        auto* copy = Serialization::ObjectSerializer::DeserializeObject(record);
        REQUIRE(copy != nullptr);
        REQUIRE(CustomWidgets::Type(copy) != nullptr);
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            const auto result = Serialization::ObjectSerializer::SerializeObject(copy);
            INFO(json::diff(ContentWithoutLocalStyleStorage(record[type]), ContentWithoutLocalStyleStorage(result[type])).dump(2));
            CHECK(ContentWithoutLocalStyleStorage(result[type]) == ContentWithoutLocalStyleStorage(record[type]));
            CustomWidgets::Apply(copy, result[type]);
        }
        CHECK(lv_color_eq(lv_obj_get_style_text_color(CustomWidgets::Part(copy, "Title"), LV_PART_MAIN), lv_color_hex(0x8844cc)));
        lv_obj_delete(object); lv_obj_delete(copy);
    }
}

TEST_CASE("Custom layer view document children preserve content host without duplicate rows")
{
    auto* object = Serialization::CustomWidgets::Create("layerView");
    auto* child = lv_checkbox_create(Serialization::CustomWidgets::ContentParent(object));
    lv_checkbox_set_text(child, "Document checkbox");
    const auto record = Serialization::Page::ToJSON({object, child});
    auto objects = Serialization::Page::FromJSON(record);
    REQUIRE(objects.size() == 2);
    auto* content = Serialization::CustomWidgets::ContentParent(objects[0]);
    CHECK(lv_obj_get_parent(objects[1]) == content);
    CHECK(lv_obj_get_class(objects[1]) == &lv_checkbox_class);
    CHECK(lv_obj_get_child_count(content) == 3);
    lv_obj_delete(object); lv_obj_delete(objects[0]);
}
