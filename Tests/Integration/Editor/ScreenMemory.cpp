#include "ScreenMemory.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <src/core/lv_obj_private.h>
#include <src/core/lv_obj_class_private.h>
#include <src/widgets/button/lv_button_private.h>
#include <src/widgets/label/lv_label_private.h>
#include <src/widgets/textarea/lv_textarea_private.h>
#include <src/widgets/table/lv_table_private.h>
#include <Serialization/Base64.h>
#include <Serialization/LVFont.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ResourceArena.h>
#include "../../../UICreator/Fonts/FontConverter.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

lv_obj_t* Label(lv_obj_t* root, const std::string& text, bool prefix = false)
{
    if (lv_obj_has_class(root, &lv_label_class)) {
        const std::string caption = lv_label_get_text(root);
        if (prefix ? caption.rfind(text, 0) == 0 : caption == text) return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = Label(lv_obj_get_child(root, i), text, prefix)) return found;
    return nullptr;
}

void Drain(SdlSimulator& simulator)
{
    // Refresh uses the existing 200 ms workspace timer, not a test-only hook.
    const auto deadline = SDL_GetTicks() + 260;
    do {
        Require(simulator.ProcessEvents(), "The editor stopped during the screen memory test");
    } while (SDL_GetTicks() < deadline);
    lv_obj_update_layout(lv_screen_active());
}

void ExpectBytes(lv_obj_t* badge, size_t bytes, const char* message)
{
    const auto expected = "Screen Memory: " + std::to_string(bytes) + " bytes";
    if (std::string(lv_label_get_text(badge)) != expected)
        throw std::runtime_error(std::string(message) + ": expected '" + expected +
            "', got '" + lv_label_get_text(badge) + "'");
}

bool Intersects(const lv_area_t& first, const lv_area_t& second)
{
    return first.x1 <= second.x2 && first.x2 >= second.x1 &&
        first.y1 <= second.y2 && first.y2 >= second.y1;
}

json TextRecord(int oid, const char* text)
{
    return {{"oid", oid}, {"par", -1}, {"text", text},
        {"base", {{"coords", {{"x", 12}, {"y", 12 + oid * 30}, {"width", 150}, {"height", 24}}}}}};
}

size_t FontRecordBytes(const json& font)
{
    const auto& d=font.at("dsc");
    size_t bytes=sizeof(lv_font_t)+sizeof(lv_font_fmt_txt_dsc_t)+
        d.at("GlyphDsc").size()*sizeof(lv_font_fmt_txt_glyph_dsc_t)+
        d.at("cmaps").size()*sizeof(lv_font_fmt_txt_cmap_t)+
        base64_decode(d.at("bitmap").get<std::string>()).size()+(d.value("bitmapFmt",0)?1:0);
    for(const auto& map:d.at("cmaps")) {
        bytes+=map.value("unicodeList",json::array()).size()*sizeof(uint16_t);
        const auto count=map.value("glyphIDOffsetList",json::array()).size();
        bytes+=count*(map.value("type",0)==LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL?sizeof(uint8_t):sizeof(uint16_t));
    }
    if(d.contains("Kern")&&d.at("Kern").is_object()) {
        const auto& kern=d.at("Kern");
        bytes+=kern.value("Values",json::array()).size()*sizeof(int8_t);
        if(d.value("kernClasses",0))bytes+=sizeof(lv_font_fmt_txt_kern_classes_t)+
            kern.at("leftClasses").size()*sizeof(uint8_t)+kern.at("rightClasses").size()*sizeof(uint8_t);
        else bytes+=sizeof(lv_font_fmt_txt_kern_pair_t)+kern.value("GlyphIDs",json::array()).size()*
            (kern.value("glyphIDSize",0)?sizeof(uint16_t):sizeof(uint8_t));
    }
    return bytes;
}

json TinyFont(bool compressed=false)
{
    return {{"lineHeight",4},{"baseLine",0},{"dsc",{
        {"bitmap","8A=="},{"bitmapFmt",compressed?2:0},{"bpp",4},{"cmapNum",1},{"kernScale",0},{"kernClasses",0},
        {"GlyphDsc",json::array({{{"bitmapIdx",0},{"adv_w",0},{"box_w",0},{"box_h",0},{"ofs_x",0},{"ofs_y",0}},
            {{"bitmapIdx",0},{"adv_w",16},{"box_w",1},{"box_h",1},{"ofs_x",0},{"ofs_y",0}}})},
        {"cmaps",json::array({{{"rangeStart",65},{"rangeLength",1},{"glyphIDstart",1},{"type",2},{"formatVersion",9},
            {"unicodeList",json::array()},{"glyphIDOffsetList",json::array()}}})},{"Kern",nullptr}}}};
}

struct ExtendedWidget {
    lv_obj_t object;
    uint64_t payload[3];
};

lv_obj_t* DerivedWidget(lv_obj_t* parent, bool extended)
{
    static const lv_obj_class_t inherited = [] {
        lv_obj_class_t value{};
        value.base_class = &lv_obj_class;
        value.name = "screen_memory_inherited";
        return value;
    }();
    static const lv_obj_class_t sized = [] {
        auto value = inherited;
        value.instance_size = sizeof(ExtendedWidget);
        value.name = "screen_memory_extended";
        return value;
    }();
    auto* object = lv_obj_class_create_obj(extended ? &sized : &inherited, parent);
    Require(object, "The derived widget fixture could not be created");
    lv_obj_class_init_obj(object);
    return object;
}
}

void CheckScreenMemory(UI& editor, SdlSimulator& simulator)
{
    auto* title = Label(lv_screen_active(), "Simulator [Size:320x240]");
    Require(title, "The existing simulator title is missing");
    auto* frame = lv_obj_get_parent(lv_obj_get_parent(title));
    auto* header = EditorWindow::Header(frame);
    auto* surface = lv_obj_get_child(EditorWindow::Content(frame), 0);
    auto* badge = Label(header, "Screen Memory: ", true);
    Require(surface && badge && lv_obj_get_parent(badge) == header,
        "The screen memory total is missing from the window frame");
    Require(lv_obj_get_child_count(surface) == 0, "The memory fixture must start with an empty screen");
    ExpectBytes(badge, sizeof(lv_obj_t), "The initial screen object was not counted");
    const size_t defaultFontBytes=FontRecordBytes(Serialization::LVFont::ToJSON(*LV_FONT_DEFAULT));

    Drain(simulator);
    lv_area_t titleArea, badgeArea, headerArea;
    lv_obj_get_coords(title, &titleArea);
    lv_obj_get_coords(badge, &badgeArea);
    lv_obj_get_coords(header, &headerArea);
    Require(badgeArea.x1 >= headerArea.x1 && badgeArea.x2 <= headerArea.x2 &&
        badgeArea.y1 >= headerArea.y1 && badgeArea.y2 <= headerArea.y2,
        "The screen memory badge extends outside its frame");
    Require(!Intersects(titleArea, badgeArea), "The memory badge overlaps the simulator title");
    bool settingsFound = false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(header); ++i) {
        auto* child = lv_obj_get_child(header, i);
        if (!lv_obj_has_class(child, &lv_button_class)) continue;
        settingsFound = true;
        lv_area_t buttonArea;
        lv_obj_get_coords(child, &buttonArea);
        Require(!Intersects(buttonArea, badgeArea), "The memory badge overlaps a frame button");
    }
    Require(settingsFound, "The frame settings button is missing");

    auto* container = lv_obj_create(surface);
    auto* button = lv_button_create(container);
    auto* text = lv_label_create(button);
    lv_label_set_text(text, "Nested button label");
    auto* hidden = lv_label_create(container);
    lv_label_set_text(hidden, "Hidden objects still occupy memory");
    lv_obj_set_hidden(hidden, true);
    auto* textarea = lv_textarea_create(container);
    Require(lv_obj_get_child_count(textarea) == 1, "The textarea fixture must own one internal label");
    auto* outside = lv_label_create(lv_layer_top());
    lv_label_set_text(outside, "Editor chrome is outside the screen");
    const size_t nestedBytes = 2 * sizeof(lv_obj_t) + sizeof(lv_button_t) +
        3 * sizeof(lv_label_t) + sizeof(lv_textarea_t)+defaultFontBytes;
    Drain(simulator);
    ExpectBytes(badge, nestedBytes, "Nested, hidden or internal widgets were counted incorrectly");

    lv_label_set_text(text, "Longer text changes its allocation, but not the LVGL widget instance size");
    Drain(simulator);
    ExpectBytes(badge, nestedBytes, "Text storage was included in the widget instance total");
    lv_obj_delete(outside);
    Drain(simulator);
    ExpectBytes(badge, nestedBytes, "An unrelated editor object changed the screen memory total");

    auto* inherited = DerivedWidget(container, false);
    auto* extended = DerivedWidget(container, true);
    Drain(simulator);
    ExpectBytes(badge, nestedBytes + sizeof(lv_obj_t) + sizeof(ExtendedWidget),
        "Inherited or extended widget classes did not match LVGL instance allocation sizes");
    lv_obj_delete(inherited);
    lv_obj_delete(extended);
    lv_obj_delete(container);
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t), "Deleting a widget subtree did not reduce the byte count");

    lv_obj_send_event(badge,LV_EVENT_HOVER_OVER,nullptr);
    Require(Label(lv_layer_sys(),"LVGL widget instances plus unique used bitmap-font",true),"The memory badge does not explain its font/data estimate");
    lv_obj_send_event(badge,LV_EVENT_HOVER_LEAVE,nullptr);

    // Native-sized payloads are separate from text length and source TTF size.
    // Reusing one font across widgets must not charge its tables repeatedly.
    auto raw=TinyFont(),compressed=TinyFont(true);
    auto& rawFont=Serialization::LVFont::FromJSON(raw);
    auto& compressedFont=Serialization::LVFont::FromJSON(compressed);
    const size_t tinyBytes=sizeof(lv_font_t)+sizeof(lv_font_fmt_txt_dsc_t)+2*sizeof(lv_font_fmt_txt_glyph_dsc_t)+sizeof(lv_font_fmt_txt_cmap_t)+1;
    Require(FontRecordBytes(raw)==tinyBytes,"The independent raw-font fixture size is inconsistent");
    auto* firstLabel=lv_label_create(surface);lv_label_set_text(firstLabel,"A");lv_obj_set_style_text_font(firstLabel,&rawFont,LV_PART_MAIN);
    Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+tinyBytes,"A used font's native tables/bitmap were not counted");
    auto* secondLabel=lv_label_create(surface);lv_label_set_text(secondLabel,"A");lv_obj_set_style_text_font(secondLabel,&rawFont,LV_PART_MAIN);
    Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+2*sizeof(lv_label_t)+tinyBytes,"A shared font was counted more than once");
    lv_obj_set_style_text_font(firstLabel,&compressedFont,static_cast<lv_style_selector_t>(LV_PART_MAIN)|LV_STATE_PRESSED);
    Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+2*sizeof(lv_label_t)+tinyBytes+FontRecordBytes(compressed),"Inactive state fonts or compressed bitmap guard bytes were omitted");
    lv_obj_remove_local_style_prop(firstLabel,LV_STYLE_TEXT_FONT,static_cast<lv_style_selector_t>(LV_PART_MAIN)|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(surface,&compressedFont,LV_PART_MAIN);lv_obj_remove_local_style_prop(secondLabel,LV_STYLE_TEXT_FONT,LV_PART_MAIN);
    Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+2*sizeof(lv_label_t)+tinyBytes+FontRecordBytes(compressed),"Canvas/inherited font data was not counted");
    lv_obj_remove_local_style_prop(surface,LV_STYLE_TEXT_FONT,LV_PART_MAIN);
    lv_obj_delete(firstLabel);lv_obj_delete(secondLabel);
    Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t),"Unused catalog fonts remained in the active screen total");

    auto* table=lv_table_create(surface);lv_table_set_cell_value(table,0,0,"A");
    lv_obj_set_style_text_font(table,&rawFont,LV_PART_ITEMS);Drain(simulator);
    Require(lv_obj_get_child_count(table)==0,"The table fixture unexpectedly owns child widgets");
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_table_t)+tinyBytes,"A table's ITEMS font invented an unused MAIN/default font");
    lv_obj_delete(table);Drain(simulator);

    auto sparse=raw;auto& sparseData=sparse["dsc"];sparseData["bitmap"]="8PA=";
    auto secondGlyph=sparseData["GlyphDsc"][1];secondGlyph["bitmapIdx"]=1;sparseData["GlyphDsc"].push_back(secondGlyph);
    sparseData["cmaps"][0]["rangeLength"]=10;sparseData["cmaps"][0]["type"]=LV_FONT_FMT_TXT_CMAP_SPARSE_FULL;
    sparseData["cmaps"][0]["unicodeList"]=json::array({0,9});sparseData["cmaps"][0]["glyphIDOffsetList"]=json::array({0,1});
    sparseData["kernScale"]=16;sparseData["kernClasses"]=1;
    sparseData["Kern"]={{"leftCount",1},{"rightCount",1},{"leftClasses",json::array({0,1,1})},{"rightClasses",json::array({0,1,1})},{"Values",json::array({-1})}};
    auto* kernLabel=lv_label_create(surface);lv_label_set_text(kernLabel,"A");
    lv_obj_set_style_text_font(kernLabel,&Serialization::LVFont::FromJSON(sparse),LV_PART_MAIN);Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+FontRecordBytes(sparse),"Sparse Unicode/offset arrays or kerning classes were omitted");
    for(const int width:{0,1}) {
        sparseData["kernClasses"]=0;sparseData["Kern"]={{"glyphIDSize",width},{"GlyphIDs",json::array({1,2})},{"Values",json::array({-1})}};
        lv_obj_set_style_text_font(kernLabel,&Serialization::LVFont::FromJSON(sparse),LV_PART_MAIN);Drain(simulator);
        ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+FontRecordBytes(sparse),"The native kerning pair ID width was counted incorrectly");
    }
    lv_obj_delete(kernLabel);Drain(simulator);

    // A fallback can be shared or cyclic; include its real resource once.
    lv_font_t withFallback=rawFont;withFallback.fallback=&compressedFont;
    auto* fallbackLabel=lv_label_create(surface);lv_label_set_text(fallbackLabel,"A");lv_obj_set_style_text_font(fallbackLabel,&withFallback,LV_PART_MAIN);
    Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+tinyBytes+FontRecordBytes(compressed),"Fallback font storage was omitted");
    withFallback.fallback=&withFallback;Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+tinyBytes,"A fallback cycle duplicated resources or retained the previous chain");
    lv_obj_delete(fallbackLabel);Drain(simulator);

    const auto sourceRoot=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    FontConversion::Options options;options.sources.push_back({sourceRoot/"3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf","0x41-0x43"});
    options.size=24;const auto small=FontConversion::Prepare(options);Require(small.success,"Could not prepare the 24px memory fixture");
    options.size=32;const auto large=FontConversion::Prepare(options);Require(large.success,"Could not prepare the 32px memory fixture");
    Require(FontRecordBytes(large.font)>FontRecordBytes(small.font),"The larger fixture did not increase font storage");
    auto* resized=lv_label_create(surface);lv_label_set_text(resized,"ABC");
    lv_obj_set_style_text_font(resized,&Serialization::LVFont::FromJSON(small.font),LV_PART_MAIN);Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+FontRecordBytes(small.font),"The 24px font estimate is incorrect");
    lv_obj_set_style_text_font(resized,&Serialization::LVFont::FromJSON(large.font),LV_PART_MAIN);Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+FontRecordBytes(large.font),"Changing font size did not refresh its storage estimate");
    lv_obj_delete(resized);Drain(simulator);

    const json canvas = {{"width", 320}, {"height", 240}};
    const auto first = json::array({TextRecord(0, "First screen")});
    const auto second = json::array({TextRecord(0, "Second screen"), TextRecord(1, "Another label")});
    json document = {{"canvas", canvas}, {"obj", first}, {"variables", json::array()},
        {"activeScreen", 0}, {"editorScreen", 0}, {"screens", json::array({
            {{"id", "memory-first"}, {"name", "First"}, {"canvas", canvas}, {"obj", first}},
            {{"id", "memory-second"}, {"name", "Second"}, {"canvas", canvas}, {"obj", second}}
        })}};
    editor.ApplyDocument(document);
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t) + sizeof(lv_label_t)+defaultFontBytes, "Loading a document did not refresh the total");
    Require(editor.SelectScreen(1), "The second screen could not be selected");
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t) + 2 * sizeof(lv_label_t)+defaultFontBytes, "Switching screens retained the previous byte count");
    Require(editor.SelectScreen(0), "The first screen could not be selected again");
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t) + sizeof(lv_label_t)+defaultFontBytes, "Switching back did not refresh the byte count");

    auto fonts=json::array({{{"id","raw"},{"name","Raw"},{"font",raw}},{{"id","compressed"},{"name","Compressed"},{"font",compressed}},
        {{"id","unused"},{"name","Unused larger font"},{"font",large.font}}});
    document["fonts"]=fonts;
    document["screens"][0]["obj"][0]["base"]["fontId"]="raw";
    for(auto& label:document["screens"][1]["obj"])label["base"]["fontId"]="compressed";
    document["obj"]=document["screens"][0]["obj"];
    editor.ApplyDocument(document);Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+tinyBytes,"Inactive-screen or unused fonts inflated the current screen");
    Require(editor.SelectScreen(1),"The font-bearing second screen could not be selected");Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+2*sizeof(lv_label_t)+FontRecordBytes(compressed),"Screen switching did not replace its unique used fonts");
    Require(editor.SelectScreen(0),"The font-bearing first screen could not be selected again");Drain(simulator);
    ExpectBytes(badge,sizeof(lv_obj_t)+sizeof(lv_label_t)+tinyBytes,"Switching back retained fonts from another screen");

    std::cout << "Screen memory covers native widgets, shared/default/state/inherited/fallback fonts, compressed bitmaps, source-size independence and active-screen updates\n";
}
