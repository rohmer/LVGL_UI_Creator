#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace BuiltinFonts {
struct Entry {
    std::string id;
    std::string name;
    std::filesystem::path path;
};
// Source TTFs are desktop resources. Merely enumerating them does not prepare
// a font or add any data to the current project or its device exports.
const std::vector<Entry>& Catalog();
const Entry* Find(const std::string& id);
}
