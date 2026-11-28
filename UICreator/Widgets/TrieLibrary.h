#pragma once
#include "../Utils/EditorEdit.h"
#include "../KeyboardModels/BundledTries.h"
#include <Serialization/ProjectTries.h>
#include <Serialization/AdvancedKeyboard.h>
#include <filesystem>
#include <fstream>
#include <map>

class TrieLibrary {
    json records=json::array();
    Serialization::ProjectTries::CatalogPtr catalog;
    std::map<std::string,std::string> bundledIds;
public:
    const json& ToJSON() const { return records; }
    void Load(const json& input) {
        auto normalized=Serialization::ProjectTries::Normalize(input);
        json document={{"tries",normalized}};
        auto next=Serialization::ProjectTries::Load(document);
        records=std::move(normalized);catalog=std::move(next);bundledIds.clear();
    }
    std::string Import(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file)throw std::runtime_error("Cannot open trie file");
        const auto length=file.tellg();
        if(length<=0 || uint64_t(length)>16u*1024u*1024u)throw std::runtime_error("Choose a trie between 1 byte and 16 MiB");
        std::vector<uint8_t> bytes(static_cast<size_t>(length));file.seekg(0);
        file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(!file)throw std::runtime_error("Cannot read the complete trie");
        return ImportBytes(path.stem().u8string(),bytes);
    }
    std::string ImportBytes(const std::string& name,const std::vector<uint8_t>& bytes) {
        std::string selected;
        auto next=Serialization::ProjectTries::Import(records,name,bytes.data(),bytes.size(),&selected);
        auto known=bundledIds;Load(next);bundledIds=std::move(known);return selected;
    }
    // Project settings select assets without constructing a keyboard in Design.
    json BundledSettings(const std::string& language,uint32_t sizeKiB) {
        const auto* model=BundledTries::Find(language,sizeKiB);
        if(!model)throw std::invalid_argument("No bundled trie for this language and size");
        auto cached=bundledIds.find(model->id);
        std::string id;
        if(cached!=bundledIds.end())id=cached->second;
        else {
            id=ImportBytes(model->name,BundledTries::Read(*model));
            bundledIds[model->id]=id;
        }
        return {{"language",language},{"trieId",id},{"trieSizeKiB",sizeKiB}};
    }
    json LanguageSettings(const json& previous,const std::string& language) {
        if(previous.at("language")==language)return json::object();
        const auto id=previous.value("trieId",std::string{});
        const auto tier=previous.value("trieSizeKiB",0u);
        const bool rtl=language=="ar-SA"||language=="ur-PK";
        if(!id.empty()&&id!="builtin-en-us"&&!tier)
            return {{"language",language},{"rightToLeft",rtl}};
        const auto* matching=BundledTries::Find(language,tier?tier:16u);
        if(!matching)matching=BundledTries::Find(language,16);
        if(matching) {
            auto patch=BundledSettings(language,matching->sizeKiB);
            patch["rightToLeft"]=rtl;
            return patch;
        }
        return {{"language",language},{"rightToLeft",rtl},{"trieId",""},{"trieSizeKiB",0}};
    }
    void Apply(lv_obj_t* keyboard,const std::string& id) {
        EditorEdit::Scope edit;
        Serialization::ProjectTries::Scope scope(catalog);
        Serialization::AdvancedKeyboard::Apply(keyboard,{{"trieId",id},{"trieSizeKiB",0}});
    }
    void ApplySettings(lv_obj_t* keyboard,const json& settings) {
        Serialization::ProjectTries::Scope scope(catalog);
        Serialization::AdvancedKeyboard::Apply(keyboard,settings);
    }
    void ApplyBundled(lv_obj_t* keyboard,const std::string& language,uint32_t sizeKiB) {
        const auto* model=BundledTries::Find(language,sizeKiB);
        if(!model)throw std::invalid_argument("No bundled trie for this language and size");
        EditorEdit::Scope edit;
        auto cached=bundledIds.find(model->id);
        auto apply=[&](const std::string& id) {
            ApplySettings(keyboard,{{"language",language},{"trieId",id},{"trieSizeKiB",sizeKiB}});
        };
        if(cached!=bundledIds.end()) {apply(cached->second);return;}
        const auto before=records;
        const auto id=ImportBytes(model->name,BundledTries::Read(*model));bundledIds[model->id]=id;
        try {apply(id);} catch(...) {Load(before);throw;}
    }
    void ApplyLanguage(lv_obj_t* keyboard,const std::string& language) {
        const auto previous=Serialization::AdvancedKeyboard::ToJSON(keyboard).at("advancedKeyboard");
        if(previous.at("language")==language)return;
        const auto id=previous.value("trieId",std::string{});
        const auto tier=previous.value("trieSizeKiB",0u);
        if(!id.empty()&&id!="builtin-en-us"&&!tier) {
            EditorEdit::Scope edit;
            ApplySettings(keyboard,{{"language",language},{"rightToLeft",language=="ar-SA"||language=="ur-PK"}});
            return;
        }
        const auto* matching=BundledTries::Find(language,tier?tier:16u);
        if(!matching)matching=BundledTries::Find(language,16);
        if(matching) {ApplyBundled(keyboard,language,matching->sizeKiB);return;}
        EditorEdit::Scope edit;
        ApplySettings(keyboard,{{"language",language},{"rightToLeft",language=="ar-SA"||language=="ur-PK"},
            {"trieId",""},{"trieSizeKiB",0}});
    }
    void ApplyDefault(lv_obj_t* object) {
        if(!Serialization::AdvancedKeyboard::Type(object))return;
        const auto language=Serialization::AdvancedKeyboard::ToJSON(object).at("advancedKeyboard").value("language",std::string("en-US"));
        if(BundledTries::Find(language,16))ApplyBundled(object,language,16);
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
