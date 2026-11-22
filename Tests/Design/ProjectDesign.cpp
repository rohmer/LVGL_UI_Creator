#include "../../LVSerial/Serialization/ProjectDesign.h"
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {
namespace D=Serialization::ProjectDesign;
using json=D::json;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
unsigned RGB(const json& value){return value.get<unsigned>()&0xffffffu;}
json Record(int id,int parent,int x,int y)
{return {{"oid",id},{"par",parent},{"base",{{"coords",{{"x",x},{"y",y},{"width",150},{"height",80}}},{"name","Widget "+std::to_string(id)}}}};}
double Luminance(unsigned color)
{
    auto channel=[](double c){c/=255;return c<=0.04045?c/12.92:std::pow((c+0.055)/1.055,2.4);};
    return 0.2126*channel((color>>16)&255)+0.7152*channel((color>>8)&255)+0.0722*channel(color&255);
}
double Contrast(unsigned a,unsigned b){const double x=Luminance(a),y=Luminance(b);return (std::max(x,y)+0.05)/(std::min(x,y)+0.05);}
json& Member(json& doc,const std::string& instance,int member)
{
    for(auto& r:doc["obj"]){const auto info=D::ComponentInfo(r);if(info.value("instance",std::string())==instance&&info.value("member",-1)==member)return r;}
    throw std::runtime_error("Missing component member");
}
}
void CheckProjectDesignCore()
{
    Require(D::Presets().size()>=36,"Theme gallery needs at least thirty-six palettes");
    std::set<std::string> ids,names,palettes;
    for(const auto& p:D::Presets()){
        const auto id=p.at("id").get<std::string>(),name=p.at("name").get<std::string>();
        Require(!id.empty()&&id.front()!='-'&&id.back()!='-'&&id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")==std::string::npos,"Theme ID is not a stable lowercase slug");
        Require(ids.insert(id).second,"Duplicate theme ID");Require(!name.empty()&&names.insert(name).second,"Empty or duplicate theme name");
        Require(p.at("description").is_string()&&!p.at("description").get<std::string>().empty(),"Theme needs a gallery description");
        Require(p.at("dark").is_boolean(),"Theme appearance metadata must be Boolean");
        const auto& t=p.at("tokens");Require(t.is_object()&&t.size()==13,"Theme token schema is incomplete");
        Require(palettes.insert(t.dump()).second,"Two theme choices have identical design tokens");
        for(const auto* color:{"background","surface","text","muted","primary","accent","success","warning","error","border","onPrimary"})
            Require(t.at(color).is_number_integer()&&t.at(color).get<std::int64_t>()>=0&&t.at(color).get<std::int64_t>()<=0xffffff,"Theme color is outside RGB24");
        for(const auto* metric:{"radius","spacing"})
            Require(t.at(metric).is_number_integer()&&t.at(metric).get<int>()>=0&&t.at(metric).get<int>()<=128,"Theme geometry token is outside supported bounds");
        Require(Contrast(t.at("text").get<unsigned>(),t.at("surface").get<unsigned>())>=4.5,"Theme body text lacks contrast");
        Require(Contrast(t.at("onPrimary").get<unsigned>(),t.at("primary").get<unsigned>())>=4.5,"Theme button text lacks contrast");
    }
    for(const auto* legacy:{"slate-light","slate-dark","ocean-light","ocean-dark","forest-light","forest-dark","amber-light","amber-dark","violet-light","violet-dark","rose-light","rose-dark","mono-light","mono-dark"})
        Require(ids.count(legacy)!=0,"An existing saved-project theme ID was removed");
    struct Addition { const char* id;const char* name;bool dark; };
    const Addition additions[]={
        {"contrast-light","Signal Paper",false},{"contrast-dark","Signal Night",true},
        {"blueprint-light","Blueprint",false},{"blueprint-dark","Blueprint Night",true},
        {"copper-light","Kiln",false},{"copper-dark","Copper Foundry",true},
        {"arctic-light","Glacier",false},{"arctic-dark","Polar Night",true},
        {"citrus-light","Citrus Grove",false},{"citrus-dark","Citrus After Dark",true},
        {"editorial-light","Editorial",false},{"editorial-dark","Edition Night",true},
        {"paper-ink","Paper Ink",false},{"black-glass","Black Glass",true},
        {"vault-light","Vault",false},{"vault-dark","Vault Night",true},
        {"bloom-light","Bloom",false},{"bloom-dark","Bloom Night",true},
        {"studio-light","Studio",false},{"studio-dark","Studio Night",true},
        {"estate-light","Estate",false},{"estate-dark","Estate Night",true}};
    for(const auto& addition:additions){
        const auto& p=D::Preset(addition.id);const auto& t=p.at("tokens");
        Require(p.at("name")==addition.name&&p.at("dark")==addition.dark,"New palette gallery identity changed");
        const double minimum=std::string(addition.id).find("contrast-")==0?7.0:4.5;
        for(const auto* foreground:{"text","muted"})for(const auto* background:{"background","surface"})
            Require(Contrast(t.at(foreground).get<unsigned>(),t.at(background).get<unsigned>())>=minimum,"New palette body or secondary text lacks promised contrast");
        Require(Contrast(t.at("onPrimary").get<unsigned>(),t.at("primary").get<unsigned>())>=minimum,"New palette action text lacks promised contrast");
    }
    for(const auto* id:{"vault-light","vault-dark","bloom-light","bloom-dark","studio-light","studio-dark","estate-light","estate-dark"}){
        auto card=Record(0,-1,12,12),button=Record(1,0,20,20),label=Record(2,0,20,110),code=Record(3,0,200,20);
        button["button"]={{"text","Save settings"}};label["text"]="Device name";
        code["qrcode"]={{"data","Device identity"},{"dark",0x000000},{"light",0xffffff}};
        D::Style(code)["bg_color"]=0xffffff;D::Style(code)["text_color"]=0x000000;
        json themed={{"canvas",{{"width",480},{"height",320}}},{"obj",json::array({card,button,label,code})}};
        themed["screens"]=json::array({{{"canvas",themed["canvas"]},{"obj",themed["obj"]}}});
        const auto original=themed;const auto& tokens=D::Preset(id).at("tokens");D::ApplyTheme(themed,id);
        Require(D::ThemeId(themed)==id,"Applying an added palette did not set the project theme");
        auto exported=json::parse(themed.dump());D::Materialize(exported,true);
        auto verify=[&](json& screen,const json& source){
            Require(!screen.contains("projectTheme")&&!screen["canvas"]["base"].contains("design"),"Device export retained theme metadata");
            Require(RGB(D::Style(screen["canvas"])["bg_color"])==tokens.at("background"),"Device export lost an added palette's screen color");
            Require(RGB(D::Style(screen["obj"][0])["bg_color"])==tokens.at("surface"),"Device export lost an added palette's surface color");
            Require(RGB(D::Style(screen["obj"][1])["bg_color"])==tokens.at("primary")&&RGB(D::Style(screen["obj"][1])["text_color"])==tokens.at("onPrimary"),"Device export lost an added palette's button colors");
            Require(screen["obj"][1]["button"]==source["obj"][1]["button"]&&screen["obj"][2]["text"]==source["obj"][2]["text"]&&screen["obj"][3]["qrcode"]==source["obj"][3]["qrcode"]&&RGB(D::Style(screen["obj"][3])["bg_color"])==0xffffff,"An added palette changed content or QR contrast");
            for(size_t i=0;i<screen["obj"].size();++i){
                Require(!screen["obj"][i]["base"].contains("design"),"Device export retained widget theme metadata");
                Require(screen["obj"][i]["base"]["coords"]==source["obj"][i]["base"]["coords"],"An added palette changed widget geometry");
            }
        };
        verify(exported,original);verify(exported["screens"][0],original["screens"][0]);
    }
    json doc={{"obj",json::array({Record(4,-1,40,60),Record(7,4,10,12),Record(9,4,10,40)})},{"canvas",{{"width",320},{"height",240}}}};
    doc["obj"][0]["button"]=json::object();doc["obj"][1]["text"]="Master";doc["obj"][2]["advancedKeyboard"]={{"targetOid",7}};
    doc["obj"][0]["base"]["events"]=json::array({{{"event","clicked"},{"action","show"},{"targetOid",7},{"screen",2}}});
    D::ApplyTheme(doc,"ocean-dark");const auto originalCoordinates=doc["obj"][0]["base"]["coords"];
    Require(RGB(D::Style(doc["obj"][0]).at("bg_color"))==D::Preset("ocean-dark")["tokens"]["primary"],"Theme did not color button");
    D::Style(doc["obj"][0])["text_color"]=0xabcdef;
    D::SetToken(doc,"ocean-dark","primary",0x1188aa);D::SetToken(doc,"ocean-dark","onPrimary",0xffffff);D::SetToken(doc,"ocean-dark","spacing",18);
    Require(RGB(D::Style(doc["obj"][0]).at("bg_color"))==0x1188aa,"Shared token did not update participating widget");
    Require(D::Style(doc["obj"][0]).at("text_color")==0xabcdef,"Shared token lost a local override");
    Require(D::Style(doc["obj"][0]).at("pad_left")==18,"Spacing token did not update widget");
    Require(doc["obj"][0]["base"]["coords"]==originalCoordinates,"Theme altered authored geometry");
    auto newWidget=Record(10,-1,0,0);D::ApplyDefaults(doc,newWidget);Require(RGB(D::Style(newWidget)["bg_color"])==D::Preset("ocean-dark")["tokens"]["surface"],"New widgets do not inherit project theme");
    D::ApplyTheme(doc,"rose-light",{7});Require(D::ThemeId(doc)=="ocean-dark","Selection theme changed project default");
    auto qrcode=Record(11,-1,0,0);qrcode["qrcode"]={{"data","LVGL"}};D::Style(qrcode)["bg_color"]=0xffffff;D::ApplyDefaults(doc,qrcode);Require(D::Style(qrcode)["bg_color"]==0xffffff,"Theme changed QR contrast");

    const auto id=D::Capture(doc,{4},"Login panel");Require(D::Definition(doc,id).at("records").size()==3,"Capture omitted descendants");
    const auto first=D::Insert(doc,id,-1,100,80);const auto second=D::Insert(doc,id,-1,220,100);Require(first.size()==1&&second.size()==1,"Insert returned incorrect roots");
    const auto instanceA=D::ComponentInfo(doc["obj"][3]).at("instance").get<std::string>();const auto instanceB=D::ComponentInfo(doc["obj"][6]).at("instance").get<std::string>();
    const auto& def=D::Definition(doc,id);const int rootMember=def["records"][0]["oid"].get<int>(),textMember=def["records"][1]["oid"].get<int>(),keyboardMember=def["records"][2]["oid"].get<int>();
    auto& aText=Member(doc,instanceA,textMember);auto& bText=Member(doc,instanceB,textMember);const auto bTextId=bText["oid"];
    Require(Member(doc,instanceB,keyboardMember)["advancedKeyboard"]["targetOid"]==bTextId,"Keyboard target did not remap on insertion");
    Require(Member(doc,instanceB,rootMember)["base"]["events"][0]["targetOid"]==bTextId,"Event target did not remap on insertion");
    Require(Member(doc,instanceB,rootMember)["base"]["events"][0]["screen"]==2,"Screen index was incorrectly remapped");
    bText["text"]="Local text";D::Style(bText)["text_color"]=0x123456;aText["text"]="Updated master";
    Member(doc,instanceA,rootMember)["base"]["coords"]["width"]=190;
    D::UpdateMaster(doc,first);
    Require(Member(doc,instanceB,textMember)["text"]=="Local text","Master update lost instance text override");
    Require(D::Style(Member(doc,instanceB,textMember))["text_color"]==0x123456,"Master update lost instance style override");
    Require(Member(doc,instanceB,rootMember)["base"]["coords"]["width"]==190,"Master layout did not propagate");
    Require(Member(doc,instanceB,rootMember)["base"]["coords"]["x"]==220,"Master update moved another instance");
    auto child=Record(30,first[0],12,100);child["text"]="New master child";doc["obj"].push_back(child);D::UpdateMaster(doc,first);
    Require(D::Definition(doc,id)["records"].size()==4,"Master update omitted a new child");
    size_t copies=0;for(const auto& r:doc["obj"])if(r.value("text",std::string())=="New master child")++copies;Require(copies==2,"New master child did not propagate to all instances");
    auto& objects=doc["obj"];objects.erase(std::remove_if(objects.begin(),objects.end(),[&](const json& r){return D::ComponentInfo(r).value("instance",std::string())==instanceA&&r.value("text",std::string())=="New master child";}),objects.end());D::UpdateMaster(doc,first);
    Require(D::Definition(doc,id)["records"].size()==3,"Master update did not remove a deleted child");
    for(const auto& r:doc["obj"])Require(r.value("text",std::string())!="New master child","Deleted child survived in a linked instance");
    json clones=json::array();std::map<int,int> cloneIds;
    for(const auto& r:doc["obj"])if(D::ComponentInfo(r).value("instance",std::string())==instanceA){clones.push_back(r);cloneIds[r.at("oid").get<int>()]=90+static_cast<int>(clones.size());}
    D::RemapReferences(clones,cloneIds);D::CloneDesignLinks(clones,doc,cloneIds);
    Require(D::ComponentInfo(clones[0]).at("instance")!=instanceA,"Duplicate retained another instance's identity");
    json partial=json::array({Member(doc,instanceA,textMember)});D::CloneDesignLinks(partial,doc,{});Require(D::ComponentInfo(partial[0]).empty(),"Partial duplicate must detach its component link");
    Require(D::InstanceCount(doc,id)==2,"Instance counting is incorrect");
    bool blocked=false;try{D::RemoveUnused(doc,id);}catch(const std::invalid_argument&){blocked=true;}Require(blocked,"Deleting a used definition must fail");
    D::Detach(doc,second);Require(D::InstanceCount(doc,id)==1,"Detach retained a linked instance");
    D::Rename(doc,id,"Renamed panel");Require(D::Definition(doc,id)["name"]=="Renamed panel","Rename failed");
    auto library=D::ExportLibrary(doc);json imported={{"obj",json::array()}};D::ImportLibrary(imported,library);Require(imported["components"].size()==1,"Library import lost definition");
    const auto importedId=imported["components"][0]["id"].get<std::string>();Require(D::Insert(imported,importedId).size()==1,"Imported component cannot be instantiated");
    const auto saved=json::parse(doc.dump());doc=saved;D::Materialize(doc);const auto once=doc;D::Materialize(doc);Require(doc==once,"Materialization must be idempotent");
    D::Materialize(doc,true);Require(!doc.contains("components")&&!doc.contains("projectTheme"),"Device export retained authoring libraries");
    for(const auto& r:doc["obj"])Require(!r["base"].contains("design"),"Device export retained instance metadata");
    Require(RGB(D::Style(doc["obj"][0])["bg_color"])==0x1188aa,"Device export lost materialized theme");
    std::cout<<"Project design tests passed: "<<D::Presets().size()<<" palettes, shared tokens, linked overrides, targets, import/export, flattening\n";
}
#ifdef PROJECT_DESIGN_STANDALONE
int main(){try{CheckProjectDesignCore();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
