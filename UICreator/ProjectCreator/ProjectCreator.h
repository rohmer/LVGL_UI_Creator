#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <atomic>
#include <filesystem>
#include <string>
namespace ProjectCreation {
using json=nlohmann::json;
struct Result {bool success=false;std::string message;json data;};
std::filesystem::path RuntimeDirectory();
std::filesystem::path DefaultCache();
json BundledCatalog();
Result Run(json request,const std::atomic<bool>* cancel=nullptr);
bool OpenFolder(const std::filesystem::path& folder,bool vscode=false);
bool OpenUrl(const std::string& url);
}
