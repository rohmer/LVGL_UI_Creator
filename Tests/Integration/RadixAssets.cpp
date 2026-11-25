#include "AssetFixtures.h"
#include <Serialization/ProjectTries.h>
#include <Serialization/AdvancedKeyboard.h>
#include <Serialization/Project.h>
#include <Serialization/Xml.h>
#include <lv_advanced_keyboard_prediction.h>
#include "../../Runtime/include/lvgl_runtime.h"
#include "../../CodeGeneration/CCodeGenerator.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Serialization;
namespace {
void Check(bool good,const char* message){if(!good)throw std::runtime_error(message);}
void Predict(lv_obj_t* keyboard,uint32_t expectedMass=0) {
    auto* predictor=lv_advanced_keyboard_get_predictor(keyboard);
    Check(predictor&&lv_advanced_keyboard_predictor_node_count(predictor)==12,"Embedded model missing or logical node count changed");
    lv_advanced_keyboard_suggestion_t words[3]{};
    const auto count=lv_advanced_keyboard_predictor_suggest_folded(predictor,"h",1,nullptr,0,nullptr,0,words,3);
    Check(count==1&&words[0].length==5&&!std::memcmp(words[0].text,"hello",5),"Embedded fixed/radix prediction changed");
    if(expectedMass)Check(words[0].mass==expectedMass,"Public C mass was truncated before the facade");
}
void Loaded(const void* bytes,size_t size,bool borrow,uint32_t expectedMass) {
    lvgl_runtime_t* runtime=nullptr;char error[512]{};
    Check(borrow?lvgl_runtime_load_memory_borrowed_assets(bytes,size,nullptr,&runtime,error,sizeof error):
                 lvgl_runtime_load_memory(bytes,size,nullptr,&runtime,error,sizeof error),error);
    Predict(lvgl_runtime_object(runtime,0,7),expectedMass);lvgl_runtime_destroy(runtime);
}
void Case(const char* name,const std::vector<uint8_t>& model,const std::filesystem::path& output) {
    std::string id;
    const auto tries=ProjectTries::Import(json::array(),std::string("C:/private/")+name+".ngtr",model.data(),model.size(),&id);
    Check(tries.size()==1&&tries[0].at("name")==std::string(name)+".ngtr","Import retained an external path");
    const bool fixed=model[4]==2;
    const auto expectedMass=fixed?19u:(model[17]&2)?0u:UINT32_MAX;
    Check(fixed?tries[0].at("indexData")=="":!tries[0].at("indexData").get<std::string>().empty(),"Fixed/radix sidecar handling changed");
    Check(ProjectTries::Normalize(tries)==tries,"Normalized trie catalog was not stable");
    json document={{"formatVersion",2},{"runtimeTarget","9.6"},{"lvglVersion","9.6.0"},
        {"canvas",{{"width",640},{"height",400}}},{"tries",tries},
        {"obj",json::array({{{"oid",7},{"par",-1},{"base",json::object()},
            {"advancedKeyboard",{{"language","fr-FR"},{"trieId",id},{"autoFocus",false}}}}})}};
    const auto binary=Protobuf::Encode(EntityKind::Project,"project",document);
    auto decoded=Protobuf::Decode(binary);
    Check(decoded.data["tries"][0]["data"].is_binary()&&decoded.data["tries"][0]["indexData"].is_binary(),"Trie wire data stopped being typed binary");
    Check(ProjectTries::Normalize(decoded.data["tries"])==tries,"Protobuf changed model or index bytes");
    auto borrowed=Protobuf::DecodeBorrowedAssets(binary.data(),binary.size());
    auto catalog=ProjectTries::Load(borrowed.data,borrowed.borrowedAssets);
    auto* keyboard=lv_advanced_keyboard_create(lv_screen_active());
    {ProjectTries::Scope scope(catalog);ProjectTries::Attach(keyboard,id);}
    catalog.reset();Predict(keyboard,expectedMass);lv_obj_delete(keyboard);
    Loaded(binary.data(),binary.size(),false,expectedMass);Loaded(binary.data(),binary.size(),true,expectedMass);
    auto temporary=binary;lvgl_runtime_t* runtime=nullptr;char error[512]{};
    Check(lvgl_runtime_load_memory(temporary.data(),temporary.size(),nullptr,&runtime,error,sizeof error),error);
    std::fill(temporary.begin(),temporary.end(),0xA5);temporary.clear();temporary.shrink_to_fit();
    Predict(lvgl_runtime_object(runtime,0,7),expectedMass);lvgl_runtime_destroy(runtime);
    const auto xml=Xml::Encode(EntityKind::Project,"project",document);
    Check(ProjectTries::Normalize(Xml::Decode(xml).data.at("tries"))==tries,"XML changed model/index bytes");
    Loaded(xml.data(),xml.size(),false,expectedMass);
    CCodeGenerator generator;Check(generator.LoadProject(document),"Cannot load radix C export project");
    const auto directory=output/name;std::filesystem::create_directories(directory);
    Check(generator.ExportProject(directory.u8string()),"Cannot export radix C project");
    Check(!std::filesystem::exists(directory/"advanced_keyboard/RadixModelBuilder.cpp")&&
          !std::filesystem::exists(directory/"advanced_keyboard/RadixModelBuilder.h"),"Offline encoder leaked into firmware export");
    {std::ofstream file(directory/"project.pb",std::ios::binary);file.write(reinterpret_cast<const char*>(binary.data()),binary.size());}
    {std::ofstream file(directory/"project.xml",std::ios::binary);file.write(xml.data(),xml.size());}
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)return 2;
        lv_init();auto* display=lv_display_create(640,400);Check(display,"Cannot create display");
        const auto output=std::filesystem::u8path(argv[1]);
        Case("fixed-v2",RadixFixtures::Fixed(),output);
        Case("radix-v4",RadixFixtures::Radix(false),output);
        Case("scored-v4",RadixFixtures::Radix(true),output);
        lv_display_delete(display);lv_deinit();
        std::cout<<"NGTR v2/v4/scored-v4 import, typed PB/XML, zero-sidecar borrowing, input lifetime, prediction and lean C export passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
