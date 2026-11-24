#include "ImageConverter.h"
#include "../Utils/ApplicationPaths.h"
#include "../Fonts/FontConverter.h"
#include <Serialization/WidgetRecords.h>
#include <Serialization/Base64.h>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace ImageConversion {
json Defaults() {return {{"colorFormat","ARGB8888"},{"format","C"},{"compression","NONE"},{"align",1},{"premultiply",false},{"dither",true},{"background",0},{"includeAlpha",true},{"chromaKey",false},{"chromaColor",0x00ff00},{"bigEndian",false},{"nemaGfx",false},{"name",""},{"verbose",false}};}
namespace {
std::string Read(const std::filesystem::path& path) {std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read converter result");return {(std::istreambuf_iterator<char>(in)),{}};}
std::filesystem::path Directory() {
    return ApplicationPaths::ResourceDirectory();
}
}
Result Convert(const std::vector<std::filesystem::path>& sources,const json& options,const std::filesystem::path& output,const std::atomic<bool>* cancel,const json& embedded) {
    Result result;std::filesystem::path job;bool owns=false;
    try {
        if(sources.empty()&&embedded.empty())throw std::invalid_argument("Choose image files or an input folder");
        auto config=Defaults();config.update(options);
        auto request=json{{"sources",json::array()},{"options",config}};
        for(const auto& path:sources){if(!std::filesystem::exists(path))throw std::invalid_argument("Image input not found: "+path.u8string());request["sources"].push_back(std::filesystem::absolute(path).u8string());}
        if(cancel&&cancel->load())throw std::runtime_error("Conversion cancelled");
        static std::atomic<uint64_t> sequence{0};
        job=std::filesystem::temp_directory_path()/("uicreator-image-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++));
        owns=std::filesystem::create_directory(job);if(!owns)throw std::runtime_error("Cannot create image conversion workspace");
        size_t index=0;for(const auto& asset:embedded) {
            auto folder=job/("source-"+std::to_string(index++));std::filesystem::create_directory(folder);
            auto file=folder/std::filesystem::u8path(asset.at("name").get<std::string>()).filename();
            auto bytes=base64_decode(asset.at("sourceData").get<std::string>());std::ofstream stream(file,std::ios::binary);stream.write(bytes.data(),bytes.size());stream.close();if(!stream)throw std::runtime_error("Cannot restore embedded image source");request["sources"].push_back(file.u8string());
        }
        {std::ofstream file(job/"request.json",std::ios::binary);file<<request.dump();if(!file)throw std::runtime_error("Cannot write conversion request");}
        auto runtime=Directory()/"image-converter"/
#ifdef _WIN32
            "UICreatorImageTool.exe";
#else
            "UICreatorImageTool";
#endif
        if(!std::filesystem::exists(runtime))throw std::runtime_error("Bundled image converter is missing. Rebuild the application assets.");
        if(FontConversion::RunTool(runtime,"--request",job/"request.json",job/"log.txt",cancel)!=0)throw std::runtime_error(Read(job/"log.txt"));
        auto manifest=json::parse(Read(job/"result.json"));result.assets=manifest.at("assets");
        if(!output.empty()) {
            const auto root=std::filesystem::absolute(output).lexically_normal();std::filesystem::create_directories(root);
            for(const auto& file:manifest.at("files")) {
                const auto name=std::filesystem::u8path(file.at("name").get<std::string>());if(name.has_parent_path()||name.is_absolute())throw std::runtime_error("Invalid image output name");
                auto destination=root/name;for(const auto& source:sources)if(std::filesystem::exists(destination)&&std::filesystem::equivalent(source,destination))throw std::runtime_error("Image output would overwrite an input");
                auto bytes=Read(job/file.at("staged").get<std::string>());std::ofstream out(destination,std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());out.close();if(!out)throw std::runtime_error("Cannot write converted image");result.files.push_back(destination);
            }
        }
        result.success=true;result.message="Converted "+std::to_string(result.assets.size())+" image(s). Settings and converted data can be applied to an image widget.";
        if(config.value("verbose",false)){auto log=Read(job/"log.txt");if(log.size()>4000)log.resize(4000);if(!log.empty())result.message+='\n'+log;}
    }catch(const std::exception& error){result.message=error.what();}
    if(owns&&job.parent_path()==std::filesystem::temp_directory_path()&&job.filename().u8string().rfind("uicreator-image-",0)==0){std::error_code ignored;std::filesystem::remove_all(job,ignored);}
    return result;
}
void Apply(lv_obj_t* image,const json& asset) {if(!image||lv_obj_get_class(image)!=&lv_image_class)throw std::invalid_argument("Select an Image widget first");Serialization::ImageAsset::Restore(image,asset);}
}
