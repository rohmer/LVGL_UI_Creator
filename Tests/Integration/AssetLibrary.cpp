#include "ImageConverter.h"
#include "../../UICreator/Fonts/FontLibrary.h"
#include "../../UICreator/Images/ImageLibrary.h"
#include <Serialization/Project.h>
#include <Serialization/ObjectSerializer.h>
#include "../../CodeGeneration/CCodeGenerator.h"
#include <fstream>
#include <iostream>
#define CHECK(condition) do {if(!(condition))throw std::runtime_error(std::string("Check failed line ")+std::to_string(__LINE__)+": " #condition);}while(0)
using namespace Serialization;
ImageConversion::Result Converted(const std::filesystem::path& path,const json& options,const std::filesystem::path& output={}) {auto result=ImageConversion::Convert({path},options,output);if(!result.success)throw std::runtime_error(result.message);return result;}
int main(int argc,char** argv) {
    try {
        const bool fixture=argc==4&&std::string(argv[1])=="--fixture";if(!fixture&&argc!=3)return 2;
        const auto root=std::filesystem::path(argv[fixture?2:1]),out=std::filesystem::path(argv[fixture?3:2]);std::filesystem::create_directories(out);
        const auto png=root/"Tests/Fixtures/Images/sample.png",gif=root/"Tests/Fixtures/Images/sample.gif";
        lv_init();auto* display=lv_display_create(800,600);
        FontLibrary library;auto id=library.Add("Body",LVFont::ToJSON(*LV_FONT_DEFAULT));auto* first=ObjectSerializer::Create("label"),*second=ObjectSerializer::Create("label");
        library.Apply(first,id);library.Apply(second,id);CHECK(library.Get(id)==lv_obj_get_style_text_font(first,LV_PART_MAIN));
        auto* pointer=library.Get(id);library.Rename(id,"Body renamed");CHECK(library.Get(id)==pointer);CHECK(library.Usage(lv_screen_active(),id)==2);
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
        {auto bytes=Protobuf::Encode(EntityKind::Project,"project",document);std::ofstream binary(out/"runtime.pb",std::ios::binary);binary.write((const char*)bytes.data(),bytes.size());}
        CCodeGenerator generator;CHECK(generator.LoadProject(document));CHECK(generator.ExportProject(out.u8string()));
        if(!fixture) {
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
