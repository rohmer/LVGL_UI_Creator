#include "FontConverter.h"
#include <Serialization/LVFont.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(condition) do { if(!(condition)) throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": " #condition); } while(0)
using namespace FontConversion;
int main(int argc,char** argv) {
    try {
        bool fixture=argc==4&&std::string(argv[1])=="--fixture";
        if(!fixture&&argc!=3)return 2;
        const auto root=std::filesystem::path(argv[fixture?2:1]),out=std::filesystem::path(argv[fixture?3:2]);
        std::filesystem::create_directories(out);
        Options o;o.sources.push_back({root/"3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf"});
        o.output=out/"converted_font.c";o.name="converted_font";
        auto result=Convert(o);if(!result.success)throw std::runtime_error(result.message);
        CHECK(std::filesystem::is_regular_file(o.output));CHECK(std::filesystem::is_regular_file(out/"converted_font.h"));CHECK(!result.preview.empty());
        if(fixture)return 0;
        lv_init();auto* display=lv_display_create(800,600);
        auto* font=lv_binfont_create_from_buffer(result.preview.data(),static_cast<uint32_t>(result.preview.size()));CHECK(font);
        lv_font_glyph_dsc_t glyph{};CHECK(font->get_glyph_dsc(font,&glyph,'A','V'));CHECK(glyph.adv_w>0);
        const auto record=Serialization::LVFont::ToJSON(*font);CHECK(record["dsc"].contains("bitmap"));
        auto& copy=Serialization::LVFont::FromJSON(Serialization::Protobuf::Decode(Serialization::Protobuf::Encode(Serialization::EntityKind::Font,"font",record)).data);
        CHECK(Serialization::LVFont::ToJSON(copy)==record);
        auto* label=lv_label_create(lv_screen_active());Serialization::LVObject::FromJSON(json::object(),label);lv_obj_set_style_text_font(label,&copy,0);lv_label_set_text(label,"AV hello");
        auto* restored=Serialization::ObjectSerializer::FromProtobuf(Serialization::ObjectSerializer::ToProtobuf(label));CHECK(restored);
        CHECK(lv_obj_get_style_text_font(restored,LV_PART_MAIN)->line_height==font->line_height);
        lv_obj_delete(restored);lv_obj_delete(label);lv_binfont_destroy(font);
        for(int bpp:{1,2,3,4,8}) {
            o.bpp=bpp;o.format="bin";o.output=out/("font_"+std::to_string(bpp)+".bin");o.compress=false;
            auto converted=Convert(o);if(!converted.success)throw std::runtime_error(converted.message);
            auto* loaded=lv_binfont_create_from_buffer(converted.preview.data(),static_cast<uint32_t>(converted.preview.size()));CHECK(loaded);
            CHECK(loaded->get_glyph_dsc(loaded,&glyph,'A',0));lv_binfont_destroy(loaded);
        }
        o.bpp=4;o.format="lvgl";o.output=out/"aligned_font.c";o.stride=4;o.align=16;o.kerning=false;o.prefilter=false;o.colorInfo=true;
        o.fallback="lv_font_montserrat_14";o.include="lvgl.h";o.relativeRoot=root;
        CHECK(Convert(o).success);
        o.stride=0;o.align=1;o.fallback.clear();o.sources[0].symbols=u8"\u00e9\u20ac";
        for(int smoothing:{1,2}) {o.smoothing=smoothing;o.output=out/("lcd_"+std::to_string(smoothing)+".c");CHECK(Convert(o).success);}
        o.smoothing=0;o.sources[0].symbols.clear();
        auto trueType=o.sources[0].path;o.sources[0].path=root/"3rdParty/LVGL/legacy/lvgl/scripts/built_in_font/Roboto-Regular.woff";
        o.output=out/"woff.c";CHECK(Convert(o).success);o.sources[0].path=trueType;
        o.stride=0;o.align=1;o.fastKerning=true;o.kerning=true;
        auto unicode=out/std::filesystem::u8path(u8"font with spaces & \u65e5\u672c.ttf");std::filesystem::copy_file(o.sources[0].path,unicode,std::filesystem::copy_options::overwrite_existing);
        o.sources.push_back({unicode,"0x41-0x42=>0xE000","",1});o.format="dump";o.output=out/"dump";o.fullInfo=true;
        auto dump=Convert(o);if(!dump.success)throw std::runtime_error(dump.message);CHECK(dump.files.size()>1);
        auto* merged=lv_binfont_create_from_buffer(dump.preview.data(),static_cast<uint32_t>(dump.preview.size()));CHECK(merged);CHECK(merged->get_glyph_dsc(merged,&glyph,0xE000,0));lv_binfont_destroy(merged);
        o.sources.resize(1);o.format="lvgl";o.output=out/"invalid.c";o.compress=true;o.stride=4;
        CHECK(!Convert(o).success);CHECK(!std::filesystem::exists(o.output));
        o.stride=0;o.name="not a C identifier";CHECK(!Convert(o).success);
        o.name="converted_font";o.sources[0].ranges="not a range";CHECK(!Convert(o).success);
        o.sources[0].ranges="0x20-0x7E";std::atomic<bool> cancel{true};CHECK(!Convert(o,&cancel).success);
        lv_display_delete(display);lv_deinit();std::cout<<"Font conversion, preview, serialization, formats, merging and validation passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
