#pragma once
#include "../Utils/EditorEdit.h"
#include <Serialization/ProjectTries.h>
#include <Serialization/AdvancedKeyboard.h>
#include <filesystem>
#include <fstream>

class TrieLibrary {
    json records=json::array();
    Serialization::ProjectTries::CatalogPtr catalog;
public:
    const json& ToJSON() const { return records; }
    void Load(const json& input) {
        auto normalized=Serialization::ProjectTries::Normalize(input);
        json document={{"tries",normalized}};
        auto next=Serialization::ProjectTries::Load(document);
        records=std::move(normalized);catalog=std::move(next);
    }
    std::string Import(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file)throw std::runtime_error("Cannot open trie file");
        const auto length=file.tellg();
        if(length<=0 || uint64_t(length)>16u*1024u*1024u)throw std::runtime_error("Choose a trie between 1 byte and 16 MiB");
        std::vector<uint8_t> bytes(static_cast<size_t>(length));file.seekg(0);
        file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(!file)throw std::runtime_error("Cannot read the complete trie");
        std::string selected;
        auto next=Serialization::ProjectTries::Import(records,path.stem().u8string(),bytes.data(),bytes.size(),&selected);
        Load(next);return selected;
    }
    void Apply(lv_obj_t* keyboard,const std::string& id) {
        EditorEdit::Scope edit;
        Serialization::ProjectTries::Scope scope(catalog);
        Serialization::AdvancedKeyboard::Apply(keyboard,{{"trieId",id}});
    }
    void ApplySettings(lv_obj_t* keyboard,const json& settings) {
        Serialization::ProjectTries::Scope scope(catalog);
        Serialization::AdvancedKeyboard::Apply(keyboard,settings);
    }
    void Rebind(lv_obj_t* root) {
        if(Serialization::AdvancedKeyboard::Type(root)) {
            const auto settings=Serialization::AdvancedKeyboard::ToJSON(root).at("advancedKeyboard");
            Serialization::ProjectTries::Scope scope(catalog);
            Serialization::ProjectTries::Attach(root,settings.value("trieId",std::string("builtin-en-us")),
                settings.value("autocorrect",true),settings.value("autocorrectThreshold",0.3));
        }
        for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)Rebind(lv_obj_get_child(root,i));
    }
};
