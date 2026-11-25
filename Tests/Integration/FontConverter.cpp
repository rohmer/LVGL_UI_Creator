#include "FontConverter.h"
#include <Serialization/LVFont.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <Serialization/Xml.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <future>
#include <ft2build.h>
#include FT_FREETYPE_H
#define CHECK(condition) do { if(!(condition)) throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": " #condition); } while(0)
using namespace FontConversion;
namespace {
std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary); CHECK(file);
    return {(std::istreambuf_iterator<char>(file)), {}};
}
// Compare LVGL's actual decoded pixels and metrics against a fresh FreeType
// rasterization, independently of the native writer and its private model.
void CheckGlyph(const lv_font_t* font, const std::filesystem::path& source, int bpp, int size = 24, bool referenceKerning = false, int smoothing = 0) {
    const auto bytes = ReadBytes(source);
    FT_Library library = nullptr; FT_Face face = nullptr;
    CHECK(!FT_Init_FreeType(&library));
    CHECK(!FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face));
    CHECK(!FT_Set_Pixel_Sizes(face, 0, size));
    const auto mode=smoothing==1?FT_LOAD_TARGET_LCD:smoothing==2?FT_LOAD_TARGET_LCD_V:bpp==1?FT_LOAD_TARGET_MONO:FT_LOAD_TARGET_LIGHT;
    CHECK(!FT_Load_Char(face, 'A', FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | mode));
    lv_font_glyph_dsc_t glyph{}; CHECK(lv_font_get_glyph_dsc(font, &glyph, 'A', 0));
    const auto& expected = face->glyph->bitmap;
    CHECK(glyph.box_w == expected.width && glyph.box_h == expected.rows);
    CHECK(glyph.ofs_x == face->glyph->bitmap_left && glyph.ofs_y == face->glyph->bitmap_top - int(expected.rows));
    const auto advance16 = std::lround(face->glyph->linearHoriAdvance / 4096.0);
    CHECK(glyph.adv_w == ((advance16 + 8) >> 4));
    if(referenceKerning && FT_HAS_KERNING(face)) {
        FT_Vector kerning{};CHECK(!FT_Get_Kerning(face,FT_Get_Char_Index(face,'A'),FT_Get_Char_Index(face,'V'),FT_KERNING_UNSCALED,&kerning));
        lv_font_glyph_dsc_t pair{};CHECK(lv_font_get_glyph_dsc(font,&pair,'A','V'));
        const auto delta=std::lround(double(kerning.x)*size*16/face->units_per_EM);
        if(pair.adv_w!=((advance16+delta+8)>>4))throw std::runtime_error("Kerning differs: bpp="+std::to_string(bpp)+" advance16="+std::to_string(advance16)+" delta16="+std::to_string(delta)+" actual="+std::to_string(pair.adv_w));
    }
    CHECK(font->line_height == int(std::ceil(double(face->ascender) * size / face->units_per_EM) - std::floor(double(face->descender) * size / face->units_per_EM)));
    auto* draw = lv_draw_buf_create(glyph.box_w, glyph.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO); CHECK(draw);
    CHECK(lv_font_get_glyph_bitmap(&glyph, draw));
    for (unsigned y = 0; y < expected.rows; ++y) for (unsigned x = 0; x < expected.width; ++x) {
        const auto* row = expected.buffer + size_t(y) * expected.pitch;
        unsigned alpha = bpp == 1 ? ((row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0) : row[x];
        const auto maximum = (1u << bpp) - 1;
        if(bpp==3) { // LVGL retains this historical, slightly nonuniform table.
            static constexpr uint8_t opacity3[8]={0,36,73,109,146,182,218,255};
            alpha=opacity3[alpha>>5];
        } else alpha = ((alpha >> (8 - bpp)) * 255 + maximum / 2) / maximum;
        const auto actual=draw->data[size_t(y) * draw->header.stride + x];
        if(actual!=alpha)throw std::runtime_error("Decoded glyph pixel differs: bpp="+std::to_string(bpp)+" x="+std::to_string(x)+" y="+std::to_string(y)+" actual="+std::to_string(actual)+" expected="+std::to_string(alpha));
    }
    lv_font_glyph_release_draw_data(&glyph); lv_draw_buf_destroy(draw);
    FT_Done_Face(face); FT_Done_FreeType(library);
}
}
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
        auto kernFixture=o;kernFixture.sources[0].path=root/"Tests/Integration/FontFixtures/UICreatorFontFixture.woff2";
        kernFixture.fastKerning=true;kernFixture.name="converted_kern_font";kernFixture.output=out/"converted_kern_font.c";
        auto kernResult=Convert(kernFixture);if(!kernResult.success)throw std::runtime_error(kernResult.message);
        if(fixture)return 0;
        // The engine (including PNG glyph dumps) must not depend on lv_init,
        // LVGL's UI heap, Node, Python, or its resource directories.
        auto noRuntime = o; noRuntime.format = "dump"; noRuntime.output = out/"before-lvgl-init";
        auto earlyDump = Convert(noRuntime); CHECK(earlyDump.success);
        auto png = ReadBytes(noRuntime.output/"41.png"); CHECK(png.size()>24 && png[0]==137 && png[1]=='P' && png[2]=='N' && png[3]=='G');
        auto parallel = [&](int id) { auto request = o; request.name="parallel_font_"+std::to_string(id); request.output=out/(request.name+".c"); return Convert(request); };
        auto first = std::async(std::launch::async, parallel, 1), second = std::async(std::launch::async, parallel, 2);
        CHECK(first.get().success && second.get().success);
        lv_init();auto* display=lv_display_create(800,600);
        auto* font=lv_binfont_create_from_buffer(result.preview.data(),static_cast<uint32_t>(result.preview.size()));CHECK(font);
        CheckGlyph(font,o.sources[0].path,4);
        // Adding a project font must never require or create technical outputs.
        // Verify the actual stored resource, rather than just its UI preview.
        auto assetOptions=o;assetOptions.output=out/"must-not-be-created"/"asset.c";
        assetOptions.name="Friendly font name";assetOptions.include.clear();assetOptions.format="ignored";
        for(int bpp:{1,2,3,4,8}) {
            assetOptions.bpp=bpp;
            auto asset=Prepare(assetOptions);if(!asset.success)throw std::runtime_error(asset.message);
            CHECK(asset.files.empty()&&!std::filesystem::exists(assetOptions.output.parent_path()));
            CHECK(asset.font["dsc"]["bpp"]==bpp);
            CHECK(asset.font["dsc"]["bitmapFmt"]==((bpp==1||bpp==8)?0:1));
            auto& stored=Serialization::LVFont::FromJSON(asset.font);
            CheckGlyph(&stored,o.sources[0].path,bpp);
            auto* preview=lv_binfont_create_from_buffer(asset.preview.data(),static_cast<uint32_t>(asset.preview.size()));CHECK(preview);
            CheckGlyph(preview,o.sources[0].path,bpp);
            lv_font_glyph_dsc_t storedPair{},previewPair{};
            CHECK(lv_font_get_glyph_dsc(&stored,&storedPair,'A','V'));
            CHECK(lv_font_get_glyph_dsc(preview,&previewPair,'A','V'));
            CHECK(storedPair.adv_w==previewPair.adv_w);lv_binfont_destroy(preview);
            const auto protobuf=Serialization::Protobuf::Decode(Serialization::Protobuf::Encode(Serialization::EntityKind::Font,"asset",asset.font));
            CHECK(protobuf.data==asset.font);
            auto& fromProtobuf=Serialization::LVFont::FromJSON(protobuf.data);
            CheckGlyph(&fromProtobuf,o.sources[0].path,bpp);
            const auto xml=Serialization::Xml::Decode(Serialization::Xml::Encode(Serialization::EntityKind::Font,"asset",asset.font));
            CHECK(xml.data==asset.font);
            auto& fromXml=Serialization::LVFont::FromJSON(xml.data);
            CheckGlyph(&fromXml,o.sources[0].path,bpp);
        }
        // This fixture has independently checked legacy kerning; the full
        // source font also has GPOS rules that differ from FT_Get_Kerning.
        for(bool classes:{false,true}) {
            auto settings=kernFixture;settings.fastKerning=classes;
            auto asset=Prepare(settings);CHECK(asset.success);
            CheckGlyph(&Serialization::LVFont::FromJSON(asset.font),o.sources[0].path,4,24,true);
        }
        assetOptions.bpp=4;assetOptions.compress=false;assetOptions.stride=4;assetOptions.align=16;
        auto alignedAsset=Prepare(assetOptions);CHECK(alignedAsset.success);
        auto& alignedFont=Serialization::LVFont::FromJSON(alignedAsset.font);
        CheckGlyph(&alignedFont,o.sources[0].path,4);
        // Stored raster data remains usable after moving/removing the source.
        auto offlineAsset=alignedAsset.font;
        assetOptions.sources[0].path=out/"no-source-font-here.ttf";
        CHECK(!Prepare(assetOptions).success);
        CheckGlyph(&Serialization::LVFont::FromJSON(offlineAsset),o.sources[0].path,4);
        // Large display fonts must work in the editor as well as in generated
        // firmware configured for large descriptors.
        auto largeOptions=o;largeOptions.size=512;largeOptions.sources[0].ranges="0x41";
        auto largeAsset=Prepare(largeOptions);CHECK(largeAsset.success);
        CHECK(largeAsset.font["dsc"]["GlyphDsc"][1]["box_w"].get<int>()>255);
        auto& largeFont=Serialization::LVFont::FromJSON(largeAsset.font);
        CheckGlyph(&largeFont,o.sources[0].path,4,512);
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
            auto converted=Convert(o);
            if(bpp==3)CHECK(!converted.success);
            if(bpp!=3) {
                if(!converted.success)throw std::runtime_error(converted.message);
                auto* loaded=lv_binfont_create_from_buffer(converted.preview.data(),static_cast<uint32_t>(converted.preview.size()));CHECK(loaded);
                CHECK(loaded->get_glyph_dsc(loaded,&glyph,'A',0));lv_binfont_destroy(loaded);
                auto exported=ReadBytes(o.output);auto* raw=lv_binfont_create_from_buffer(exported.data(),static_cast<uint32_t>(exported.size()));CHECK(raw);
                CheckGlyph(raw,o.sources[0].path,bpp);lv_binfont_destroy(raw);
            }
            o.compress=true;o.output=out/("compressed_"+std::to_string(bpp)+".bin");
            auto compressed=Convert(o);if(!compressed.success)throw std::runtime_error(compressed.message);
            auto exported=ReadBytes(o.output);auto* packed=lv_binfont_create_from_buffer(exported.data(),static_cast<uint32_t>(exported.size()));CHECK(packed);
            CheckGlyph(packed,o.sources[0].path,bpp);lv_binfont_destroy(packed);
            if(bpp>1) {o.prefilter=false;o.output=out/("compressed_plain_"+std::to_string(bpp)+".bin");CHECK(Convert(o).success);exported=ReadBytes(o.output);packed=lv_binfont_create_from_buffer(exported.data(),static_cast<uint32_t>(exported.size()));CHECK(packed);CheckGlyph(packed,o.sources[0].path,bpp);lv_binfont_destroy(packed);o.prefilter=true;}
        }
        o.bpp=4;o.compress=false;o.format="lvgl";o.output=out/"aligned_font.c";o.stride=4;o.align=16;o.kerning=false;o.prefilter=false;o.colorInfo=true;
        o.fallback="lv_font_montserrat_14";o.include="lvgl.h";o.relativeRoot=root;
        CHECK(Convert(o).success);
        o.stride=0;o.align=1;o.fallback.clear();o.sources[0].symbols=u8"\u00e9\u20ac";
        for(int smoothing:{1,2}) {
            o.smoothing=smoothing;o.output=out/("lcd_"+std::to_string(smoothing)+".c");auto lcd=Convert(o);CHECK(lcd.success);
            const auto expected=smoothing==1?LV_FONT_SUBPX_HOR:LV_FONT_SUBPX_VER;
            auto* lcdFont=lv_binfont_create_from_buffer(lcd.preview.data(),static_cast<uint32_t>(lcd.preview.size()));CHECK(lcdFont&&lcdFont->subpx==expected);
            CheckGlyph(lcdFont,o.sources[0].path,4,24,false,smoothing);lv_binfont_destroy(lcdFont);
            auto binary=o;binary.format="bin";binary.output=out/("lcd_"+std::to_string(smoothing)+".bin");CHECK(Convert(binary).success);
            auto exported=ReadBytes(binary.output);lcdFont=lv_binfont_create_from_buffer(exported.data(),static_cast<uint32_t>(exported.size()));CHECK(lcdFont&&lcdFont->subpx==expected);
            CheckGlyph(lcdFont,o.sources[0].path,4,24,false,smoothing);lv_binfont_destroy(lcdFont);
        }
        o.smoothing=0;o.sources[0].symbols.clear();
        auto trueType=o.sources[0].path;o.sources[0].path=root/"3rdParty/LVGL/legacy/lvgl/scripts/built_in_font/Roboto-Regular.woff";
        o.output=out/"woff.c";CHECK(Convert(o).success);o.sources[0].path=trueType;
        o.sources[0].path=root/"Tests/Integration/FontFixtures/UICreatorFontFixture.woff2";o.output=out/"woff2.c";o.kerning=true;o.fastKerning=true;
        auto woff2=Convert(o);if(!woff2.success)throw std::runtime_error(woff2.message);
        auto* woff2Font=lv_binfont_create_from_buffer(woff2.preview.data(),static_cast<uint32_t>(woff2.preview.size()));CHECK(woff2Font);
        CheckGlyph(woff2Font,trueType,4,24,true);lv_binfont_destroy(woff2Font);o.sources[0].path=trueType;
        o.stride=0;o.align=1;o.fastKerning=true;o.kerning=true;
        auto unicode=out/std::filesystem::u8path(u8"font with spaces & \u65e5\u672c.ttf");std::filesystem::copy_file(o.sources[0].path,unicode,std::filesystem::copy_options::overwrite_existing);
        o.sources.push_back({unicode,"0x41-0x42=>0xE000","",1});o.format="dump";o.output=out/"dump";o.fullInfo=true;
        auto dump=Convert(o);if(!dump.success)throw std::runtime_error(dump.message);CHECK(dump.files.size()>1);
        const auto infoBytes=ReadBytes(o.output/"font_info.json");const auto info=json::parse(infoBytes.begin(),infoBytes.end());
        bool partialOpacity=false;
        for(const auto& record:info.at("glyphs")) {
            CHECK(record.contains("freetype"));
            for(const auto& row:record.at("pixels"))for(const auto& pixel:row) {const auto value=pixel.get<unsigned>();CHECK(value%17==0);partialOpacity|=value>0&&value<255;}
        }
        CHECK(partialOpacity);
        auto* merged=lv_binfont_create_from_buffer(dump.preview.data(),static_cast<uint32_t>(dump.preview.size()));CHECK(merged);CHECK(merged->get_glyph_dsc(merged,&glyph,0xE000,0));lv_binfont_destroy(merged);
        o.sources.resize(1);o.format="lvgl";o.output=out/"invalid.c";o.compress=true;o.stride=4;
        CHECK(!Convert(o).success);CHECK(!std::filesystem::exists(o.output));
        o.stride=0;o.name="not a C identifier";CHECK(!Convert(o).success);
        for(const char* name:{"class","namespace","constexpr","bool","NULL"}) {o.name=name;CHECK(!Convert(o).success);}
        o.name="converted_font";o.sources[0].ranges="not a range";CHECK(!Convert(o).success);
        o.sources[0].ranges="0x20-0x7E";std::atomic<bool> cancel{true};CHECK(!Convert(o,&cancel).success);
        o.sources[0].symbols=std::string("\xC0\xAF",2);CHECK(!Convert(o).success);o.sources[0].symbols.clear();
        // The generated companion header must not overwrite another input.
        const auto protectedHeader=out/"protected.h",protectedSource=out/"protected.c";
        std::filesystem::copy_file(trueType,protectedHeader,std::filesystem::copy_options::overwrite_existing);
        {std::ofstream sentinel(protectedSource);sentinel<<"Existing user output\n";}
        const auto original=ReadBytes(protectedHeader),existing=ReadBytes(protectedSource);
        o.sources[0].path=protectedHeader;o.output=protectedSource;CHECK(!Convert(o).success);
        CHECK(ReadBytes(protectedHeader)==original&&ReadBytes(protectedSource)==existing);
        lv_display_delete(display);lv_deinit();std::cout<<"Font conversion, preview, serialization, formats, merging and validation passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
