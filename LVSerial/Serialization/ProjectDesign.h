#pragma once

// Authoring data is materialized into ordinary LVGL records. This header has no
// LVGL or desktop dependencies, so the same document works in trimmed runtimes.
#include "../../3rdParty/JSON/json.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>

namespace Serialization { namespace ProjectDesign {
using json = nlohmann::json;

inline const json& Presets()
{
    static const json themes = [] {
        json all = json::array();
        auto add = [&](const char* id, const char* name, const char* description, bool dark,
            unsigned bg, unsigned surface, unsigned text, unsigned muted, unsigned primary,
            unsigned accent, unsigned success, unsigned warning, unsigned error, unsigned border,
            unsigned onPrimary, int radius, int spacing) {
            all.push_back({{"id",id},{"name",name},{"description",description},{"dark",dark},
                {"tokens",{{"background",bg},{"surface",surface},{"text",text},{"muted",muted},
                    {"primary",primary},{"accent",accent},{"success",success},{"warning",warning},
                    {"error",error},{"border",border},{"onPrimary",onPrimary},{"radius",radius},{"spacing",spacing}}}});
        };
        add("slate-light","Porcelain","Quiet graphite on cool porcelain",false,0xf3f5f8,0xffffff,0x182334,0x64748b,0x334155,0x6473c5,0x18794e,0xa56308,0xc53846,0xd7dee8,0xffffff,8,10);
        add("slate-dark","Midnight Slate","Crisp blue accents, layered charcoal",true,0x111827,0x1e293b,0xf1f5f9,0xa7b6cb,0x93b4ff,0xc4b5fd,0x5cdb9c,0xf6c760,0xff8990,0x3b4b63,0x142340,8,10);
        add("ocean-light","Ocean Mist","Airy marine blue with sea-glass green",false,0xedf6fb,0xffffff,0x16354a,0x527387,0x036caa,0x0e928b,0x15835b,0xa86b08,0xc33a4e,0xc5deeb,0xffffff,10,12);
        add("ocean-dark","Deep Ocean","Inky navy, luminous cyan and turquoise",true,0x071d2b,0x102f40,0xe6f4fa,0x93b8cd,0x58c9f4,0x5ee1c2,0x66dcaa,0xf5c467,0xff929b,0x285365,0x062737,10,12);
        add("forest-light","Sage Studio","Soft sage and warm white with pine accents",false,0xf1f5ef,0xfdfefb,0x233c2b,0x637a67,0x2e704d,0x927241,0x257449,0x9b690d,0xbb4144,0xd1dece,0xffffff,12,12);
        add("forest-dark","Forest Night","Evergreen surfaces and mint highlights",true,0x101f19,0x1b3228,0xeaf3e9,0xa0b9a7,0x87d4a0,0xd5bb81,0x81d6a1,0xecc76c,0xf29996,0x375445,0x163323,12,12);
        add("amber-light","Warm Sand","Linen, dark umber and confident amber",false,0xf8f3e9,0xfffdf8,0x413425,0x83715a,0xa85c08,0x147c83,0x3c7b45,0xa45a03,0xbc4037,0xe4d8c3,0xffffff,7,10);
        add("amber-dark","Amber Atelier","Espresso and warm gold for rich dashboards",true,0x251e18,0x372c22,0xf8eddb,0xc4ae91,0xf1be67,0x8bd1cf,0x9bd59d,0xf4cb75,0xf4a49a,0x5a4836,0x3d290a,7,10);
        add("violet-light","Lavender Paper","Lavender haze with precise violet controls",false,0xf4f1fa,0xffffff,0x302347,0x7a6b91,0x7050ba,0x157f92,0x288360,0xa66a0b,0xbe3e61,0xdfd5ed,0xffffff,10,12);
        add("violet-dark","Amethyst","Deep plum with lilac and ice-blue details",true,0x1e182d,0x302640,0xf2eafa,0xb9a9d0,0xc3a7ff,0x91d8ee,0x87d8ba,0xedcb7c,0xffa0bc,0x514165,0x2d194c,10,12);
        add("rose-light","Rose Quartz","Subtle blush, berry actions and stone neutrals",false,0xfbf2f3,0xfffcfc,0x482b35,0x896a76,0xa93f63,0x537c8b,0x358167,0xa56e14,0xc63c4f,0xead4da,0xffffff,12,11);
        add("rose-dark","Velvet Rose","Burgundy surfaces with soft rose highlights",true,0x291920,0x402631,0xffedf3,0xcba7b6,0xf3a3bc,0x9dcbd7,0x93d6b6,0xeec780,0xffa0a5,0x63404f,0x4a1c30,12,11);
        add("mono-light","Architect","Sharp monochrome, generous space, electric blue",false,0xf4f4f4,0xffffff,0x161616,0x686868,0x154acb,0x6841c1,0x187547,0x9b6607,0xc33440,0xd4d4d4,0xffffff,2,14);
        add("mono-dark","Carbon","Restrained black, silver controls and cyan focus",true,0x101112,0x222426,0xf5f6f7,0xa9adb3,0xd8e0e9,0x75d5e5,0x85d8aa,0xeac76a,0xffa0a5,0x44494f,0x1a222c,2,14);
        add("contrast-light","Signal Paper","Deep blue actions and strong outlines on pure white",false,0xffffff,0xffffff,0x0b1020,0x344054,0x003b8e,0x006b61,0x006b3c,0x765000,0xa10f2b,0x374151,0xffffff,2,12);
        add("contrast-dark","Signal Night","True black, bright yellow actions and crisp cyan",true,0x000000,0x0b0b0b,0xffffff,0xd9e2f2,0xffd54a,0x6fe7ff,0x85f7ad,0xffe18a,0xff9c9c,0xc4cdd8,0x000000,2,12);
        add("blueprint-light","Blueprint","Precise navy and teal with square technical controls",false,0xeef4fc,0xffffff,0x152c4b,0x435e7e,0x1d4d91,0x006b70,0x166947,0x8a5700,0xb12f4a,0x899fba,0xffffff,0,10);
        add("blueprint-dark","Blueprint Night","Technical navy with clear blue and turquoise accents",true,0x0a1528,0x13223c,0xe8f0ff,0xa8b8d0,0x8ab4ff,0x5fd6d0,0x6dd8a0,0xf2c76c,0xffa1ac,0x5a7192,0x0b1b32,0,10);
        add("copper-light","Kiln","Warm clay, ivory surfaces and copper actions",false,0xf6f0ea,0xfffcf8,0x3f2b22,0x765b4c,0x984421,0x287b82,0x2e714e,0x8a5b0a,0xb62f3c,0xc2ad9c,0xffffff,6,11);
        add("copper-dark","Copper Foundry","Dark earthen surfaces, soft copper and pale teal",true,0x211a19,0x342624,0xfaede7,0xcbb0a7,0xefa780,0x9fd8d1,0x96dbb0,0xf0cb80,0xffaaa3,0x7b5b52,0x2c1a11,6,11);
        add("arctic-light","Glacier","Clean ice-white and deep teal with indigo details",false,0xf0f8fa,0xffffff,0x123844,0x416977,0x006778,0x4f52a0,0x146d55,0x8c5c00,0xad3550,0x9fbfc7,0xffffff,5,13);
        add("arctic-dark","Polar Night","Deep teal surfaces with icy cyan and lavender",true,0x081e24,0x10313a,0xe7f7fb,0xa3c7d0,0x6fd6e5,0xb8b7ff,0x7ee0bc,0xf6d28a,0xffa3b5,0x527e8a,0x06313b,5,13);
        add("citrus-light","Citrus Grove","Lime-tinted paper, olive actions and warm orange",false,0xf6f7e9,0xfefff7,0x2f3618,0x626d3a,0x536816,0x9b4e1b,0x246c44,0x826000,0xb63239,0xb9c49c,0xffffff,14,12);
        add("citrus-dark","Citrus After Dark","Rounded olive surfaces with bright citrus highlights",true,0x1a2010,0x2a341b,0xf4f7e6,0xbec999,0xc9de72,0xf4b184,0x97deb0,0xf6d47d,0xffaba1,0x71824d,0x26320a,14,12);
        add("editorial-light","Editorial","Warm paper, burgundy actions and square editorial spacing",false,0xf6f4ef,0xfffefa,0x1f1e1b,0x666259,0x8c233b,0x28597c,0x2b6846,0x8b5b10,0xa82530,0xb8b1a4,0xffffff,0,16);
        add("editorial-dark","Edition Night","Warm charcoal, rose actions and calm blue details",true,0x191818,0x292725,0xf5f1e9,0xc6beb3,0xe9a7b8,0xa3c9e5,0x9ad0af,0xe6c483,0xf1a5a5,0x766c61,0x401524,0,16);
        add("paper-ink","Paper Ink","Sharp grayscale controls for a restrained paper aesthetic",false,0xf3f3f3,0xffffff,0x000000,0x454545,0x111111,0x666666,0x333333,0x555555,0x222222,0x777777,0xffffff,0,12);
        add("black-glass","Black Glass","True-black canvas, rounded blue actions and mint details",true,0x000000,0x11151c,0xf5f8ff,0xb0bdcf,0x91c4ff,0x9aeacb,0x91d9ac,0xeed58f,0xffadb9,0x526177,0x091a2b,16,12);
        add("vault-light","Vault","Indigo finance controls, crisp white cards and electric lime",false,0xf5f5fc,0xffffff,0x222240,0x625f78,0x4636ae,0xb4cf28,0x26644d,0x895c0e,0xb03452,0xc5c4de,0xffffff,14,12);
        add("vault-dark","Vault Night","Midnight indigo cards with bright lime actions and lilac details",true,0x121225,0x211f3a,0xf5f3ff,0xb5b0d0,0xc9f16b,0xaa9cff,0x9de0aa,0xefc975,0xffacbf,0x56516f,0x1f2a08,14,12);
        add("bloom-light","Bloom","Cream, terracotta and botanical accents for calm wellness screens",false,0xfaf4eb,0xfffcf6,0x443029,0x786157,0x9e4930,0x536e59,0x306647,0x89600d,0xb33241,0xdbc5b4,0xffffff,20,14);
        add("bloom-dark","Bloom Night","Cocoa surfaces, peach actions and soft botanical green",true,0x251b19,0x3a2824,0xfff2e6,0xcdb5a4,0xf0b593,0xb1c7a5,0xa2d7b1,0xefd08d,0xf5a7a9,0x76564b,0x3a2118,20,14);
        add("studio-light","Studio","Violet workspace controls with lively orange and airy lavender",false,0xf6f3ff,0xffffff,0x281844,0x6d5b82,0x7029c9,0xdb6528,0x1f7453,0x926005,0xb92d5a,0xd3c6e8,0xffffff,16,12);
        add("studio-dark","Studio Night","Ink-purple workspace, lilac actions and warm apricot highlights",true,0x1b102e,0x2c1c47,0xf8f0ff,0xc3add9,0xc6a2ff,0xffb277,0x95dfb9,0xf3ce88,0xffa2c4,0x695080,0x2b104c,16,12);
        add("estate-light","Estate","Stone-white cards, deep forest actions and brass details",false,0xf2f0e9,0xfffdf8,0x27362e,0x5b6b60,0x235b44,0x9c703c,0x2d6d45,0x8a5f11,0xb43840,0xc2c6b7,0xffffff,4,16);
        add("estate-dark","Estate Night","Layered evergreen, pale sage actions and understated brass",true,0x16231e,0x26392f,0xeef4e9,0xb4c4b5,0xb3d1ac,0xd9b786,0x9fd1ac,0xe7ca8b,0xf0b0a8,0x5e7563,0x1b3524,4,16);
        return all;
    }();
    return themes;
}
inline const json& Preset(const std::string& id)
{ for(const auto& p:Presets()) if(p.at("id")==id) return p; throw std::invalid_argument("Unknown project theme: "+id); }
inline json Tokens(const json& doc,const std::string& id)
{
    auto result=Preset(id).at("tokens");
    if(doc.contains("projectTheme")&&doc["projectTheme"].is_object()) {
        const auto& t=doc["projectTheme"];
        if(t.contains("overrides")&&t["overrides"].is_object()&&t["overrides"].contains(id)&&t["overrides"][id].is_object())
            for(auto i=t["overrides"][id].begin();i!=t["overrides"][id].end();++i) if(result.contains(i.key())&&i.value().is_number_integer())result[i.key()]=i.value();
    }
    return result;
}
inline std::string ThemeId(const json& doc)
{ return doc.contains("projectTheme")&&doc["projectTheme"].is_object()?doc["projectTheme"].value("id",std::string("slate-light")):"slate-light"; }
inline json& Style(json& record,unsigned selector=0)
{
    auto& styles=record["base"]["nativeStyles"];
    if(!styles.is_array())styles=json::array();
    for(auto& style:styles)if(style.is_object()&&style.value("selector",0u)==selector) {
        if(!style["values"].is_object())style["values"]=json::object();
        return style["values"];
    }
    styles.push_back({{"selector",selector},{"values",json::object()}});return styles.back()["values"];
}
inline void BindTheme(json& record,const std::string& preset,bool canvas=false)
{
    auto& theme=record["base"]["design"]["theme"];
    theme={{"preset",preset},{"bindings",json::object()},{"last",json::object()}};
    auto& b=theme["bindings"];
    b["text_color"]="text";
    const bool code=record.contains("barcode")||record.contains("qrcode");
    if(code)return; // Machine-readable widgets retain their specified contrast.
    const bool label=record.contains("text")||record.contains("img")||record.contains("line")||record.contains("led");
    const bool button=record.contains("button")||record.contains("imagebutton");
    if(!label) {
        b["bg_color"]=canvas?"background":button?"primary":"surface";
        b["bg_grad_color"]=canvas?"background":button?"primary":"surface";
        b["border_color"]="border";
        if(!canvas)b["radius"]="radius";
        if(!canvas&&!record.contains("advancedKeyboard"))for(const auto* padding:{"pad_left","pad_right","pad_top","pad_bottom","pad_row","pad_column"})b[padding]="spacing";
        if(button)b["text_color"]="onPrimary";
    }
    if(record.contains("advancedKeyboard")) {
        b["keyboard.keyBackground"]="primary";b["keyboard.keyForeground"]="onPrimary";b["keyboard.keyRadius"]="radius";
    }
    // Part selectors are stable across the supported LVGL 8/9 releases. Avoid
    // encoding state bits here, which differ between those releases.
    if(record.contains("bar")||record.contains("slider")||record.contains("arc")||record.contains("switch")||record.contains("cb")) {
        b["131072:bg_color"]="primary";b["131072:border_color"]="primary";
    }
    if(record.contains("slider")||record.contains("arc")||record.contains("switch"))b["196608:bg_color"]="accent";
    if(record.contains("dropdown")||record.contains("roller")||record.contains("textarea")||record.contains("textBox")) {
        b["262144:bg_color"]="primary";b["262144:text_color"]="onPrimary";
    }
}
inline void ResolveTheme(json& record,const json& doc)
{
    if(!record.contains("base")||!record["base"].is_object()||!record["base"].contains("design"))return;
    auto& design=record["base"]["design"];
    if(!design.is_object()||!design.contains("theme")||!design["theme"].is_object())return;
    auto& theme=design["theme"];
    std::string id=theme.value("preset",std::string("project"));if(id=="project")id=ThemeId(doc);
    const auto tokens=Tokens(doc,id);
    auto bindings=theme.value("bindings",json::object());if(!bindings.is_object())return;
    auto& last=theme["last"];if(!last.is_object())last=json::object();
    for(auto entry=bindings.begin();entry!=bindings.end();++entry) {
        if(!entry.value().is_string()||!tokens.contains(entry.value().get<std::string>()))continue;
        const std::string property=entry.key();const auto value=tokens.at(entry.value().get<std::string>());
        const bool color=property.find("color")!=std::string::npos||property=="keyboard.keyBackground"||property=="keyboard.keyForeground";
        json* target=nullptr;
        if(property.rfind("keyboard.",0)==0)target=&record["advancedKeyboard"][property.substr(9)];
        else {
            const auto colon=property.find(':');
            const unsigned selector=colon==std::string::npos?0:static_cast<unsigned>(std::stoul(property.substr(0,colon)));
            target=&Style(record,selector)[colon==std::string::npos?property:property.substr(colon+1)];
        }
        // A property changed in the inspector is an instance override until the
        // user explicitly reapplies a theme. Other token bindings stay live.
        const bool equal=last.contains(property)&&(*target==last[property]||(color&&target->is_number_integer()&&last[property].is_number_integer()&&
            (target->get<uint32_t>()&0xffffffu)==(last[property].get<uint32_t>()&0xffffffu)));
        if(last.contains(property)&&!target->is_null()&&!equal) {
            theme["bindings"].erase(property);last.erase(property);continue;
        }
        // NativeStyles::Read uses opaque ARGB; matching that representation
        // keeps load/save/history stable and avoids false inspector overrides.
        *target=color?json(0xff000000u|value.get<unsigned>()):value;last[property]=*target;
        if(property=="bg_color")Style(record)["bg_opa"]=255;
    }
}
inline void ApplyTheme(json& doc,const std::string& id,const std::vector<int>& selected={})
{
    Preset(id);
    if(!doc.contains("obj")||!doc["obj"].is_array())throw std::invalid_argument("Project has no widget records");
    const bool all=selected.empty();std::set<int> ids(selected.begin(),selected.end());
    if(all) {doc["projectTheme"]["id"]=id;BindTheme(doc["canvas"],"project",true);ResolveTheme(doc["canvas"],doc);}
    for(auto& record:doc["obj"])if(all||ids.count(record.value("oid",-1))) {
        BindTheme(record,all?"project":id);ResolveTheme(record,doc);
    }
    if(all&&doc.contains("screens")&&doc["screens"].is_array())for(auto& screen:doc["screens"]) {
        screen.erase("projectTheme");
        BindTheme(screen["canvas"],"project",true);ResolveTheme(screen["canvas"],doc);
        for(auto& record:screen["obj"]){BindTheme(record,"project");ResolveTheme(record,doc);}
    }
}
inline void SetToken(json& doc,const std::string& id,const std::string& role,int value)
{
    const auto& tokens=Preset(id).at("tokens");if(!tokens.contains(role))throw std::invalid_argument("Unknown theme token");
    if(value<0||value>(role=="radius"||role=="spacing"?128:0xffffff))throw std::invalid_argument("Token is outside its supported range");
    doc["projectTheme"]["overrides"][id][role]=value;
    if(doc.contains("canvas"))ResolveTheme(doc["canvas"],doc);
    if(doc.contains("obj")&&doc["obj"].is_array())for(auto& record:doc["obj"])ResolveTheme(record,doc);
    if(doc.contains("screens")&&doc["screens"].is_array())for(auto& screen:doc["screens"]) {
        if(screen.contains("canvas"))ResolveTheme(screen["canvas"],doc);
        if(screen.contains("obj")&&screen["obj"].is_array())for(auto& record:screen["obj"])ResolveTheme(record,doc);
    }
}
inline void ApplyDefaults(const json& doc,json& record)
{ if(doc.contains("projectTheme")){BindTheme(record,"project");ResolveTheme(record,doc);} }

inline void RemapReferences(json& value,const std::map<int,int>& ids)
{
    if(value.is_array()) {for(auto& item:value)RemapReferences(item,ids);return;}
    if(!value.is_object())return;
    for(auto i=value.begin();i!=value.end();++i) {
        if((i.key()=="targetOid"||i.key()=="oid"||i.key()=="par")&&i.value().is_number_integer()) {
            auto found=ids.find(i.value().get<int>());if(found!=ids.end())i.value()=found->second;
        } else if(i.key()!="baseline")RemapReferences(i.value(),ids);
    }
}
inline std::string UniqueId(const json& doc,const std::string& prefix)
{
    const auto raw=doc.dump();for(unsigned n=1;;++n){const auto id=prefix+std::to_string(n);if(raw.find("\""+id+"\"")==std::string::npos)return id;}
}
inline json& Definition(json& doc,const std::string& id)
{
    auto& defs=doc["components"];if(!defs.is_array())defs=json::array();
    for(auto& d:defs)if(d.value("id",std::string())==id)return d;
    throw std::invalid_argument("Component definition is missing");
}
inline json ComponentInfo(const json& record)
{
    if(record.contains("base")&&record["base"].is_object()&&record["base"].contains("design")) {
        const auto& d=record["base"]["design"];if(d.is_object()&&d.contains("component")&&d["component"].is_object())return d["component"];
    }
    return json::object();
}
inline json CleanRecord(json record)
{
    record.erase("oid");record.erase("par");record.erase("hostIndex");record.erase("hostPath");
    if(record.contains("base")&&record["base"].is_object()&&record["base"].contains("design")) {
        auto& design=record["base"]["design"];if(design.is_object()){design.erase("component");if(design.empty())record["base"].erase("design");}
    }
    return record;
}
inline std::set<int> Subtree(const json& doc,const std::vector<int>& selected)
{
    std::set<int> ids(selected.begin(),selected.end());bool changed=true;
    while(changed){changed=false;for(const auto& r:doc.at("obj"))if(ids.count(r.value("par",-1)))changed=ids.insert(r.at("oid").get<int>()).second||changed;}
    return ids;
}
inline json CaptureRecords(const json& doc,const std::vector<int>& selected)
{
    if(selected.empty())throw std::invalid_argument("Select a widget or group first");
    const auto ids=Subtree(doc,selected);json result=json::array();std::map<int,int> remap;
    int next=0;for(const auto& r:doc.at("obj"))if(ids.count(r.value("oid",-1))) {
        const auto info=ComponentInfo(r);if(info.contains("member"))next=std::max(next,info["member"].get<int>()+1);
    }
    std::set<int> used;
    for(const auto& r:doc.at("obj"))if(ids.count(r.value("oid",-1))) {
        const auto info=ComponentInfo(r);int local=info.value("member",next++);
        if(!used.insert(local).second){local=next++;used.insert(local);}remap[r.at("oid").get<int>()]=local;
    }
    int minX=32767,minY=32767;
    for(const auto& r:doc.at("obj"))if(ids.count(r.value("oid",-1))&&!ids.count(r.value("par",-1))) {
        const auto c=r.value("base",json::object()).value("coords",json::object());minX=std::min(minX,c.value("x",0));minY=std::min(minY,c.value("y",0));
    }
    for(auto r:doc.at("obj"))if(ids.count(r.value("oid",-1))) {
        const int old=r.at("oid").get<int>(), parent=r.value("par",-1);auto clean=CleanRecord(r);
        RemapReferences(clean,remap);clean["oid"]=remap[old];clean["par"]=ids.count(parent)?remap[parent]:-1;
        if(!ids.count(parent)){auto& c=clean["base"]["coords"];c["x"]=c.value("x",0)-minX;c["y"]=c.value("y",0)-minY;}
        result.push_back(std::move(clean));
    }
    if(result.empty())throw std::invalid_argument("Selection contains no widgets");
    return result;
}
inline std::string Capture(json& doc,const std::vector<int>& selected,const std::string& name)
{
    if(name.empty()||name.size()>120)throw std::invalid_argument("Enter a component name (1–120 characters)");
    auto records=CaptureRecords(doc,selected);auto id=UniqueId(doc,"component-");
    if(!doc["components"].is_array())doc["components"]=json::array();
    doc["components"].push_back({{"id",id},{"name",name},{"revision",1},{"records",records}});return id;
}
inline json Override(const json& fresh,const json& baseline,const json& current)
{
    if(current==baseline)return fresh;
    if(current.is_object()&&baseline.is_object()&&fresh.is_object()) {
        auto result=fresh;
        for(auto i=baseline.begin();i!=baseline.end();++i)if(!current.contains(i.key()))result.erase(i.key());
        for(auto i=current.begin();i!=current.end();++i) {
            if(!baseline.contains(i.key()))result[i.key()]=i.value();
            else if(result.contains(i.key()))result[i.key()]=Override(result[i.key()],baseline[i.key()],i.value());
            else if(i.value()!=baseline[i.key()])result[i.key()]=i.value();
        }
        return result;
    }
    if(current.is_array()&&baseline.is_array()&&fresh.is_array()&&current.size()==baseline.size()&&fresh.size()==baseline.size()) {
        auto result=fresh;for(size_t i=0;i<result.size();++i)result[i]=Override(fresh[i],baseline[i],current[i]);return result;
    }
    return current;
}
inline void Propagate(json& doc,const std::string& definition,const std::string& source={})
{
    const auto def=Definition(doc,definition);const auto masters=def.at("records");
    if(!masters.is_array())throw std::invalid_argument("Invalid component records");
    std::map<std::string,std::map<int,json>> instances;int next=0;
    for(const auto& r:doc.at("obj")) {
        next=std::max(next,r.value("oid",-1)+1);const auto info=ComponentInfo(r);
        if(info.value("definition",std::string())==definition)instances[info.at("instance").get<std::string>()][info.at("member").get<int>()]=r;
    }
    for(const auto& instance:instances) {
        std::map<int,int> ids;int parent=-1,x=20,y=20;bool rootFound=false;
        for(const auto& m:masters) {const int local=m.at("oid").get<int>();auto old=instance.second.find(local);ids[local]=old==instance.second.end()?next++:old->second.at("oid").get<int>();}
        for(const auto& old:instance.second)if(old.second.value("par",-1)<0||!std::any_of(instance.second.begin(),instance.second.end(),[&](const auto& other){return other.second.value("oid",-2)==old.second.value("par",-1);})) {
            parent=old.second.value("par",-1);const auto& c=old.second.at("base").at("coords");
            // Normalize the first root against the corresponding master root.
            auto master=std::find_if(masters.begin(),masters.end(),[&](const json& m){return m.at("oid").get<int>()==old.first;});
            x=c.value("x",0)-(master==masters.end()?0:master->at("base").at("coords").value("x",0));
            y=c.value("y",0)-(master==masters.end()?0:master->at("base").at("coords").value("y",0));rootFound=true;break;
        }
        (void)rootFound;
        json replacement=json::array();
        for(auto master:masters) {
            const int local=master.at("oid").get<int>();const bool root=master.value("par",-1)<0;
            RemapReferences(master,ids);if(root){master["par"]=parent;auto& c=master["base"]["coords"];c["x"]=c.value("x",0)+x;c["y"]=c.value("y",0)+y;}
            const auto baseline=CleanRecord(master);auto merged=master;const auto old=instance.second.find(local);
            if(old!=instance.second.end()&&instance.first!=source) {
                const auto oldInfo=ComponentInfo(old->second);auto current=CleanRecord(old->second);
                auto prior=oldInfo.value("baseline",current);
                // Placement and name belong to each instance. A master edit can
                // still update every other property independently.
                if(root){prior["base"]["coords"]["x"]=current["base"]["coords"]["x"];prior["base"]["coords"]["y"]=current["base"]["coords"]["y"];}
                auto authored=Override(baseline,prior,current);authored["oid"]=master["oid"];authored["par"]=master["par"];merged=std::move(authored);
            }
            merged["base"]["design"]["component"]={{"definition",definition},{"instance",instance.first},{"member",local},{"revision",def.value("revision",1)},{"baseline",baseline}};
            replacement.push_back(std::move(merged));
        }
        auto& objects=doc["obj"];json updated=json::array();bool placed=false;
        for(auto& r:objects) {
            const auto info=ComponentInfo(r);
            if(info.value("instance",std::string())==instance.first){if(!placed){for(auto& item:replacement)updated.push_back(item);placed=true;}}
            else updated.push_back(r);
        }
        objects=std::move(updated);
    }
}
inline std::vector<int> Insert(json& doc,const std::string& definition,int parent=-1,int x=20,int y=20)
{
    const auto def=Definition(doc,definition);std::map<int,int> ids;int next=0;
    for(const auto& r:doc.at("obj"))next=std::max(next,r.value("oid",-1)+1);
    const auto instance=UniqueId(doc,"instance-");for(const auto& r:def.at("records"))ids[r.at("oid").get<int>()]=next++;
    std::vector<int> roots;
    for(auto r:def.at("records")) {
        const int local=r.at("oid").get<int>();const bool root=r.value("par",-1)<0;RemapReferences(r,ids);
        if(root){r["par"]=parent;auto& c=r["base"]["coords"];c["x"]=c.value("x",0)+x;c["y"]=c.value("y",0)+y;roots.push_back(r.at("oid").get<int>());}
        const auto baseline=CleanRecord(r);r["base"]["design"]["component"]={{"definition",definition},{"instance",instance},{"member",local},{"revision",def.value("revision",1)},{"baseline",baseline}};
        doc["obj"].push_back(std::move(r));
    }
    return roots;
}
inline std::string SelectedInstance(const json& doc,const std::vector<int>& selected)
{
    for(const auto& r:doc.at("obj"))if(std::find(selected.begin(),selected.end(),r.value("oid",-1))!=selected.end()) {
        const auto info=ComponentInfo(r);if(info.contains("instance"))return info.at("instance").get<std::string>();
    }
    throw std::invalid_argument("Select a linked component instance first");
}
inline void UpdateMaster(json& doc,const std::vector<int>& selected)
{
    const auto instance=SelectedInstance(doc,selected);std::string id;std::vector<int> roots;std::set<int> members;
    for(const auto& r:doc.at("obj"))if(ComponentInfo(r).value("instance",std::string())==instance) {id=ComponentInfo(r).at("definition").get<std::string>();members.insert(r.at("oid").get<int>());}
    for(const auto& r:doc.at("obj"))if(members.count(r.value("oid",-1))&&!members.count(r.value("par",-1)))roots.push_back(r.at("oid").get<int>());
    const auto records=CaptureRecords(doc,roots);auto& def=Definition(doc,id);def["records"]=records;def["revision"]=def.value("revision",1)+1;
    // Children added to the edited instance become members before propagation,
    // so replacement consumes the original child instead of leaving a duplicate.
    const auto sourceIds=Subtree(doc,roots);size_t index=0;
    for(auto& r:doc["obj"])if(sourceIds.count(r.value("oid",-1))) {
        const auto baseline=CleanRecord(r);r["base"]["design"]["component"]={{"definition",id},{"instance",instance},{"member",records.at(index++).at("oid")},{"revision",def["revision"]},{"baseline",baseline}};
    }
    Propagate(doc,id,instance);
    if(doc.contains("screens")&&doc["screens"].is_array()) {
        const auto current=doc.value("editorScreen",doc.value("activeScreen",size_t(0)));
        for(size_t i=0;i<doc["screens"].size();++i)if(i!=current) {
            auto& screen=doc["screens"][i];json view={{"obj",screen.value("obj",json::array())},{"components",doc["components"]}};
            Propagate(view,id);screen["obj"]=std::move(view["obj"]);
        }
    }
}
inline void Detach(json& doc,const std::vector<int>& selected)
{
    const auto instance=SelectedInstance(doc,selected);for(auto& r:doc.at("obj"))if(ComponentInfo(r).value("instance",std::string())==instance)r["base"]["design"].erase("component");
}
inline size_t InstanceCount(const json& doc,const std::string& id)
{
    std::set<std::string> found;
    auto count=[&](const json& records,const std::string& prefix){for(const auto& r:records){const auto info=ComponentInfo(r);if(info.value("definition",std::string())==id)found.insert(prefix+info.value("instance",std::string()));}};
    if(doc.contains("screens")&&doc["screens"].is_array()&&!doc["screens"].empty()) {
        const auto current=doc.value("editorScreen",doc.value("activeScreen",size_t(0)));
        for(size_t i=0;i<doc["screens"].size();++i)count(i==current&&doc.contains("obj")?doc["obj"]:doc["screens"][i].value("obj",json::array()),std::to_string(i)+":");
    }else if(doc.contains("obj"))count(doc["obj"],"");
    return found.size();
}
inline void CloneDesignLinks(json& records,const json& document,const std::map<int,int>& objectIds)
{
    // Duplicating a complete linked instance creates a new link identity.
    // Duplicating only one of its children produces an ordinary editable copy.
    if(!records.is_array())return;
    std::map<std::string,size_t> totals,copies;std::set<std::string> used;
    if(document.contains("obj"))for(const auto& r:document["obj"]) {
        const auto info=ComponentInfo(r);const auto id=info.value("instance",std::string());if(!id.empty()){++totals[id];used.insert(id);}
    }
    for(const auto& r:records){const auto id=ComponentInfo(r).value("instance",std::string());if(!id.empty())++copies[id];}
    std::map<std::string,std::string> renamed;unsigned next=1;
    for(const auto& pair:copies)if(pair.second==totals[pair.first]) {
        std::string id;do{id="instance-"+std::to_string(next++);}while(used.count(id));used.insert(id);renamed[pair.first]=id;
    }
    for(auto& r:records) {
        auto info=ComponentInfo(r);if(info.empty())continue;const auto old=info.value("instance",std::string());
        if(!renamed.count(old)){r["base"]["design"].erase("component");continue;}
        info["instance"]=renamed[old];if(info.contains("baseline"))RemapReferences(info["baseline"],objectIds);
        r["base"]["design"]["component"]=std::move(info);
    }
}
inline void Rename(json& doc,const std::string& id,const std::string& name)
{if(name.empty()||name.size()>120)throw std::invalid_argument("Enter a component name (1–120 characters)");Definition(doc,id)["name"]=name;}
inline void RemoveUnused(json& doc,const std::string& id)
{
    if(InstanceCount(doc,id))throw std::invalid_argument("Detach or remove this component's instances before deleting its definition");
    Definition(doc,id);auto& defs=doc["components"];defs.erase(std::remove_if(defs.begin(),defs.end(),[&](const json& d){return d.value("id",std::string())==id;}),defs.end());
}
inline json ExportLibrary(const json& doc)
{
    json result={{"format","lvgl-component-library"},{"version",1},{"components",doc.value("components",json::array())}};
    // A library can be imported into a project with a different default theme.
    // Keep its concrete captured appearance until the destination applies a
    // palette, rather than resolving a source-project token against that theme.
    for(auto& definition:result["components"])if(definition.contains("records")&&definition["records"].is_array())for(auto& r:definition["records"]) {
        if(r.contains("base")&&r["base"].contains("design")&&r["base"]["design"].is_object()) {
            auto& design=r["base"]["design"];design.erase("theme");if(design.empty())r["base"].erase("design");
        }
    }
    for(const auto* key:{"fonts","images","tries"})if(doc.contains(key))result[key]=doc[key];
    return result;
}
inline void ImportLibrary(json& doc,const json& library)
{
    if(!library.is_object()||library.value("format",std::string())!="lvgl-component-library"||library.value("version",0)!=1||!library.contains("components")||!library["components"].is_array())throw std::invalid_argument("Choose an LVGL component library JSON file");
    // Asset IDs are immutable references; conflicting assets would silently alter
    // a screen, so reject that conflict before changing the document.
    for(const auto* key:{"fonts","images","tries"})if(library.contains(key)&&library[key].is_array()) {
        if(!doc[key].is_array())doc[key]=json::array();
        for(const auto& asset:library[key]) {if(!asset.is_object()||!asset.contains("id"))throw std::invalid_argument("Invalid component library asset");bool exists=false;
            for(const auto& old:doc[key])if(old.value("id",json())==asset["id"]) {if(old!=asset)throw std::invalid_argument("Library asset ID conflicts with this project");exists=true;break;}
            if(!exists)doc[key].push_back(asset);
        }
    }
    if(!doc["components"].is_array())doc["components"]=json::array();
    for(auto d:library["components"]) {
        if(!d.is_object()||!d.contains("records")||!d["records"].is_array()||d["records"].empty())throw std::invalid_argument("Invalid component definition");
        std::set<int> ids;for(const auto& r:d["records"])if(!r.is_object()||!r.contains("base")||!r.contains("oid")||!r["oid"].is_number_integer()||!ids.insert(r["oid"].get<int>()).second)throw std::invalid_argument("Invalid component member");
        d["id"]=UniqueId(doc,"component-");d["revision"]=1;doc["components"].push_back(std::move(d));
    }
}
inline void Materialize(json& doc,bool stripAuthoring=false)
{
    if(!doc.is_object())return;
    if(doc.contains("canvas"))ResolveTheme(doc["canvas"],doc);
    if(doc.contains("obj")&&doc["obj"].is_array()) {
        for(auto& record:doc["obj"])ResolveTheme(record,doc);
        if(stripAuthoring)for(auto& record:doc["obj"])if(record.contains("base")&&record["base"].is_object())record["base"].erase("design");
    }
    // Screen records use project-level tokens unless they explicitly carry a
    // palette of their own. Resolve children before stripping the shared table.
    if(doc.contains("screens")&&doc["screens"].is_array())for(auto& screen:doc["screens"]) {
        const bool inherit=!screen.contains("projectTheme")&&doc.contains("projectTheme");
        if(inherit)screen["projectTheme"]=doc["projectTheme"];
        Materialize(screen,stripAuthoring);
        if(inherit)screen.erase("projectTheme");
    }
    if(stripAuthoring){if(doc.contains("canvas")&&doc["canvas"].contains("base"))doc["canvas"]["base"].erase("design");for(const auto* key:{"components","projectTheme","sharedStyles","previewProfiles","deviceTargets","editorScreen","editorSession","accessibility"})doc.erase(key);}
}
} }
