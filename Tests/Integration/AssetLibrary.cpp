#include "ImageConverter.h"
#include "../../UICreator/Fonts/FontLibrary.h"
#include "../../UICreator/Images/ImageLibrary.h"
#include <Serialization/Project.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Base64.h>
#include <Serialization/Xml.h>
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../CodeGeneration/XmlCodeGenerator.h"
#include <fstream>
#include <iostream>
#define CHECK(condition) do {if(!(condition))throw std::runtime_error(std::string("Check failed line ")+std::to_string(__LINE__)+": " #condition);}while(0)
using namespace Serialization;
ImageConversion::Result Converted(const std::filesystem::path& path,const json& options,const std::filesystem::path& output={}) {auto result=ImageConversion::Convert({path},options,output);if(!result.success)throw std::runtime_error(result.message);return result;}
json TinyCompressedFont() {
    return {{"lineHeight",4},{"baseLine",0},{"dsc",{
        {"bitmap","8A=="},{"bitmapFmt",2},{"bpp",4},{"cmapNum",1},{"kernScale",0},{"kernClasses",0},
        {"GlyphDsc",json::array({{{"bitmapIdx",0},{"adv_w",0},{"box_w",0},{"box_h",0},{"ofs_x",0},{"ofs_y",0}},
            {{"bitmapIdx",0},{"adv_w",16},{"box_w",1},{"box_h",1},{"ofs_x",0},{"ofs_y",0}}})},
        {"cmaps",json::array({{{"rangeStart",65},{"rangeLength",1},{"glyphIDstart",1},{"type",2},{"formatVersion",9}}})},
        {"Kern",json::object()}}}};
}
int main(int argc,char** argv) {
    try {
        const bool fixture=argc==4&&std::string(argv[1])=="--fixture";if(!fixture&&argc!=3)return 2;
        const auto root=std::filesystem::path(argv[fixture?2:1]),out=std::filesystem::path(argv[fixture?3:2]);std::filesystem::create_directories(out);
        const auto png=root/"Tests/Fixtures/Images/sample.png",gif=root/"Tests/Fixtures/Images/sample.gif";
        lv_init();auto* display=lv_display_create(800,600);
        FontLibrary library;auto id=library.Add("Body",LVFont::ToJSON(*LV_FONT_DEFAULT),
            {{"output","private/source/location/font.c"},{"name","obsolete_output_symbol"},{"include","private_font_include.h"}});auto* first=ObjectSerializer::Create("label"),*second=ObjectSerializer::Create("label");
        library.Apply(first,id);library.Apply(second,id);CHECK(library.Get(id)==lv_obj_get_style_text_font(first,LV_PART_MAIN));
        auto* pointer=library.Get(id);library.Rename(id,u8"Body / renamed + \"quoted\" \u65e5\u672c\u8a9e");CHECK(library.Get(id)==pointer);CHECK(library.Usage(lv_screen_active(),id)==2);
        auto lz4=Converted(png,{{"colorFormat","RGB565A8"},{"compression","LZ4"},{"align",8}});
        auto rle=Converted(png,{{"colorFormat","ARGB8888"},{"compression","RLE"}});
        auto animation=Converted(gif,{{"colorFormat","RGB565"},{"compression","LZ4"}});
        auto raw=Converted(png,{{"colorFormat","RAW_ALPHA"}});
        std::vector<lv_obj_t*> objects{first,second};
        for(const auto& result:{lz4,rle,animation,raw}) {std::cerr<<"Applying converted image "<<result.assets[0]["conversion"]<<'\n';auto* image=ObjectSerializer::Create("image");ImageConversion::Apply(image,result.assets[0]);objects.push_back(image);auto bytes=ObjectSerializer::ToProtobuf(image);auto* copy=ObjectSerializer::FromProtobuf(bytes);CHECK(ObjectSerializer::SerializeObject(copy)["img"]["asset"]==ObjectSerializer::SerializeObject(image)["img"]["asset"]);lv_obj_delete(copy);}
        ImageLibrary images;auto imageId=images.Add("Shared image",lz4.assets[0]);CHECK(images.Add("Duplicate",lz4.assets[0])==imageId);
        images.Apply(objects[2],imageId);auto* shared=ObjectSerializer::Create("image");objects.push_back(shared);images.Apply(shared,imageId);CHECK(images.Usage(lv_screen_active(),imageId)==2);
        images.Rename(imageId,"Shared renamed");CHECK(ImageLibrary::Selection(objects[2])==imageId);
        auto unused=images.Add("Unused",rle.assets[0]);images.Remove(unused);CHECK(images.ToJSON().size()==1);
        auto* imageButton=ObjectSerializer::Create("imagebutton");objects.push_back(imageButton);lv_obj_set_size(imageButton,120,40);
        for(size_t state=0;state<LV_IMAGEBUTTON_STATE_NUM;++state)for(size_t side=0;side<3;++side){images.ApplyButton(imageButton,state,side,imageId);CHECK(ImageLibrary::ButtonSelection(imageButton,state,side)==imageId);}
        CHECK(images.Usage(lv_screen_active(),imageId)==3);images.ApplyButton(imageButton,0,0,"",true);CHECK(ImageLibrary::ButtonSelection(imageButton,0,0).empty());images.ApplyButton(imageButton,0,0,"");CHECK(lv_imagebutton_get_src_left(imageButton,LV_IMAGEBUTTON_STATE_RELEASED)==nullptr);images.ApplyButton(imageButton,0,0,imageId);
        CHECK(lv_obj_get_style_width(imageButton,LV_PART_MAIN)==120);CHECK(lv_obj_get_style_height(imageButton,LV_PART_MAIN)==40);
        auto buttonRecord=ObjectSerializer::SerializeObject(imageButton);auto* buttonCopy=ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(imageButton));CHECK(ObjectSerializer::SerializeObject(buttonCopy)["imagebutton"]["states"]==buttonRecord["imagebutton"]["states"]);lv_obj_delete(buttonCopy);
        auto document=Project::ToJSON(objects,lv_screen_active(),json::array(),800,600,library.ToJSON(),images.ToJSON());
        CHECK(document["images"].size()==1);CHECK(document["obj"][2]["img"]["asset"]["projectImage"]==imageId);CHECK(document["obj"][6]["img"]["asset"]["projectImage"]==imageId);
        for(const auto& state:document["obj"][7]["imagebutton"]["states"])for(const auto& source:state){CHECK(source["imageId"]==imageId);CHECK(source["asset"]["projectImage"]==imageId);}
        CHECK(document["fonts"].size()==1);CHECK(document["obj"][0]["base"]["fontId"]==id);
        bool reference=false;for(const auto& style:document["obj"][0]["base"]["nativeStyles"])if(style["values"].contains("text_font")){CHECK(style["values"]["text_font"]["projectFont"]==id);reference=true;}CHECK(reference);
        auto restored=Project::FromProtobuf(Protobuf::Encode(EntityKind::Project,"project",document));CHECK(restored.images==images.ToJSON());CHECK(ObjectSerializer::SerializeObject(restored.objects[2])["img"]["asset"]==images.Get(imageId));CHECK(restored.fonts==library.ToJSON());CHECK(lv_obj_get_style_text_font(restored.objects[0],LV_PART_MAIN)->line_height==pointer->line_height);
        CHECK(lv_obj_get_style_text_font(restored.objects[0],LV_PART_MAIN)==lv_obj_get_style_text_font(restored.objects[1],LV_PART_MAIN));
        CHECK(ObjectSerializer::SerializeObject(restored.objects[7])["imagebutton"]["states"]==buttonRecord["imagebutton"]["states"]);
        for(auto* object:restored.objects)lv_obj_delete(object);
        // Saved conversion settings and friendly names are project metadata.
        // Exports derive their own symbols and share identical bitmap data.
        document["fonts"].push_back({{"id","font_alias"},{"name","Shared body alias"},{"font",library.ToJSON()[0]["font"]}});
        document["fonts"].push_back({{"id","font_compressed"},{"name","Pressed accent"},{"font",TinyCompressedFont()}});
        document["obj"][0]["base"]["nativeStyles"].push_back({{"selector",LV_STATE_PRESSED},{"values",{{"text_font",{{"projectFont","font_compressed"}}}}}});
        document["obj"][1]["base"]["nativeStyles"].push_back({{"selector",LV_STATE_CHECKED},{"values",{{"text_font",{{"projectFont","font_alias"}}}}}});
        auto unusedFont=library.ToJSON()[0]["font"];unusedFont["lineHeight"]=unusedFont["lineHeight"].get<int>()+1;
        auto unusedBitmap=base64_decode(unusedFont["dsc"]["bitmap"]);unusedBitmap+="UNUSED_FONT_FLASH_SENTINEL_3721";
        unusedFont["dsc"]["bitmap"]=base64_encode(reinterpret_cast<const unsigned char*>(unusedBitmap.data()),unusedBitmap.size());
        document["fonts"].push_back({{"id","font_unused"},{"name",u8"Unused \u03a9 font"},{"font",unusedFont}});
        const auto unusedStyle=json::array({{{"selector",0},{"values",{{"text_font",{{"projectFont","font_unused"}}}}}}});
        document["sharedStyles"]=unusedStyle;document["components"]=json::array({{{"records",json::array({{{"base",{{"nativeStyles",unusedStyle}}}}})}}});
        const auto device=ProjectScreens::Device(document);CHECK(document["fonts"][0].contains("conversion"));CHECK(!device["fonts"][0].contains("conversion"));
        CHECK(document["fonts"].size()==4);CHECK(device["fonts"].size()==3);CHECK(!device.contains("components"));CHECK(!device.contains("sharedStyles"));
        CHECK(Protobuf::Decode(Protobuf::Encode(EntityKind::Project,"project",document)).data["fonts"].size()==4);
        CHECK(ProjectScreens::Canonical(document)["fonts"][0].contains("conversion"));CHECK(device["fonts"][0]["font"]==document["fonts"][0]["font"]);
        {auto bytes=Protobuf::Encode(EntityKind::Project,"project",device);CHECK(Protobuf::Decode(bytes).data["fonts"]==device["fonts"]);
            std::ofstream binary(out/"runtime.pb",std::ios::binary);binary.write((const char*)bytes.data(),bytes.size());}
#if LVSERIAL_USE_XML
        {XmlCodeGenerator xml;CHECK(xml.LoadProject(document));CHECK(xml.ExportProject((out/"runtime.xml").u8string()));
            std::ifstream file(out/"runtime.xml",std::ios::binary);const std::string text((std::istreambuf_iterator<char>(file)),{});
            CHECK(text.find("private/source/location")==std::string::npos);CHECK(Xml::Decode(text.data(),text.size()).data["fonts"]==device["fonts"]);}
#endif
        CCodeGenerator generator;CHECK(generator.LoadProject(document));CHECK(generator.ExportProject(out.u8string()));
        json summary;CHECK(generator.Preflight(summary));
        CHECK(summary["manifest"]["fonts"].size()==3);CHECK(summary["manifest"]["fonts"][0]["symbol"]==summary["manifest"]["fonts"][1]["symbol"]);
        for(const auto& font:summary["manifest"]["fonts"])CHECK(font["id"]!="font_unused");
        CHECK(summary["requirements"].get<std::string>().find("LV_USE_FONT_COMPRESSED")!=std::string::npos);
        CHECK(summary["requirements"].get<std::string>().find("LV_FONT_FMT_TXT_LARGE")==std::string::npos);
        for(const auto& entry:std::filesystem::directory_iterator(out))if(entry.path().extension()==".c"||entry.path().extension()==".h"||entry.path().filename()=="manifest.json") {
            std::ifstream file(entry.path(),std::ios::binary);const std::string text((std::istreambuf_iterator<char>(file)),{});
            CHECK(text.find("private/source/location")==std::string::npos);CHECK(text.find("obsolete_output_symbol")==std::string::npos);CHECK(text.find("private_font_include.h")==std::string::npos);
            CHECK(text.find("font_unused")==std::string::npos);
        }
        if(!fixture) {
            auto large=document;auto& font=large["fonts"][2]["font"];font["lineHeight"]=300;
            font["dsc"]["GlyphDsc"][1]["box_w"]=256;font["dsc"]["GlyphDsc"][1]["adv_w"]=4096;
            const std::string wideBitmap(32,char(255));font["dsc"]["bpp"]=1;font["dsc"]["bitmapFmt"]=0;
            font["dsc"]["bitmap"]=base64_encode(reinterpret_cast<const unsigned char*>(wideBitmap.data()),wideBitmap.size());
            for(const auto& target:{"8.3","9.1","9.2","9.3","9.5","9.6"}) {
                CCodeGenerator exportFont;exportFont.SetTargetVersion(target);CHECK(exportFont.LoadProject(large));CHECK(exportFont.Preflight(summary));
                const auto requirements=summary["requirements"].get<std::string>();CHECK(requirements.find("#if !LV_FONT_FMT_TXT_LARGE")!=std::string::npos);
                CHECK(requirements.find("LV_USE_FONT_FMT_TXT_LARGE")==std::string::npos);
                CHECK(requirements.find("LV_USE_FONT_COMPRESSED")==std::string::npos);
            }
            const std::vector<std::string> colors={"ARGB8888","XRGB8888","RGB888","RGB565","RGB565_SWAPPED","RGB565A8","ARGB8565","ARGB8888_PREMULTIPLIED","L8","AL88","A1","A2","A4","A8","I1","I2","I4","I8","AUTO","RAW","RAW_ALPHA"};
            for(const auto& color:colors) {std::cerr<<"Converting "<<color<<'\n';auto converted=Converted(png,{{"colorFormat",color}},out/color);CHECK(converted.assets.size()==1);CHECK(!converted.assets[0]["deviceFrames"].empty());CHECK(!ImageAsset::Decode(converted.assets[0]).pixels.empty());CHECK(!converted.files.empty());}
            CHECK(animation.assets[0]["deviceFrames"].size()==2);
            auto embedded=ImageConversion::Convert({},{{"colorFormat","I4"}},out/"embedded",nullptr,json::array({lz4.assets[0]}));CHECK(embedded.success);
            Converted(png,{{"colorFormat","I8"},{"nemaGfx",true},{"premultiply",true},{"format","BIN"}},out/"nema");
            Converted(png,{{"colorFormat","RGB565"},{"bigEndian",true},{"includeAlpha",false},{"background",0xff00ff},{"format","PNG"}},out/"swapped");
            auto svg=out/"sample.svg";{std::ofstream stream(svg);stream<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"12\" height=\"8\"><rect width=\"12\" height=\"8\" fill=\"red\"/></svg>";}
            CHECK(Converted(svg,{{"chromaKey",true},{"chromaColor",0xff0000}}).assets[0]["deviceFrames"][0]["w"]==12);
            Converted(root/"Tests/Fixtures/Images/sample.jpg",{{"format","BIN"}},out/"jpeg");
            CHECK(!ImageConversion::Convert({png},{{"colorFormat","RAW"},{"compression","RLE"}}).success);
            CHECK(!ImageConversion::Convert({png},{{"colorFormat","L8"},{"bigEndian",true}}).success);
            CHECK(!ImageConversion::Convert({png},{{"align",0}}).success);
            auto badImage=document;badImage["images"][0]["id"]="missing";bool rejectedImage=false;try{Project::FromJSON(badImage);}catch(const std::exception&){rejectedImage=true;}CHECK(rejectedImage);
            auto bad=document;bad["fonts"][0]["id"]="missing";bool rejected=false;try{Project::FromJSON(bad);}catch(const std::exception&){rejected=true;}CHECK(rejected);
        }
        for(auto* object:objects)lv_obj_delete(object);lv_display_delete(display);lv_deinit();std::cout<<"Project font library and image conversion passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
