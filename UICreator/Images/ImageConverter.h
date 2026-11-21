#pragma once
#include <Serialization/ImageAsset.h>
#include <atomic>
namespace ImageConversion {
struct Result { bool success=false;std::string message;json assets=json::array();std::vector<std::filesystem::path> files; };
json Defaults();
Result Convert(const std::vector<std::filesystem::path>& sources,const json& options,const std::filesystem::path& output={},const std::atomic<bool>* cancel=nullptr,const json& embedded=json::array());
void Apply(lv_obj_t* image,const json& asset);
}
