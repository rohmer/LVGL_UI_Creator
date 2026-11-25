#include "BuiltinFonts.h"
#include "../Utils/ApplicationPaths.h"

namespace BuiltinFonts {
namespace {
struct Definition { const char *id, *name, *filename; };
constexpr Definition definitions[] = {
#include "BuiltinFontData.inc"
};
}

const std::vector<Entry>& Catalog() {
    static const auto catalog=[] {
        // Resolve only deployed resources, so a source checkout or the user's
        // temporary import directory cannot mask missing package assets.
        const auto folder=ApplicationPaths::ResourceDirectory()/"fonts"/"builtin";
        std::vector<Entry> result;
        result.reserve(sizeof(definitions)/sizeof(*definitions));
        for(const auto& definition:definitions)
            result.push_back({definition.id,definition.name,folder/definition.filename});
        return result;
    }();
    return catalog;
}

const Entry* Find(const std::string& id) {
    for(const auto& entry:Catalog())if(entry.id==id)return &entry;
    return nullptr;
}
}
