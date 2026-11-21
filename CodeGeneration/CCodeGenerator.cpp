#include "CCodeGenerator.h"
#include "ProjectAssets.h"
#include "CRuntime.h"
#include "../LVSerial/Serialization/InteractionActions.h"
#include "../LVSerial/Serialization/ProjectDesign.h"
#include "Targets/Target.h"
#include <Targets/CompatSource.h>
#include "../LVSerial/Serialization/ImageAsset.h"
#include "../LVSerial/Serialization/ImageRecords.h"
#include "../LVSerial/Serialization/LVVariable.h"
#include "../LVSerial/Serialization/ProjectFonts.h"
#include "../LVSerial/Serialization/ProjectImages.h"
#include "../LVSerial/Serialization/CodeWidgets.h"
#include "../LVSerial/Serialization/AdvancedKeyboard.h"
#include "../LVSerial/Serialization/AdvancedKeyboardMigration.h"
#include "../LVSerial/Serialization/ProjectTries.h"
#include <AdvancedKeyboardSource.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <algorithm>
#include <iomanip>
#include <functional>

namespace {
using json = nlohmann::json;
std::string quote(const std::string& text) {
    std::ostringstream out; out << '"';
    for (unsigned char c : text) {
        if(c >= 32 && c < 127 && c != '"' && c != '\\') out << char(c);
        else out << '\\' << std::oct << std::setw(3) << std::setfill('0') << unsigned(c) << std::dec;
    }
    return out.str() + '"';
}
std::string number(const json& j) {
    if(j.is_object()&&j.contains("coordinateKind")) {auto type=j.at("coordinateKind").get<std::string>();if(type=="content")return "LV_SIZE_CONTENT";if(type=="maximum")return "LV_COORD_MAX";if(type=="minimum")return "LV_COORD_MIN";if(type=="gridContent")return "LV_GRID_CONTENT";auto value=j.at("value").get<int32_t>();if(type=="percent")return "LV_PCT("+std::to_string(value)+")";if(type=="gridFraction"&&value>=0&&value<=100)return "LV_GRID_FR("+std::to_string(value)+")";throw std::runtime_error("Invalid portable coordinate");}
    if(j.is_boolean()) return j.get<bool>() ? "true" : "false";
    if(!j.is_number()) throw std::runtime_error("Expected a numeric property");
    return j.dump();
}
std::string upper(std::string s) { for(char& c:s) c=char(std::toupper(static_cast<unsigned char>(c))); return s; }
std::string identifier(std::string s) {
    for(char& c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9'))) c='_';
    return "v_" + s;
}
std::vector<std::string> variableIdentifiers(const json& records) {
    if(!records.is_array())throw std::runtime_error("Variables must be an array");
    std::set<std::string> names;std::vector<std::string> result;size_t serial=0;
    for(const auto& record:records) {
        const auto stem=identifier(Serialization::LVVariable::FromJSON(record).name);auto name=stem;
        while(!names.insert(name).second)name=stem+"_"+std::to_string(++serial);
        ++serial;result.push_back(std::move(name));
    }
    return result;
}
const json& section(const json& j,const char* key) {
    static const json empty=json::object(); auto it=j.find(key); return it!=j.end()&&it->is_object()?*it:empty;
}
const std::map<std::string,std::string> kinds={
    {"arc","arc"},{"bar","bar"},{"button","button"},{"buttonMatrix","btnm"},{"calendar","cal"},
    {"canvas","canvas"},{"checkbox","cb"},{"image","img"},{"label","text"},
    {"dropdown","dropdown"},{"roller","roller"},{"slider","slider"},{"spinbox","spinbox"},
    {"switch","switch"},{"textarea","textarea"},{"led","led"},{"line","line"},{"spinner","spinner"},
    {"chart","chart"},{"table","table"},{"scale","scale"},{"keyboard","keyboard"},{"tabview","tabview"},
    {"tileview","tileview"},{"window","window"},{"messagebox","messagebox"},{"imagebutton","imagebutton"},
    {"page","page"},{"textBox","textBox"},{"layerView","layerView"},{"barcode","barcode"},{"qrcode","qrcode"},{"advancedKeyboard","advancedKeyboard"},{"object","object"}};
std::string kind(const json& r) {
    for(auto& k:kinds) if(r.contains(k.second) && k.first!="object") return k.first;
    if(r.contains("base")) return "object";
    throw std::runtime_error("Widget record has no base or supported widget type");
}
struct Generator {
    std::ostringstream globals,body,assets,assetHeader,header,variableCode,variableInit,variableFree,variableWiring,keyboardFree,actionCode,actionInit;
    std::filesystem::path source;
    std::set<std::string> requirements;
    std::map<std::string,std::string> assetCache,fontCache;
    std::map<std::pair<size_t,int>,std::string> handles,types;
    std::map<std::pair<size_t,int>,json> variableWidgetRecords;
    struct ActionVariable { std::string field; Serialization::VariableData definition; };
    std::map<std::string, ActionVariable> actionVariables;
    json manifest={{"lvglVersion","9.6.0"},{"screens",json::array()},{"objects",json::array()},{"variables",json::array()},{"images",json::array()}};
    size_t serial=0,screenCount=0;
    bool usesAdvancedKeyboard=false;
    bool usesPrediction=false;
    std::vector<std::pair<std::string,int32_t>> focusOwners;
    std::vector<std::pair<std::string,uint32_t>> screenTransitions;
    bool usesTransitions=false;
    size_t authoringScreen=0;
    struct TrieAsset {std::string data,index;size_t dataSize,indexSize;};
    std::map<std::string,TrieAsset> trieAssets;
    struct KeyboardBinding {std::string object;size_t screen;json settings;};
    std::vector<KeyboardBinding> keyboards;
    std::vector<std::pair<size_t,std::string>> textTargets;
    std::set<std::string> bootstrapActions;
    std::string unique(const std::string& prefix) {return "ui_"+prefix+"_"+std::to_string(serial++);}
    void line(const std::string& s) {body << "    " << s << "\n";}
    void call(const std::string& fn,const std::string& o,const std::string& args) {line(fn+"("+o+(args.empty()?"":", "+args)+");");}
    void prop(const json& d,const std::string& key,const std::string& fn,const std::string& o,const std::string& extra="") {
        if(d.contains(key)) call(fn,o,number(d.at(key))+extra);
    }
    void text(const json& d,const std::string& key,const std::string& fn,const std::string& o) {
        if(d.contains(key)) call(fn,o,d.at(key).is_null()?"NULL":quote(d.at(key).get<std::string>()));
    }
    void linkKeyboards() {
        for(const auto& keyboard:keyboards) {
            const int id=keyboard.settings.value("targetOid",-1);
            const bool automatic=keyboard.settings.value("autoFocus",true);
            std::vector<std::string> targets;
            if(id>=0) {
                const auto key=std::make_pair(keyboard.screen,id);const auto found=handles.find(key);
                if(found==handles.end())throw std::runtime_error("Advanced Keyboard references a missing text box");
                const auto& type=types.at(key);
                if(type=="textarea")targets.push_back(found->second);
                else if(type=="textBox")targets.push_back(found->second+"_custom.entry");
                else throw std::runtime_error("Advanced Keyboard target must be a text area or Text box");
            } else if(automatic) {
                for(const auto& entry:textTargets)if(entry.first==keyboard.screen)targets.push_back(entry.second);
            }
            for(const auto& target:targets) {
                if(automatic)line("if (!lv_advanced_keyboard_bind_textarea("+keyboard.object+", "+target+")) { ui_deinit(); return false; }");
                else call("lv_advanced_keyboard_set_textarea",keyboard.object,target);
            }
        }
    }
    std::string bytes(const std::string& data,bool writable=false,bool exported=false) {
        auto name=unique("bytes"); assets << (exported?"":"static ") << "LV_ATTRIBUTE_MEM_ALIGN " << (writable?"":"const ") << "uint8_t " << name << "[] = {";
        size_t count=0; for(unsigned char c:data) {if(count++%24==0) assets<<"\n    ";assets<<unsigned(c)<<',';}
        if(data.empty()) assets<<'0'; assets<<"\n};\n";
        if(exported)assetHeader<<"extern const uint8_t "<<name<<"[];\n";
        return name;
    }
    std::string array(const json& data,const std::string& type,const std::string& end="") {
        auto name=unique("array"); assets<<"static const "<<type<<' '<<name<<"[] = {";
        for(const auto& item:data) assets<<number(item)<<',';
        assets<<end; if(data.empty()&&end.empty()) assets<<'0'; assets<<"};\n";return name;
    }
    struct Image {std::string expression="NULL",frames,delays;size_t count=0;};
    Image image(const json& record) {
        if(record.is_null()||record.empty()) return {};
        auto key=(record.contains("asset")?json{{"asset",record.at("asset")}}:record).dump(); auto cached=assetCache.find(key);
        if(cached!=assetCache.end()) { auto meta=json::parse(cached->second);return {meta[0],meta[1],meta[2],meta[3]}; }
        json d=record; Image result;
        if(d.contains("asset")&&d["asset"].contains("deviceFrames")) {
            const auto& asset=d["asset"];const auto& stored=asset.at("deviceFrames");if(!stored.is_array()||stored.empty())throw std::runtime_error("Converted image has no device frames");
            const auto name=unique("converted_image");std::vector<std::string> frames;size_t i=0;
            for(const auto& f:stored) {
                auto raw=base64_decode(f.at("data").get<std::string>());if(raw.empty())throw std::runtime_error("Converted image data is empty");
                const auto width=f.at("w").get<uint32_t>(),height=f.at("h").get<uint32_t>(),stride=f.at("stride").get<uint32_t>(),cf=f.at("cf").get<uint32_t>(),flags=f.value("flags",0u);
                if(!width||!height||width>65535||height>65535||stride>65535)throw std::runtime_error("Invalid converted image dimensions");
                auto data=bytes(raw);auto frame=name+"_"+std::to_string(i++);frames.push_back(frame);
                assets<<"const lv_image_dsc_t "<<frame<<" = {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf="<<cf<<",.flags="<<flags<<",.w="<<width<<",.h="<<height<<",.stride="<<stride<<"},.data_size="<<raw.size()<<",.data="<<data<<"};\n";assetHeader<<"extern const lv_image_dsc_t "<<frame<<";\n";
                if(flags&8) {requirements.insert("BIN_DECODER_RAM_LOAD");auto method=raw.size()>=4?static_cast<unsigned char>(raw[0]):0;if(method==2)requirements.insert("LZ4");else if(method==1)requirements.insert("RLE");else throw std::runtime_error("Unknown image compression method");}
                if(cf==1||cf==2){if(raw.size()>3&&static_cast<unsigned char>(raw[0])==137)requirements.insert("LODEPNG");else if(raw.size()>2&&static_cast<unsigned char>(raw[0])==255)requirements.insert("TJPGD");}
                manifest["images"].push_back({{"symbol",frame},{"width",width},{"height",height},{"colorFormat",cf},{"flags",flags},{"bytes",raw.size()}});
            }
            result={"&"+frames[0],name+"_frames",name+"_delays",frames.size()};
            assets<<"const lv_image_dsc_t *const "<<result.frames<<"[] = {";for(auto& f:frames)assets<<'&'<<f<<',';assets<<"};\nconst uint32_t "<<result.delays<<"[] = {";
            for(size_t n=0;n<frames.size();++n)assets<<(asset.contains("delays")&&n<asset["delays"].size()?std::max(20u,asset["delays"][n].get<uint32_t>()):100u)<<',';assets<<"};\n";
            assetHeader<<"extern const lv_image_dsc_t *const "<<result.frames<<"[];\nextern const uint32_t "<<result.delays<<"[];\n";
            assetCache[key]=json::array({result.expression,result.frames,result.delays,result.count}).dump();return result;
        }
        if(d.value("kind",std::string{})=="text") {
            const auto s=d.value("source",std::string{});
            if(!s.empty()&&static_cast<unsigned char>(s[0])>=0x80) return {quote(s)};
            d={{"fileName",s}};
        }
        if(d.contains("fileName")) {
            auto name=d.at("fileName").get<std::string>();
            auto path=std::filesystem::u8path(name);
            if(!std::filesystem::exists(path)&&name.size()>2&&name[1]==':') path=std::filesystem::u8path(name.substr(2));
            if(path.is_relative()) path=source/path;
            std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Image file not found: "+path.u8string());
            std::string raw((std::istreambuf_iterator<char>(input)),{});
            auto extension=path.extension().u8string();if(!extension.empty())extension.erase(0,1);
            std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
            d={{"asset",{{"name","image."+extension},{"sourceData",base64_encode(reinterpret_cast<const unsigned char*>(raw.data()),static_cast<unsigned int>(raw.size()))}}}};
        }
        std::vector<uint8_t> decoded;std::vector<uint32_t> delays;uint32_t width=0,height=0,stride=0,cf=0;
        if(d.contains("asset") || d.value("kind",std::string{})=="encoded") {
            json a;
            if(d.contains("asset")) a=d["asset"];
            else {
                auto raw=base64_decode(d.value("data",std::string{}));
                std::string extension=raw.size()>3&&raw.substr(0,3)=="GIF"?"gif":raw.size()>3&&static_cast<unsigned char>(raw[0])==137?"png":"jpg";
                a={{"name","image."+extension},{"sourceData",d.at("data")}};
            }
            auto pixels=Serialization::ImageAsset::Decode(a);width=pixels.width;height=pixels.height;decoded=std::move(pixels.pixels);delays=std::move(pixels.delays);
            stride=width*4;cf=LV_COLOR_FORMAT_ARGB8888;
        } else {
            width=d.value("w",d.value("header.w",0u));height=d.value("h",d.value("header.h",0u));
            cf=Serialization::ReadImageFormat(d);stride=d.value("stride",(width*lv_color_format_get_bpp(static_cast<lv_color_format_t>(cf))+7)/8);
            auto raw=base64_decode(d.value("data",std::string{}));decoded.assign(raw.begin(),raw.end());delays={100};
            const auto required=uint64_t(stride)*height+LV_COLOR_INDEXED_PALETTE_SIZE(cf)*4;
            if(!width||!height||cf==LV_COLOR_FORMAT_UNKNOWN||required!=decoded.size()) throw std::runtime_error("Invalid image pixel data");
        }
        if(delays.empty()) delays.push_back(100);
        auto name=unique("image");std::vector<std::string> frames;
        const size_t size=decoded.size()/delays.size();
        for(size_t i=0;i<delays.size();++i) {
            auto data=bytes(std::string(reinterpret_cast<const char*>(decoded.data()+i*size),size)); auto frame=name+"_"+std::to_string(i);frames.push_back(frame);
            assets<<"const lv_image_dsc_t "<<frame<<" = {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf="<<cf<<",.w="<<width<<",.h="<<height<<",.stride="<<stride<<"},.data_size="<<size<<",.data="<<data<<"};\n";
            assetHeader<<"extern const lv_image_dsc_t "<<frame<<";\n";
        }
        result.expression="&"+frames[0];result.count=frames.size();result.frames=name+"_frames";result.delays=name+"_delays";
        assets<<"const lv_image_dsc_t *const "<<result.frames<<"[] = {";for(auto& f:frames) assets<<'&'<<f<<',';assets<<"};\n";
        assets<<"const uint32_t "<<result.delays<<"[] = {";for(auto t:delays) assets<<std::max(20u,t)<<',';assets<<"};\n";
        assetHeader<<"extern const lv_image_dsc_t *const "<<result.frames<<"[];\nextern const uint32_t "<<result.delays<<"[];\n";
        manifest["images"].push_back({{"symbol",frames[0]},{"width",width},{"height",height},{"frames",frames.size()}});
        assetCache[key]=json::array({result.expression,result.frames,result.delays,result.count}).dump();return result;
    }
    void animate(const Image& im,const std::string& o,int target=0,int state=0,int side=0,const std::string& selector="0") {
        if(im.count<2) return;auto name=unique("animation");
        globals<<"static ui_animation_t "<<name<<" = {.frames="<<im.frames<<",.delays="<<im.delays<<",.count="<<im.count<<",.target="<<target<<",.state="<<state<<",.side="<<side<<",.selector="<<selector<<"};\n";
        line("ui_animation_start(&"+name+", "+o+");");
    }
    std::string font(const json& d);
    void legacyStyle(const std::string& o,const json& d,const std::string& sel="LV_PART_MAIN") {
        const std::map<std::string,std::string> props={
            {"/body/main_color","bg_color"},{"/body/gradColor","bg_grad_color"},{"/body/opa","bg_opa"},{"/body/radius","radius"},
            {"/body/border/color","border_color"},{"/body/border/opa","border_opa"},{"/body/border/width","border_width"},{"/body/border/part","border_side"},
            {"/body/padding/left","pad_left"},{"/body/padding/right","pad_right"},{"/body/padding/top","pad_top"},{"/body/padding/bottom","pad_bottom"},
            {"/body/padding/inner","pad_row"},{"/body/shadow/color","shadow_color"},{"/body/shadow/width","shadow_width"},
            {"/text/color","text_color"},{"/text/opa","text_opa"},{"/text/letter_space","text_letter_space"},{"/text/line_space","text_line_space"},
            {"/line/color","line_color"},{"/line/opa","line_opa"},{"/line/width","line_width"},{"/line/rounded","line_rounded"},
            {"/image/color","image_recolor"},{"/image/intense","image_recolor_opa"},{"/image/opa","image_opa"}};
        for(auto& p:props) {json::json_pointer ptr(p.first);if(d.contains(ptr)) {
            auto val=number(d.at(ptr));if(p.second.find("color")!=std::string::npos&&p.second.find("opa")==std::string::npos) val="lv_color_hex("+val+")";
            call("lv_obj_set_style_"+p.second,o,val+", "+sel);
        }}
        if(d.contains(json::json_pointer("/body/gradColor"))) call("lv_obj_set_style_bg_grad_dir",o,"LV_GRAD_DIR_VER, "+sel);
        if(d.contains(json::json_pointer("/body/padding/inner"))) call("lv_obj_set_style_pad_column",o,number(d.at(json::json_pointer("/body/padding/inner")))+", "+sel);
        if(d.contains(json::json_pointer("/text/font"))) call("lv_obj_set_style_text_font",o,font(d.at(json::json_pointer("/text/font")))+", "+sel);
    }
    void base(const std::string& o,const json& b) {
        if(b.contains("bootstrapAction")) {
            const auto action=b.at("bootstrapAction").get<std::string>();
            if(action!="dismiss" && action!="collapse") throw std::runtime_error("Unknown Bootstrap action: "+action);
            if(bootstrapActions.insert(action).second) {
                globals << "static void ui_bootstrap_" << action << "(lv_event_t *event) {\n"
                    "    lv_obj_t *parent = lv_obj_get_parent((lv_obj_t*)lv_event_get_current_target(event));\n"
                    "    if (!parent) return;\n";
                if(action=="dismiss") globals << "    lv_obj_set_hidden(parent, true);\n";
                else globals << "    lv_obj_t *body = lv_obj_get_child(parent, 1);\n    if (body) lv_obj_set_hidden(body, !lv_obj_is_hidden(body));\n";
                globals << "}\n";
            }
            call("lv_obj_add_event_cb",o,"ui_bootstrap_"+action+", LV_EVENT_CLICKED, NULL");
        }
        legacyStyle(o,section(b,"style"));
        auto& c=section(b,"coords");for(auto p:{"width","height","x","y"}) prop(c,p,"lv_obj_set_"+std::string(p),o);
        for(auto p:{std::make_pair("click","clickable"),{"hidden","hidden"},{"parevent","event_bubble"},{"checked","checked"},{"disabled","disabled"}}) prop(b,p.first,"lv_obj_set_"+std::string(p.second),o);
        if(b.value("opascaleen",false)) prop(b,"opascale","lv_obj_set_style_opa",o,", LV_PART_MAIN");
        if(b.contains("nativeStyles")) for(const auto& entry:b["nativeStyles"]) {
            const auto sel=number(entry.at("selector")); const auto& v=entry.at("values");
            for(auto it=v.begin();it!=v.end();++it) {
                const auto& key=it.key();const auto& val=it.value();
                if(key=="text_font") call("lv_obj_set_style_text_font",o,font(val)+", "+sel);
                else if(key=="bg_image_src"||key=="arc_image_src"||key=="bitmap_mask_src") {
                    auto im=image(val);call("lv_obj_set_style_"+key,o,im.expression+", "+sel);animate(im,o,key=="bg_image_src"?1:key=="arc_image_src"?2:3,0,0,sel);
                } else if(key=="grid_column_dsc_array"||key=="grid_row_dsc_array") {
                    requirements.insert("GRID");auto arr=unique("grid_tracks");globals<<"static const int32_t "<<arr<<"[] = {";for(const auto& track:val)globals<<number(track)<<',';globals<<"LV_GRID_TEMPLATE_LAST};\n";
                    call("lv_obj_set_style_"+key,o,arr+", "+sel);
                } else if(key=="bg_grad") gradient(o,val,sel);
                else {
                    if(key.rfind("flex_",0)==0)requirements.insert("FLEX");
                    if(key.rfind("grid_",0)==0)requirements.insert("GRID");
                    if(!std::all_of(key.begin(),key.end(),[](char x){return (x>='a'&&x<='z')||x=='_'||(x>='0'&&x<='9');})) throw std::runtime_error("Invalid style property");
                    const bool color=key.find("color")!=std::string::npos && key.find("opa")==std::string::npos && key!="color_filter_opa";
                    call(color?"ui_style_color":"ui_style_num",o,"LV_STYLE_"+upper(key)+", "+number(val)+", "+sel);
                }
            }
        }
        auto& sc=section(b,"scroll");
        for(auto p:{std::make_pair("enabled","scrollable"),{"direction","scroll_dir"},{"scrollbar","scrollbar_mode"},{"snapX","scroll_snap_x"},{"snapY","scroll_snap_y"},{"elastic","scroll_elastic"},{"momentum","scroll_momentum"},{"chainHorizontal","scroll_chain_hor"},{"chainVertical","scroll_chain_ver"}}) prop(sc,p.first,"lv_obj_set_"+std::string(p.second),o);
        if(sc.contains("x")||sc.contains("y")) call("lv_obj_scroll_to",o,std::to_string(sc.value("x",0))+", "+std::to_string(sc.value("y",0))+", LV_ANIM_OFF");
        auto& flags=section(b,"flags");for(auto it=flags.begin();it!=flags.end();++it) {
            static const std::set<std::string> allowed={"hidden","clickable","click_focusable","checkable","scrollable","scroll_elastic","scroll_momentum","scroll_one","scroll_chain_hor","scroll_chain_ver","scroll_on_focus","scroll_with_arrow","snappable","press_lock","event_bubble","gesture_bubble","adv_hittest","ignore_layout","floating","send_draw_task_events","overflow_visible","event_trickle","state_trickle","radio_button","flex_in_new_track","checked","disabled","state_user_1","state_user_2","state_user_3","state_user_4"};
            if(!allowed.count(it.key())) throw std::runtime_error("Unknown object flag: "+it.key());call("lv_obj_set_"+it.key(),o,number(it.value()));
        }
    }
    void gradient(const std::string& o,const json& d,const std::string& sel);
    std::string object(const json& r,const std::string& parent,const std::string& requested="");
    void children(const json& d,const char* key,const std::string& parent) {if(d.contains(key)) for(const auto& r:d.at(key)) object(r,parent);}
    void matrix(const std::string& o,const json& d);
    void custom(const std::string& o,const std::string& type,const json& d);
    void variables(const json& records);
    void interactions(const json& project);
    void screens(const json& project);
    void focusOwner(const std::string& object,const json& base) {
        const auto& accessibility=section(base,"accessibility");
        if(!accessibility.contains("tabIndex"))return;
        const auto& value=accessibility.at("tabIndex");
        if(!value.is_number_integer()||value.get<int64_t>() < -1||value.get<int64_t>() > INT32_MAX)
            throw std::runtime_error("Accessibility tabIndex must be -1 or a nonnegative 32-bit integer");
        focusOwners.emplace_back(object,value.get<int32_t>());
    }
    std::string focusCode() const;
    std::map<std::string,std::string> files(const json& project);
};
}

namespace {
std::string Generator::font(const json& record) {
    if(record.is_null() || (record.is_string()&&record=="default")) return "LV_FONT_DEFAULT";
    const auto key=(record.contains("asset")?json{{"asset",record.at("asset")}}:record).dump();if(fontCache.count(key)) return "&"+fontCache[key];
    const auto& d=section(record,"dsc");if(!d.contains("bitmap")) throw std::runtime_error("Font has no embedded bitmap; save a bitmap font before exporting");
    auto name=unique("font");fontCache[key]=name;
    auto bitmap=bytes(base64_decode(d.at("bitmap")));
    auto glyphs=unique("glyphs");assets<<"static const lv_font_fmt_txt_glyph_dsc_t "<<glyphs<<"[] = {\n";
    for(const auto& g:d.at("GlyphDsc")) assets<<"{.bitmap_index="<<g.value("bitmapIdx",0u)<<",.adv_w="<<g.value("adv_w",0)<<",.box_w="<<g.value("box_w",0)<<",.box_h="<<g.value("box_h",0)<<",.ofs_x="<<g.value("ofs_x",0)<<",.ofs_y="<<g.value("ofs_y",0)<<"},\n";
    assets<<"};\n";
    std::ostringstream maps;for(const auto& c:d.at("cmaps")) {
        int type=c.value("type",0);if(c.value("formatVersion",6)==6) {const int types[]={2,0,3,1};if(type>=0&&type<4)type=types[type];}
        auto unicode=c.value("unicodeList",json::array()),offsets=c.value("glyphIDOffsetList",json::array());
        auto un=unicode.empty()?"NULL":array(unicode,"uint16_t"),off=offsets.empty()?"NULL":array(offsets,type==0?"uint8_t":"uint16_t");
        maps<<"{.range_start="<<c.value("rangeStart",0u)<<",.range_length="<<c.value("rangeLength",0)<<",.glyph_id_start="<<c.value("glyphIDstart",0)<<",.unicode_list="<<un<<",.glyph_id_ofs_list="<<off<<",.list_length="<<unicode.size()<<",.type="<<type<<"},\n";
    }
    auto cmaps=unique("cmaps");assets<<"static const lv_font_fmt_txt_cmap_t "<<cmaps<<"[] = {\n"<<maps.str()<<"};\n";
    std::string kern="NULL";const auto& k=section(d,"Kern");if(!k.empty()) {
        auto values=array(k.at("Values"),"int8_t");auto kn=unique("kern");kern="&"+kn;
        if(d.value("kernClasses",0)) {
            auto left=array(k.at("leftClasses"),"uint8_t"),right=array(k.at("rightClasses"),"uint8_t");
            assets<<"static const lv_font_fmt_txt_kern_classes_t "<<kn<<" = {.class_pair_values="<<values<<",.left_class_mapping="<<left<<",.right_class_mapping="<<right<<",.left_class_cnt="<<k.value("leftCount",0)<<",.right_class_cnt="<<k.value("rightCount",0)<<"};\n";
        } else {
            auto ids=array(k.at("GlyphIDs"),k.value("glyphIDSize",0)?"uint16_t":"uint8_t");
            assets<<"static const lv_font_fmt_txt_kern_pair_t "<<kn<<" = {.glyph_ids="<<ids<<",.values="<<values<<",.pair_cnt="<<k.at("Values").size()<<",.glyph_ids_size="<<k.value("glyphIDSize",0)<<"};\n";
        }
    }
    assets<<"static const lv_font_fmt_txt_dsc_t "<<name<<"_dsc = {.glyph_bitmap="<<bitmap<<",.glyph_dsc="<<glyphs<<",.cmaps="<<cmaps<<",.kern_dsc="<<kern<<",.kern_scale="<<d.value("kernScale",0)<<",.cmap_num="<<d.at("cmaps").size()<<",.bpp="<<d.value("bpp",4)<<",.kern_classes="<<d.value("kernClasses",0)<<",.bitmap_format="<<d.value("bitmapFmt",0)<<",.stride="<<d.value("stride",0)<<"};\n";
    assets<<"const lv_font_t "<<name<<" = {.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height="<<record.value("lineHeight",16)<<",.base_line="<<record.value("baseLine",0)<<",.cap_height="<<record.value("capHeight",0)<<",.x_height="<<record.value("xHeight",0)<<",.subpx="<<record.value("subpx",0)<<",.kerning="<<record.value("kerning",0)<<",.underline_position="<<record.value("underlinePosition",0)<<",.underline_thickness="<<record.value("underlineThickness",0)<<",.static_bitmap="<<(d.value("bitmapFmt",0)==0?1:0)<<",.dsc=&"<<name<<"_dsc};\n";
    assetHeader<<"extern const lv_font_t "<<name<<";\n";return "&"+name;
}
void Generator::gradient(const std::string& o,const json& d,const std::string& sel) {
    auto name=unique("gradient");globals<<"static lv_grad_dsc_t "<<name<<";\n";
    line("memset(&"+name+", 0, sizeof "+name+");");
    line(name+".dir = "+std::to_string(d.value("direction",0))+"; "+name+".extend = "+std::to_string(d.value("extend",0))+";");
    size_t i=0;for(const auto& s:d.at("stops")) {
        const auto p=name+".stops["+std::to_string(i++)+"]";
        line(p+".color = lv_color_hex("+number(s.at("color"))+"); "+p+".opa = "+number(s.at("opacity"))+"; "+p+".frac = "+number(s.at("fraction"))+";");
    }
    line(name+".stops_count = "+std::to_string(i)+";");
    if(d.contains("points")) {
        requirements.insert("DRAW_SW_COMPLEX_GRADIENTS");
        std::vector<std::string> fields;
        int dir=d.value("direction",0);if(dir==LV_GRAD_DIR_LINEAR) fields={"linear.start","linear.end"};
        else if(dir==LV_GRAD_DIR_RADIAL)fields={"radial.focal","radial.focal_extent","radial.end","radial.end_extent"};
        else if(dir==LV_GRAD_DIR_CONICAL)fields={"conical.center"};
        for(size_t p=0;p<fields.size();++p) for(size_t xy=0;xy<2;++xy)line(name+".params."+fields[p]+(xy?".y = ":".x = ")+number(d.at("points").at(p).at(xy))+";");
        if(dir==LV_GRAD_DIR_CONICAL) {line(name+".params.conical.start_angle = "+std::to_string(d.value("startAngle",0))+";");line(name+".params.conical.end_angle = "+std::to_string(d.value("endAngle",360))+";");}
    }
    globals<<"#if LV_GRADIENT_MAX_STOPS < "<<i<<"\n#error Increase LV_GRADIENT_MAX_STOPS for this project\n#endif\n";
    call("lv_obj_set_style_bg_grad",o,"&"+name+", "+sel);
}
void Generator::matrix(const std::string& o,const json& d) {
    if(d.contains("btnMap")) {
        auto name=unique("map");globals<<"static const char *const "<<name<<"[] = {";
        for(const auto& item:d.at("btnMap")) if(item.is_string()&&!item.get<std::string>().empty())globals<<quote(item)<<',';
        globals<<"\"\"};\n";call("lv_buttonmatrix_set_map",o,name);
    }
    if(d.contains("nativeCtrl")) {size_t i=0;for(const auto& ctrl:d.at("nativeCtrl")) {
        call("lv_buttonmatrix_clear_button_ctrl",o,std::to_string(i)+", (lv_buttonmatrix_ctrl_t)0xffff");
        call("lv_buttonmatrix_set_button_ctrl",o,std::to_string(i++)+", (lv_buttonmatrix_ctrl_t)"+number(ctrl));
    }}
    else if(d.contains("ctrl")) {size_t i=0;for(const auto& ctrl:d.at("ctrl")) {const auto legacy=ctrl.get<uint32_t>();const auto native=(legacy&7u)|((legacy&0x1f8u)<<1);call("lv_buttonmatrix_clear_button_ctrl",o,std::to_string(i)+", (lv_buttonmatrix_ctrl_t)0xffff");call("lv_buttonmatrix_set_button_ctrl",o,std::to_string(i++)+", (lv_buttonmatrix_ctrl_t)"+std::to_string(native));}}
    if(d.contains("recolor")&&!d.contains("nativeCtrl"))call(d.value("recolor",false)?"lv_buttonmatrix_set_button_ctrl_all":"lv_buttonmatrix_clear_button_ctrl_all",o,"LV_BUTTONMATRIX_CTRL_RECOLOR");
    prop(d,"oneToggle","lv_buttonmatrix_set_one_checked",o);prop(d,"selected","lv_buttonmatrix_set_selected_button",o);
    if(d.contains("btnStyles")) {const char* selectors[]={"LV_PART_ITEMS","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_PRESSED","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_DISABLED"};for(size_t i=0;i<std::min(size_t(5),d["btnStyles"].size());++i)legacyStyle(o,d["btnStyles"][i],selectors[i]);}
}

std::string Generator::object(const json& r,const std::string& parent,const std::string& requested) {
    const auto t=kind(r);const auto& d=section(r,kinds.at(t).c_str());auto o=requested.empty()?unique("object"):requested;
    if(requested.empty()) globals<<"static lv_obj_t *"<<o<<";\n";
    focusOwner(o,section(r,"base"));
    static const std::map<std::string,std::string> ctor={{"buttonMatrix","buttonmatrix"},{"messagebox","msgbox"},{"textBox","obj"},{"layerView","obj"},{"window","obj"},{"page","obj"},{"object","obj"},{"advancedKeyboard","advanced_keyboard"}};
    const auto native=ctor.count(t)?ctor.at(t):t;
    if(native!="obj" && t!="advancedKeyboard")requirements.insert(upper(native));
    const auto cc=r.value("contentClass",std::string{});
    if(cc=="messageboxHeaderButton"||cc=="messageboxFooterButton") {
        std::string owner="lv_obj_get_parent("+parent+")";
        const bool close=r.value("close",false)&&cc=="messageboxHeaderButton";
        line(o+" = "+(cc=="messageboxFooterButton"?"lv_msgbox_add_footer_button(":close?"lv_msgbox_add_close_button(":"lv_msgbox_add_header_button(")+owner+(close?")":" , NULL)")+";");
        call("lv_obj_clean",o,"");
    } else line(o+" = lv_"+native+"_create("+parent+");");
    line("ui_track_object("+o+", &"+o+");");
    if(t=="textarea")textTargets.emplace_back(authoringScreen,o);
    if(t=="advancedKeyboard") {
        usesAdvancedKeyboard=true;
        requirements.insert("BUTTON");requirements.insert("LABEL");requirements.insert("TEXTAREA");
        const auto settings=Serialization::AdvancedKeyboard::Normalize(d);
        line("if (!"+o+") { ui_deinit(); return false; }");
        base(o,section(r,"base"));
        line("if (!lv_advanced_keyboard_set_language("+o+", "+quote(settings.at("language").get<std::string>())+")) { ui_deinit(); return false; }");
        prop(settings,"keyRadius","lv_advanced_keyboard_set_key_radius",o);
        prop(settings,"hideOnDefocus","lv_advanced_keyboard_set_hide_on_defocus",o);
        prop(settings,"autocorrect","lv_advanced_keyboard_set_autocorrect",o);
        prop(settings,"autocorrectThreshold","lv_advanced_keyboard_set_autocorrect_threshold",o);
        for(const auto* tone:{"Background","Foreground"})
            call(std::string("lv_advanced_keyboard_set_key_")+(std::string(tone)=="Background"?"background":"foreground"),o,
                "lv_color_hex("+std::to_string(settings.at(std::string("key")+tone).get<uint32_t>())+")");
        const auto trieId=settings.value("trieId",std::string("builtin-en-us"));
        if(trieId.empty())call("lv_advanced_keyboard_set_predictor",o,"NULL");
        else {
            usesPrediction=true;
            if(trieId!="builtin-en-us") {
                const auto found=trieAssets.find(trieId);if(found==trieAssets.end())throw std::runtime_error("Unknown prediction trie: "+trieId);
                const auto predictor=unique("predictor");globals<<"static lv_advanced_keyboard_predictor_t *"<<predictor<<";\n";
                const auto& trie=found->second;
                line(predictor+" = lv_advanced_keyboard_predictor_create_borrowed("+trie.data+", "+std::to_string(trie.dataSize)+", "+trie.index+", "+std::to_string(trie.indexSize)+");");
                line("if (!"+predictor+") { ui_deinit(); return false; }");call("lv_advanced_keyboard_set_predictor",o,predictor);
                keyboardFree<<"    lv_advanced_keyboard_predictor_destroy("<<predictor<<"); "<<predictor<<"=NULL;\n";
            }
        }
        keyboards.push_back({o,authoringScreen,settings});
        children(r,"contentChildren",o);
        return o;
    } else if(t=="barcode" || t=="qrcode") {
        requirements.insert("CANVAS");requirements.insert("IMAGE");
        const auto code=Serialization::CodeWidgets::Normalize(t,d);
        const auto& content=code.at("data").get_ref<const std::string&>();
        base(o,section(r,"base"));
        call("lv_"+t+"_set_update_mode",o,t=="barcode"?"LV_BARCODE_UPDATE_MODE_DEFERRED":"LV_QRCODE_UPDATE_MODE_DEFERRED");
        for(const auto* tone:{"dark","light"})call("lv_"+t+"_set_"+tone+"_color",o,"lv_color_hex("+std::to_string(code.at(std::string(tone)+"Color").get<uint32_t>())+")");
        if(t=="barcode") {
            call("lv_barcode_set_encoding",o,"(lv_barcode_encoding_t)"+number(code.at("encoding")));
            prop(code,"scale","lv_barcode_set_scale",o);prop(code,"direction","lv_barcode_set_direction",o);prop(code,"tiled","lv_barcode_set_tiled",o);
            line("if (lv_barcode_update("+o+", "+quote(content)+") != LV_RESULT_OK || lv_barcode_render("+o+") != LV_RESULT_OK) { ui_deinit(); return false; }");
        } else {
            prop(code,"size","lv_qrcode_set_size",o);prop(code,"quietZone","lv_qrcode_set_quiet_zone",o);
            line("if (lv_qrcode_update("+o+", "+quote(content)+", "+std::to_string(content.size())+") != LV_RESULT_OK) { ui_deinit(); return false; }");
        }
        call("lv_"+t+"_set_update_mode",o,t=="barcode"?"LV_BARCODE_UPDATE_MODE_IMMEDIATE":"LV_QRCODE_UPDATE_MODE_IMMEDIATE");
        children(r,"contentChildren",o);return o;
    } else if(t=="arc") {
        call("lv_obj_remove_style",o,"NULL, LV_PART_KNOB | LV_STATE_ANY");call("lv_obj_set_style_arc_opa",o,"0, LV_PART_INDICATOR");
        if(d.contains("min")&&d.contains("max"))call("lv_arc_set_range",o,number(d["min"])+", "+number(d["max"]));
        if(d.contains("mode"))line("if (lv_arc_get_mode("+o+") != "+number(d["mode"])+") lv_arc_set_mode("+o+", "+number(d["mode"])+");");
        if(d.contains("value")&&d["value"]!=INT16_MIN)prop(d,"value","lv_arc_set_value",o);
        for(auto p:{std::make_pair("rotation","rotation"),{"knobOffset","knob_offset"},{"changeRate","change_rate"}})prop(d,p.first,"lv_arc_set_"+std::string(p.second),o);
        if(d.contains("angle"))call("lv_arc_set_bg_angles",o,number(d["angle"]["start"])+", "+number(d["angle"]["end"]));
        if(d.contains("indicatorStart")&&d.contains("indicatorEnd"))call("lv_arc_set_angles",o,number(d["indicatorStart"])+", "+number(d["indicatorEnd"]));
        auto& s=section(d,"style");for(auto p:{"rounded","width","color","opa"})if(s.contains("line."+std::string(p)))call("lv_obj_set_style_arc_"+std::string(p),o,(std::string(p)=="color"?"lv_color_hex("+number(s["line."+std::string(p)])+")":number(s["line."+std::string(p)]))+", LV_PART_MAIN");
    } else if(t=="bar"||t=="slider") {
        const auto& range=t=="bar"?section(d,"range"):d;
        if(range.contains("min")&&range.contains("max"))call("lv_"+t+"_set_range",o,number(range["min"])+", "+number(range["max"]));
        prop(d,"mode","lv_"+t+"_set_mode",o);prop(d,"orientation","lv_"+t+"_set_orientation",o);
        prop(d,"value","lv_"+t+"_set_value",o,", LV_ANIM_OFF");
        prop(d,t=="bar"?"startValue":"leftValue",t=="bar"?"lv_bar_set_start_value":"lv_slider_set_left_value",o,", LV_ANIM_OFF");
        if(t=="bar"){prop(d,"animT","lv_obj_set_style_anim_duration",o,", LV_PART_MAIN");legacyStyle(o,section(d,"styleBG"));legacyStyle(o,section(d,"styleInd"),"LV_PART_INDICATOR");}
    } else if(t=="button") {
        prop(d,"toggle","lv_obj_set_checkable",o);
        if(d.contains("text") && (!r.contains("contentChildren") || r["contentChildren"].empty())) {auto label=object({{"base",json::object()},{"text",d["text"]}},o);call("lv_obj_center",label,"");}
        const int layout=d.value("layout",0);if(layout) {requirements.insert("FLEX");call("lv_obj_set_flex_flow",o,layout>=2&&layout<=4?"LV_FLEX_FLOW_COLUMN":layout>=8?"LV_FLEX_FLOW_ROW_WRAP":"LV_FLEX_FLOW_ROW");call("lv_obj_set_flex_align",o,std::string(layout==1?"LV_FLEX_ALIGN_CENTER":"LV_FLEX_ALIGN_START")+", "+(layout==1||layout==3||layout==6?"LV_FLEX_ALIGN_CENTER":layout==4||layout==7?"LV_FLEX_ALIGN_END":"LV_FLEX_ALIGN_START")+", LV_FLEX_ALIGN_START");}
        for(auto s:{std::make_pair("rel","0"),{"pr","LV_STATE_PRESSED"},{"ina","LV_STATE_DISABLED"},{"tglRel","LV_STATE_CHECKED"},{"tglPr","LV_STATE_CHECKED | LV_STATE_PRESSED"}})legacyStyle(o,section(section(d,"styles"),s.first),s.second);
    } else if(t=="label") {
        text(r,"text","lv_label_set_text",o);
        if(r.contains("longmode")){const char* modes[]={"LV_LABEL_LONG_MODE_WRAP","LV_LABEL_LONG_MODE_WRAP","LV_LABEL_LONG_MODE_DOTS","LV_LABEL_LONG_MODE_SCROLL","LV_LABEL_LONG_MODE_SCROLL_CIRCULAR","LV_LABEL_LONG_MODE_CLIP"};const auto mode=r["longmode"].get<int>();if(mode<0||mode>5)throw std::runtime_error("Invalid label long mode");call("lv_label_set_long_mode",o,modes[mode]);}
        if(!Serialization::HasNativeStyleSnapshot(section(r,"base")) && r.contains("textalign")){const char* align[]={"LV_TEXT_ALIGN_LEFT","LV_TEXT_ALIGN_CENTER","LV_TEXT_ALIGN_RIGHT"};const auto index=r["textalign"].get<int>();if(index<0||index>2)throw std::runtime_error("Invalid label alignment");call("lv_obj_set_style_text_align",o,std::string(align[index])+", LV_PART_MAIN");}
        if(!Serialization::HasNativeStyleSnapshot(section(r,"base")) && r.contains("drawback"))call("lv_obj_set_style_bg_opa",o,std::string(r.value("drawback",false)?"LV_OPA_COVER":"LV_OPA_TRANSP")+", LV_PART_MAIN");
    } else if(t=="checkbox"||t=="switch") {
        if(t=="checkbox")text(d,"text","lv_checkbox_set_text",o);
        prop(d,"checked","lv_obj_set_checked",o);prop(d,"inact","lv_obj_set_disabled",o);
        if(t=="checkbox")for(auto s:{std::make_pair("bg","LV_PART_MAIN"),{"rel","LV_PART_INDICATOR"},{"pr","LV_PART_INDICATOR | LV_STATE_PRESSED"},{"tglRel","LV_PART_INDICATOR | LV_STATE_CHECKED"},{"tglPr","LV_PART_INDICATOR | LV_STATE_CHECKED | LV_STATE_PRESSED"},{"ina","LV_PART_INDICATOR | LV_STATE_DISABLED"}})legacyStyle(o,section(section(d,"style"),s.first),s.second);
    } else if(t=="buttonMatrix")matrix(o,d);
    else if(t=="keyboard") {prop(d,"mode","lv_keyboard_set_mode",o);prop(d,"popovers","lv_keyboard_set_popovers",o);matrix(o,section(d,"buttons"));}
    else if(t=="dropdown") {
        text(d,"options","lv_dropdown_set_options",o);prop(d,"selected","lv_dropdown_set_selected",o);prop(d,"direction","lv_dropdown_set_dir",o);text(d,"text","lv_dropdown_set_text",o);text(d,"symbol","lv_dropdown_set_symbol",o);prop(d,"highlight","lv_dropdown_set_selected_highlight",o);
    } else if(t=="roller") {
        call("lv_roller_set_options",o,quote(d.value("options",std::string{}))+", "+std::to_string(d.value("mode",0)));prop(d,"visibleRows","lv_roller_set_visible_row_count",o);prop(d,"selected","lv_roller_set_selected",o,", LV_ANIM_OFF");
    } else if(t=="spinbox") {
        call("lv_spinbox_set_range",o,std::to_string(d.value("min",-99999))+", "+std::to_string(d.value("max",99999)));
        call("lv_spinbox_set_digit_format",o,std::to_string(d.value("digits",5))+", "+std::to_string(d.value("decimalPlaces",0)));
        for(auto p:{"step","value","rollover"})prop(d,p,"lv_spinbox_set_"+std::string(p),o);
        prop(d,"stepDirection","lv_spinbox_set_digit_step_direction",o);
    } else if(t=="textarea") {
        for(auto p:{std::make_pair("accepted","accepted_chars"),{"passwordBullet","password_bullet"},{"placeholder","placeholder_text"}})text(d,p.first,"lv_textarea_set_"+std::string(p.second),o);
        for(auto p:{std::make_pair("maxLength","max_length"),{"oneLine","one_line"},{"password","password_mode"},{"passwordShowTime","password_show_time"},{"cursorClick","cursor_click_pos"},{"selection","text_selection"}})prop(d,p.first,"lv_textarea_set_"+std::string(p.second),o);
        text(d,"text","lv_textarea_set_text",o);prop(d,"cursor","lv_textarea_set_cursor_pos",o);
    } else if(t=="led") {
        if(d.contains("color"))call("lv_led_set_color",o,"lv_color_hex("+number(d["color"])+")");prop(d,"brightness","lv_led_set_brightness",o);
    } else if(t=="line") {
        if(d.contains("points")&&!d["points"].empty()) {auto name=unique("points");globals<<"static const lv_point_precise_t "<<name<<"[] = {";for(auto& p:d["points"])globals<<'{'<<number(p[0])<<','<<number(p[1])<<"},";globals<<"};\n";call("lv_line_set_points",o,name+", "+std::to_string(d["points"].size()));}
        prop(d,"invertY","lv_line_set_y_invert",o);
    } else if(t=="spinner")call("lv_spinner_set_anim_params",o,std::to_string(d.value("duration",1000))+", "+std::to_string(d.value("sweep",60)));
    else if(t=="image") {
        Image im;if(d.contains("asset"))im=image(d);else if(d.contains("encodedSource"))im=image(d["encodedSource"]);else if(d.value("srcType",-1)==2)im.expression=quote(d.value("sym",std::string{}));else if(d.value("srcType",-1)==1||d.contains("data"))im=image(d);
        if(im.expression!="NULL")call("lv_image_set_src",o,im.expression);animate(im,o);
        for(auto p:{std::make_pair("offX","offset_x"),{"offY","offset_y"},{"rotation","rotation"},{"scaleX","scale_x"},{"scaleY","scale_y"},{"antialias","antialias"},{"blendMode","blend_mode"},{"innerAlign","inner_align"}})prop(d,p.first,"lv_image_set_"+std::string(p.second),o);
        if(d.contains("pivot"))call("lv_image_set_pivot",o,number(d["pivot"]["x"])+", "+number(d["pivot"]["y"]));legacyStyle(o,section(d,"style"));
    } else if(t=="canvas") {
        if(d.contains("data")||d.contains("width")) {
            const auto w=d.value("width",d.value("header.w",96u)),h=d.value("height",d.value("header.h",64u));
            const auto cf=d.value("format",d.value("cf",int(LV_COLOR_FORMAT_ARGB8888)));const auto stride=d.value("stride",(w*lv_color_format_get_bpp(static_cast<lv_color_format_t>(cf))+7)/8);
            const auto minimum=(uint64_t(w)*lv_color_format_get_bpp(static_cast<lv_color_format_t>(cf))+7)/8;
            const auto expected=uint64_t(stride)*h+LV_COLOR_INDEXED_PALETTE_SIZE(cf)*4;
            if(!w||!h||w>65535||h>65535||!minimum||stride<minimum||expected>UINT32_MAX)throw std::runtime_error("Invalid canvas buffer dimensions");
            auto raw=d.contains("data")?base64_decode(d["data"]):std::string(size_t(expected),'\0');
            if(raw.size()!=expected)throw std::runtime_error("Invalid canvas pixel data length");
            auto name=unique("canvas_pixels");globals<<"static LV_ATTRIBUTE_MEM_ALIGN uint8_t "<<name<<'['<<raw.size()<<"] = {";for(unsigned char c:raw)globals<<unsigned(c)<<',';globals<<"};\n";
            auto buf=unique("canvas_buffer");globals<<"static lv_draw_buf_t "<<buf<<";\n";
            line("lv_draw_buf_init(&"+buf+", "+std::to_string(w)+", "+std::to_string(h)+", "+std::to_string(cf)+", "+std::to_string(stride)+", "+name+", sizeof "+name+");");call("lv_canvas_set_draw_buf",o,"&"+buf);
        }
        auto& off=section(d,"offset");prop(off,"x","lv_image_set_offset_x",o);prop(off,"y","lv_image_set_offset_y",o);
    } else if(t=="calendar") {
        requirements.insert("CALENDAR_HEADER_ARROW");
        line("lv_calendar_add_header_arrow("+o+");");
        auto& today=section(d,"today");if(!today.empty())call("lv_calendar_set_today_date",o,number(today.at("year"))+", "+number(today.at("month"))+", "+number(today.at("day")));
        auto& shown=section(d,"shown");if(!shown.empty())call("lv_calendar_set_showed_date",o,number(shown.at("year"))+", "+number(shown.at("month")));
        if(d.contains("hlDates")&&!d["hlDates"].empty()) {auto name=unique("dates");globals<<"static lv_calendar_date_t "<<name<<"[] = {";for(auto& date:d["hlDates"])globals<<'{'<<date.at("year")<<','<<date.at("month")<<','<<date.at("day")<<"},";globals<<"};\n";call("lv_calendar_set_highlighted_dates",o,name+", "+std::to_string(d["hlDates"].size()));}
        if(d.contains("dayN")) {auto name=unique("days");globals<<"static const char *"<<name<<"[] = {";for(auto& day:d["dayN"])globals<<quote(day)<<',';globals<<"};\n";call("lv_calendar_set_day_names",o,name);}
        if(d.contains("monthN")) {
            if(d["monthN"].size()!=12)throw std::runtime_error("Calendar requires twelve month names");
            auto name=unique("months");globals<<"static const char *const "<<name<<"[] = {";for(auto& month:d["monthN"])globals<<quote(month)<<',';globals<<"};\n";
            globals<<"static void "<<name<<"_update(lv_event_t *event) { lv_obj_t *calendar=lv_event_get_target_obj(event); const lv_calendar_date_t *date=lv_calendar_get_showed_date(calendar); if (date && date->month>=1 && date->month<=12 && ui_calendar_month_label(calendar)) lv_label_set_text_fmt(ui_calendar_month_label(calendar), \"%u %s\", date->year, "<<name<<"[date->month-1]); }\n";
            call("lv_obj_add_event_cb",o,name+"_update, LV_EVENT_VALUE_CHANGED, NULL");call("lv_obj_send_event",o,"LV_EVENT_VALUE_CHANGED, NULL");
        }
        legacyStyle(o,section(d,"styleBG"));
        const auto head="ui_calendar_header("+o+")",dates="lv_calendar_get_btnmatrix("+o+")";
        legacyStyle(head,section(d,"styleHead"));legacyStyle(head,section(d,"styleHeadPR"),"LV_STATE_PRESSED");
        for(auto p:{std::make_pair("styleDayName","LV_PART_ITEMS"),{"styleHDay","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED"},{"styleInaDay","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_DISABLED"},{"weekBox","LV_PART_MAIN"},{"todayBox","(lv_style_selector_t)LV_PART_ITEMS | LV_STATE_FOCUSED"}})legacyStyle(dates,section(d,p.first),p.second);
    } else if(t=="chart") {
        const auto count=d.value("pointCount",10u);if(count<1||count>65535)throw std::runtime_error("Invalid chart point count");
        prop(d,"type","lv_chart_set_type",o);prop(d,"pointCount","lv_chart_set_point_count",o);prop(d,"updateMode","lv_chart_set_update_mode",o);
        call("lv_chart_set_div_line_count",o,std::to_string(d.value("horizontalDivisions",3))+", "+std::to_string(d.value("verticalDivisions",5)));
        if(d.contains("axes"))for(auto& a:d["axes"])call("lv_chart_set_axis_range",o,number(a.at("axis"))+", "+number(a.at("min"))+", "+number(a.at("max")));
        std::vector<std::string> series;if(d.contains("series"))for(auto& s:d["series"]) {
            auto sn=unique("series");series.push_back(sn);globals<<"static lv_chart_series_t *"<<sn<<";\n";
            line(sn+" = lv_chart_add_series("+o+", lv_color_hex("+std::to_string(s.value("color",0xffffffu))+"), "+std::to_string(s.value("axis",0))+");");
            prop(s,"hidden","lv_chart_hide_series",o+", "+sn);prop(s,"start","lv_chart_set_x_start_point",o+", "+sn);
            for(auto axis:{"x","y"})if(s.contains(axis))for(size_t i=0;i<std::min(size_t(count),s[axis].size());++i)line("if (lv_chart_get_series_"+std::string(axis)+"_array("+o+", "+sn+")) lv_chart_get_series_"+axis+"_array("+o+", "+sn+")["+std::to_string(i)+"] = "+number(s[axis][i])+";");
        }
        if(d.contains("cursors"))for(auto it=d["cursors"].rbegin();it!=d["cursors"].rend();++it) {
            auto cn=unique("cursor");line("lv_chart_cursor_t *"+cn+" = lv_chart_add_cursor("+o+", lv_color_hex("+std::to_string(it->value("color",0xffffffu))+"), "+std::to_string(it->value("directions",15))+");");
            int si=it->value("series",-1);if(!it->value("positionSet",true)&&si>=0&&size_t(si)<series.size())call("lv_chart_set_cursor_point",o,cn+", "+series[si]+", "+std::to_string(it->value("point",0)));
            else {auto point=unique("point");line("lv_point_t "+point+" = {"+std::to_string(it->value("x",0))+", "+std::to_string(it->value("y",0))+"};");call("lv_chart_set_cursor_pos",o,cn+", &"+point);}
        }
    } else if(t=="table") {
        if(d.contains("columns")){call("lv_table_set_column_count",o,std::to_string(d["columns"].size()));for(size_t i=0;i<d["columns"].size();++i)call("lv_table_set_column_width",o,std::to_string(i)+", "+number(d["columns"][i]));}
        if(d.contains("rows")){call("lv_table_set_row_count",o,std::to_string(d["rows"].size()));for(size_t i=0;i<d["rows"].size();++i)for(size_t j=0;j<d["rows"][i].size();++j){const auto& cell=d["rows"][i][j];auto pos=std::to_string(i)+", "+std::to_string(j);call("lv_table_set_cell_value",o,pos+", "+quote(cell.value("text",std::string{})));call("lv_table_set_cell_ctrl",o,pos+", "+std::to_string(cell.value("flags",0)));}}
        call("lv_table_set_selected_cell",o,std::to_string(d.value("selectedRow",0xffffu))+", "+std::to_string(d.value("selectedColumn",0xffffu)));
    } else if(t=="scale") {
        prop(d,"mode","lv_scale_set_mode",o);call("lv_scale_set_range",o,std::to_string(d.value("min",0))+", "+std::to_string(d.value("max",100)));
        for(auto p:{std::make_pair("ticks","total_tick_count"),{"majorEvery","major_tick_every"},{"rotation","rotation"},{"angleRange","angle_range"},{"showLabels","label_show"}})prop(d,p.first,"lv_scale_set_"+std::string(p.second),o);
        children(d,"children",o);
        if(d.value("variant",std::string{})=="gauge") {requirements.insert("LINE");auto needle=object({{"base",json::object()},{"line",json::object()}},o);call("lv_obj_set_style_line_color",needle,"lv_color_hex("+std::to_string(d.value("needleColor",0xffffffu))+"), 0");call("lv_obj_set_style_line_width",needle,std::to_string(d.value("needleWidth",3))+", 0");call("lv_obj_update_layout",o,"");call("lv_scale_set_line_needle_value",o,needle+", "+std::to_string(d.value("needleLength",50))+", "+std::to_string(d.value("value",50)));}
    } else if(t=="tabview"||t=="tileview") {
        bool tabs=t=="tabview";if(tabs){prop(d,"position","lv_tabview_set_tab_bar_position",o);prop(d,"barSize","lv_tabview_set_tab_bar_size",o);base("lv_tabview_get_tab_bar("+o+")",section(d,"barBase"));base("lv_tabview_get_content("+o+")",section(d,"contentBase"));}
        const char* key=tabs?"tabs":"tiles";std::vector<std::string> panels;if(d.contains(key))for(const auto& p:d[key]) {
            auto pn=unique("panel");globals<<"static lv_obj_t *"<<pn<<";\n";panels.push_back(pn);
            line(pn+" = "+(tabs?"lv_tabview_add_tab("+o+", "+quote(p.value("name",std::string("Tab"))):"lv_tileview_add_tile("+o+", "+std::to_string(p.value("column",0))+", "+std::to_string(p.value("row",0))+", "+std::to_string(p.value("directions",15)))+");");
            base(pn,section(p,"base"));children(p,"children",pn);if(tabs)base("lv_tabview_get_tab_button("+o+", "+std::to_string(panels.size()-1)+")",section(p,"buttonBase"));
        }
        auto active=d.value("active",0u);if(active<panels.size())call(tabs?"lv_tabview_set_active":"lv_tileview_set_tile",o,(tabs?std::to_string(active):panels[active])+", LV_ANIM_OFF");
    } else if(t=="window") {
        requirements.insert("FLEX");call("lv_obj_set_flex_flow",o,"LV_FLEX_FLOW_COLUMN");
        auto h=object({{"base",d.value("headerBase",json::object())}},o);call("lv_obj_set_height",h,std::to_string(d.value("headerHeight",40)));call("lv_obj_set_flex_flow",h,"LV_FLEX_FLOW_ROW");
        auto c=object({{"base",d.value("contentBase",json::object())}},o);call("lv_obj_set_flex_grow",c,"1");children(d,"header",h);children(d,"children",c);
        if(!d.contains("header")&&d.contains("title"))object({{"base",json::object()},{"text",d["title"]}},h);
    } else if(t=="messagebox") {
        if(d.contains("headerBase")||!d.value("title",std::string{}).empty()||(d.contains("headerButtons")&&!d["headerButtons"].empty()))call("lv_msgbox_add_title",o,quote(d.value("title",std::string{})));
        base("lv_msgbox_get_header("+o+")",section(d,"headerBase"));base("lv_msgbox_get_content("+o+")",section(d,"contentBase"));children(d,"headerButtons","lv_msgbox_get_header("+o+")");children(d,"children","lv_msgbox_get_content("+o+")");
        if(d.contains("footerBase")||(d.contains("footer")&&!d["footer"].empty())) {line("lv_obj_delete(lv_msgbox_add_footer_button("+o+", NULL));");base("lv_msgbox_get_footer("+o+")",section(d,"footerBase"));children(d,"footer","lv_msgbox_get_footer("+o+")");}
    } else if(t=="imagebutton") {
        if(d.contains("states"))for(size_t state=0;state<d["states"].size()&&state<6;++state) {std::vector<Image> imgs;for(const auto& s:d["states"][state])imgs.push_back(image(s));if(imgs.size()!=3)throw std::runtime_error("Image button needs three image sources per state");call("lv_imagebutton_set_src",o,std::to_string(state)+", "+imgs[0].expression+", "+imgs[1].expression+", "+imgs[2].expression);for(int i=0;i<3;++i)animate(imgs[i],o,4,int(state),i);}
        children(d,"children",o);
    } else if(t=="page") {prop(d,"scrollDirection","lv_obj_set_scroll_dir",o);prop(d,"scrollbarMode","lv_obj_set_scrollbar_mode",o);children(d,"children",o);}
    else if(t=="textBox"||t=="layerView")custom(o,t,d);
    if(d.contains("styles"))for(auto p:{std::make_pair("Main","LV_PART_MAIN"),{"Items","LV_PART_ITEMS"},{"Indicator","LV_PART_INDICATOR"},{"Cursor","LV_PART_CURSOR"}})legacyStyle(o,section(d["styles"],p.first),p.second);
    base(o,section(r,"base"));children(r,"contentChildren",o);return o;
}
}

namespace {
void Generator::custom(const std::string& o,const std::string& t,const json& d) {
    requirements.insert("FLEX");requirements.insert("BUTTON");requirements.insert("TEXTAREA");
    auto title=object({{"base",json::object()},{"text",d.value("title",std::string(t=="textBox"?"Value":"Layer view"))}},o);
    const auto state=o+"_custom";
    if(t=="textBox") {
        globals<<"static ui_textbox_t "<<state<<";\n";
        call("lv_obj_set_flex_flow",o,"LV_FLEX_FLOW_ROW");call("lv_obj_set_flex_align",o,"LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER");
        call("lv_obj_set_style_pad_column",o,std::to_string(d.value("spacing",6))+", 0");call("lv_obj_set_width",title,std::to_string(d.value("titleWidth",64)));
        auto entry=object({{"base",json::object()},{"textarea",{{"oneLine",true},{"text",d.value("text",std::string("0"))},{"placeholder",d.value("placeholder",std::string{})},{"password",d.value("password",false)},{"maxLength",d.value("maxLength",32)}}}},o);
        call("lv_obj_set_flex_grow",entry,"1");call("lv_obj_set_height",entry,"36");
        if(!d.value("acceptedChars",std::string{}).empty())call("lv_textarea_set_accepted_chars",entry,quote(d["acceptedChars"]));
        call("lv_obj_set_disabled",entry,d.value("readOnly",false)?"true":"false");
        line(state+".root = "+o+"; "+state+".entry = "+entry+";");line("ui_track_object("+o+", &"+state+".root);");
        line(state+".minimum = "+std::to_string(d.value("min",int64_t(0)))+"; "+state+".maximum = "+std::to_string(d.value("max",int64_t(100)))+"; "+state+".step = "+std::to_string(d.value("step",int64_t(1)))+";");
        line(state+".value = "+std::to_string(d.value("value",int64_t(0)))+"; "+state+".numeric = "+(d.value("numeric",true)?"true":"false")+"; "+state+".read_only = "+(d.value("readOnly",false)?"true":"false")+";");
        call("lv_obj_add_event_cb",entry,"ui_textbox_entry, LV_EVENT_VALUE_CHANGED, &"+state);
        base(title,section(section(d,"partBases"),"Title"));base(entry,section(section(d,"partBases"),"Entry"));
        legacyStyle(title,section(section(d,"styles"),"Title"));legacyStyle(entry,section(section(d,"styles"),"Entry"));
        for(bool plus:{false,true}) {
            auto p=plus?"Plus":"Minus";
            auto button=object({{"base",json::object()},{"button",{{"text",d.value(plus?"plusText":"minusText",std::string(plus?"+":"-"))}}}},o);
            call("lv_obj_set_size",button,std::to_string(d.value("buttonWidth",28))+", 36");call("lv_obj_set_hidden",button,d.value("showButtons",true)?"false":"true");
            call("lv_obj_set_disabled",button,d.value("readOnly",false)||!d.value("numeric",true)?"true":"false");
            call("lv_obj_add_event_cb",button,std::string(plus?"ui_textbox_plus":"ui_textbox_minus")+", LV_EVENT_CLICKED, &"+state);
            legacyStyle(button,section(section(d,"styles"),p));base(button,section(section(d,"partBases"),p));
        }
    } else {
        call("lv_obj_set_flex_flow",o,"LV_FLEX_FLOW_COLUMN");call("lv_obj_set_hidden",title,d.value("showTitle",true)?"false":"true");
        auto content=object({{"base",json::object()}},o);call("lv_obj_set_width",content,"LV_PCT(100)");call("lv_obj_set_flex_grow",content,"1");call("lv_obj_set_flex_flow",content,"LV_FLEX_FLOW_COLUMN");
        auto nodes=d.value("nodes",json::array());
        std::map<uint32_t,json> byId;for(const auto& node:nodes) {auto id=node.value("id",0u);if(!id||!byId.emplace(id,node).second)throw std::runtime_error("Layer view node IDs must be unique and nonzero");}
        std::map<uint32_t,int> depths;
        for(auto& n:byId) {std::set<uint32_t> seen{n.first};auto parent=n.second.value("parent",0u);int depth=0;while(parent) {if(!byId.count(parent)||!seen.insert(parent).second)throw std::runtime_error("Invalid layer view hierarchy");++depth;parent=byId.at(parent).value("parent",0u);}depths[n.first]=depth;}
        std::vector<json> ordered;std::function<void(uint32_t)> visit=[&](uint32_t parent){for(const auto& node:nodes)if(node.value("parent",0u)==parent){ordered.push_back(node);visit(node.at("id"));}};visit(0);
        globals<<"static ui_layer_row_t "<<state<<"_rows["<<std::max(size_t(1),nodes.size())<<"];\nstatic ui_layer_t "<<state<<";\n";
        line(state+".root = "+o+"; "+state+".content = "+content+"; "+state+".rows = "+state+"_rows; "+state+".count = "+std::to_string(nodes.size())+"; "+state+".selected = "+std::to_string(d.value("selected",0u))+";");
        line("ui_track_object("+o+", &"+state+".root);");
        size_t i=0;for(const auto& node:ordered) {
            const auto id=node.at("id").get<uint32_t>();auto row=object({{"base",json::object()},{"button",json::object()}},content);
            call("lv_obj_set_width",row,"LV_PCT(100)");call("lv_obj_set_height",row,std::to_string(d.value("rowHeight",32)));call("lv_obj_set_flex_flow",row,"LV_FLEX_FLOW_ROW");
            call("lv_obj_set_style_pad_left",row,std::to_string(d.value("indent",18)*depths[id])+", 0");
            auto toggle=object({{"base",json::object()},{"button",{{"text",node.value("expanded",true)?LV_SYMBOL_DOWN:LV_SYMBOL_RIGHT}}}},row);call("lv_obj_set_size",toggle,"24, 24");
            auto caption=object({{"base",json::object()},{"text",node.value("text",std::string{})}},row);call("lv_obj_set_flex_grow",caption,"1");
            auto r=state+"_rows["+std::to_string(i++)+"]";
            line(r+".owner = &"+state+"; "+r+".button = "+row+"; "+r+".toggle = "+toggle+"; "+r+".caption = "+caption+";");
            line(r+".id = "+std::to_string(id)+"; "+r+".parent = "+std::to_string(node.value("parent",0u))+"; "+r+".expanded = "+(node.value("expanded",true)?"true":"false")+"; "+r+".disabled = "+(node.value("disabled",false)?"true":"false")+";");
            call("lv_obj_add_event_cb",row,"ui_layer_select, LV_EVENT_CLICKED, &"+r);call("lv_obj_add_event_cb",toggle,"ui_layer_toggle, LV_EVENT_CLICKED, &"+r);
            base(row,section(node,"base"));base(toggle,section(node,"toggleBase"));base(caption,section(node,"captionBase"));
        }
        line("ui_layer_refresh(&"+state+");");
        base(title,section(section(d,"partBases"),"Title"));base(content,section(section(d,"partBases"),"Content"));
        legacyStyle(title,section(section(d,"styles"),"Title"));legacyStyle(content,section(section(d,"styles"),"Content"));
        children(d,"children",content);
    }
    if(t=="textBox")children(d,"children",o);
}

void Generator::screens(const json& project) {
    auto documents=project.contains("screens")?project.at("screens"):json::array({project});
    if(!documents.is_array()||documents.empty())throw std::runtime_error("Project needs at least one screen");
    screenCount=documents.size();globals<<"static lv_obj_t *ui_screens["<<screenCount<<"];\nstatic lv_obj_t *ui_previous_screen;\nstatic bool ui_ready;\nstatic bool ui_syncing_vars;\n";
    if(project.value("activeScreen",size_t(0))>=screenCount)throw std::runtime_error("Initial screen index is out of range");
    size_t si=0;for(const auto& doc:documents) {
        authoringScreen=si;
        const auto screen="ui_screens["+std::to_string(si)+"]";
        const auto transition=doc.value("transition",json::object());
        if(!transition.is_object())throw std::runtime_error("Screen transition must be an object");
        const auto transitionType=transition.value("type",std::string("none"));
        static const std::map<std::string,std::string> transitionNames={{"none","NONE"},{"fade","FADE_IN"},{"slide_left","MOVE_LEFT"},{"slide_right","MOVE_RIGHT"},{"slide_up","MOVE_TOP"},{"slide_down","MOVE_BOTTOM"}};
        const auto transitionName=transitionNames.find(transitionType);
        if(transitionName==transitionNames.end())throw std::runtime_error("Unknown screen transition: "+transitionType);
        const auto duration=transition.value("duration",json(0));
        if(!duration.is_number_integer()||duration.get<int64_t>()<0||duration.get<int64_t>()>2000)throw std::runtime_error("Screen transition duration must be 0..2000 ms");
        screenTransitions.emplace_back(transitionName->second,duration.get<uint32_t>());
        usesTransitions|=transitionType!="none"&&duration.get<uint32_t>()!=0;
        auto canvas=doc.value("canvas",doc.value("screen",json::object()));if(canvas.empty())canvas=doc;
        const auto width=canvas.value("width",320),height=canvas.value("height",240);if(width<1||height<1)throw std::runtime_error("Invalid screen size");
        line(screen+" = lv_obj_create(NULL);");line("ui_track_object("+screen+", &"+screen+");");
        base(screen,section(canvas,"base"));
        focusOwner(screen,section(canvas,"base"));
        // Canvas offsets position the preview inside the desktop simulator, not the device screen.
        call("lv_obj_set_pos",screen,"0, 0");call("lv_obj_set_size",screen,std::to_string(width)+", "+std::to_string(height));
        manifest["screens"].push_back({{"index",si},{"name",doc.value("name",std::string("Screen ")+std::to_string(si+1))},{"width",width},{"height",height}});
        const auto records=doc.value("obj",json::array());if(!records.is_array())throw std::runtime_error("Screen objects must be an array");
        std::map<int,json> pending;int next=0;
        for(const auto& r:records) {
            auto id=r.value("oid",next++);if(id<0||!pending.emplace(id,r).second)throw std::runtime_error("Duplicate or negative widget ID");
            auto name="ui_screen_"+std::to_string(si)+"_object_"+std::to_string(id);handles[{si,id}]=name;types[{si,id}]=kind(r);globals<<"static lv_obj_t *"<<name<<";\n";
            const auto widgetType=types.at({si,id});
            auto metadata=json{{kinds.at(widgetType),widgetType=="label"?json(""):json::object()}};
            if(widgetType=="button") {
                const auto& flags=section(section(r,"base"),"flags");
                if(flags.contains("checkable"))metadata["base"]["flags"]["checkable"]=flags.at("checkable");
                const auto& settings=section(r,"button");
                if(settings.contains("toggle"))metadata["button"]["toggle"]=settings.at("toggle");
            }
            variableWidgetRecords[{si,id}]=std::move(metadata);
            manifest["objects"].push_back({{"screen",si},{"id",id},{"name",section(r,"base").value("name",std::string{})},{"type",kind(r)},{"symbol",name}});
        }
        std::set<int> created;
        while(!pending.empty()) {
            bool progress=false;for(auto it=pending.begin();it!=pending.end();) {
                const auto& r=it->second;int parent=r.value("par",-1);if(parent!=-1&&!created.count(parent)){++it;continue;}
                auto host=parent==-1?screen:handles.at({si,parent});
                if(r.contains("hostPath"))for(const auto& p:r["hostPath"]) {int index=p.get<int>();if(index<0)throw std::runtime_error("Invalid internal parent path");host="lv_obj_get_child("+host+", "+std::to_string(index)+")";}
                // A corrupt path must fail safely instead of creating an unintended screen.
                line("if (!("+host+")) { ui_deinit(); return false; }");
                auto objectName=object(r,host,handles.at({si,it->first}));
                if(r.contains("hostIndex"))call("lv_obj_move_to_index",objectName,number(r["hostIndex"]));
                created.insert(it->first);it=pending.erase(it);progress=true;
            }
            if(!progress)throw std::runtime_error("Missing parent or cycle in screen hierarchy");
        }
        call("lv_obj_update_layout",screen,"");++si;
    }
}

std::string Generator::focusCode() const {
    if(focusOwners.empty())return {};
    std::ostringstream code;
    code<<"typedef struct {lv_obj_t **object; int32_t index;} ui_focus_owner_t;\nstatic const ui_focus_owner_t ui_focus_owners[] = {\n";
    for(const auto& owner:focusOwners)code<<"    {&"<<owner.first<<", "<<owner.second<<"},\n";
    code<<R"FOCUS(};
typedef struct {lv_obj_t *object; size_t screen, natural; int32_t index;} ui_focus_entry_t;
static ui_focus_entry_t *ui_focus_entries;
static size_t ui_focus_count;
static lv_group_t *ui_focus_group;
static size_t ui_focus_collect(lv_obj_t *object, size_t screen, bool store)
{
    size_t count=0, i; uint32_t child;
    if (!object) return 0;
    if (lv_obj_get_group(object)==ui_focus_group) {
        if (store) {
            lv_obj_t *ancestor; int32_t index=INT32_MAX; bool found=false;
            for (ancestor=object; ancestor && !found; ancestor=lv_obj_get_parent(ancestor)) {
                for (i=0;i<sizeof(ui_focus_owners)/sizeof(ui_focus_owners[0]);++i) {
                    if (*ui_focus_owners[i].object==ancestor) {index=ui_focus_owners[i].index;found=true;break;}
                }
            }
            ui_focus_entries[ui_focus_count].object=object;
            ui_focus_entries[ui_focus_count].screen=screen;
            ui_focus_entries[ui_focus_count].natural=ui_focus_count;
            ui_focus_entries[ui_focus_count++].index=index;
        }
        ++count;
    }
    for (child=0;child<lv_obj_get_child_count(object);++child)
        count+=ui_focus_collect(lv_obj_get_child(object,child),screen,store);
    return count;
}
static int ui_focus_compare(const void *left, const void *right)
{
    const ui_focus_entry_t *a=(const ui_focus_entry_t*)left, *b=(const ui_focus_entry_t*)right;
    if (a->index!=b->index) return a->index < b->index ? -1 : 1;
    return a->natural==b->natural ? 0 : a->natural < b->natural ? -1 : 1;
}
static bool ui_focus_init(void)
{
    size_t i, count=0;
    if (!ui_focus_group) return true;
    for (i=0;i<sizeof(ui_screens)/sizeof(ui_screens[0]);++i) count+=ui_focus_collect(ui_screens[i],i,false);
    if (!count) return true;
    if (count>SIZE_MAX/sizeof(ui_focus_entry_t)) return false;
    ui_focus_entries=(ui_focus_entry_t*)lv_realloc(NULL,count*sizeof(ui_focus_entry_t));
    if (!ui_focus_entries) return false;
    ui_focus_count=0;
    for (i=0;i<sizeof(ui_screens)/sizeof(ui_screens[0]);++i) ui_focus_collect(ui_screens[i],i,true);
    qsort(ui_focus_entries,ui_focus_count,sizeof(ui_focus_entry_t),ui_focus_compare);
    /* Slot addresses are stable only after sorting. */
    for (i=0;i<ui_focus_count;++i) ui_track_object(ui_focus_entries[i].object,&ui_focus_entries[i].object);
    return true;
}
static void ui_focus_activate(size_t screen)
{
    size_t i; lv_obj_t *focused, *first=NULL;
    if (!ui_focus_group) return;
    focused=lv_group_get_focused(ui_focus_group);
    for (i=0;i<ui_focus_count;++i) {
        lv_obj_t *object=ui_focus_entries[i].object;
        if (object && lv_obj_get_group(object)==ui_focus_group) lv_group_remove_obj(object);
    }
    for (i=0;i<ui_focus_count;++i) {
        lv_obj_t *object=ui_focus_entries[i].object;
        if (!object || ui_focus_entries[i].screen!=screen || ui_focus_entries[i].index<0) continue;
        lv_group_add_obj(ui_focus_group,object);
        if (!first) first=object;
    }
    if (focused && lv_obj_get_group(focused)==ui_focus_group) lv_group_focus_obj(focused);
    else if (first) lv_group_focus_obj(first);
}
static void ui_focus_free(void)
{
    size_t i;
    for (i=0;i<ui_focus_count;++i) if (ui_focus_entries[i].object)
        lv_obj_remove_event_cb_with_user_data(ui_focus_entries[i].object,ui_forget_object,&ui_focus_entries[i].object);
    lv_free(ui_focus_entries); ui_focus_entries=NULL; ui_focus_count=0; ui_focus_group=NULL;
}
)FOCUS";
    return code.str();
}

void Generator::variables(const json& records) {
    if(!records.is_array())throw std::runtime_error("Variables must be an array");
    const auto normalized=Serialization::LVVariable::NormalizeList(records);
    const auto identifiers=variableIdentifiers(normalized);std::ostringstream fields;
    std::vector<std::string> variableNames;
    size_t index=0;for(const auto& record:normalized) {
        auto v=Serialization::LVVariable::FromJSON(record);const auto n=identifiers[index++];
        if(!actionVariables.emplace(v.name, ActionVariable{n,v}).second) throw std::runtime_error("Variable names must be unique: "+v.name);
        auto type=v.type==Serialization::VariableType::String?"const char *":v.type==Serialization::VariableType::Boolean?"bool":v.type==Serialization::VariableType::Float?"double":"int64_t";
        bool string=v.type==Serialization::VariableType::String;
        const auto storageType=Serialization::LVVariable::CodeType(v);
        const auto codeSymbol=Serialization::LVVariable::CodeSymbol(v);
        const auto external="ui_external_"+n;
        variableNames.push_back(n);
        auto literal=[&](const json& value){if(string)return quote(value.get<std::string>());if(value.is_number_unsigned()&&value.get<uint64_t>()>INT64_MAX&&v.type!=Serialization::VariableType::Float)throw std::runtime_error("Variable exceeds int64_t: "+v.name);auto s=number(value);if(type==std::string("int64_t")){if(s=="-9223372036854775808")return std::string("INT64_MIN");return "INT64_C("+s+")";}return s;};
        auto current="ui_variables."+n, history="ui_storage_"+n;fields<<"    "<<type<<' '<<n<<";\n";
        globals<<"static "<<type<<" *"<<history<<";\nstatic size_t "<<history<<"_count, "<<history<<"_capacity;\n";
        globals<<"static "<<storageType<<" *"<<external<<";\n";
        if(string)globals<<"static size_t "<<external<<"_capacity;\n";
        header<<"bool ui_set_"<<n<<'('<<type<<" value);\nsize_t ui_history_"<<n<<"_count(void);\nbool ui_history_"<<n<<"_get(size_t index, "<<type<<" *value);\n";
        header<<"bool ui_bind_"<<n<<'('<<storageType<<" *storage"<<(string?", size_t capacity":"")<<");\nvoid ui_unbind_"<<n<<"(void);\nbool ui_sync_"<<n<<"(void);\n";
        auto metadata=json{{"name",v.name},{"field",n},{"setter","ui_set_"+n},{"dataType",int(v.type)},
                           {"cType",storageType},{"bind","ui_bind_"+n},{"sync","ui_sync_"+n},{"unbind","ui_unbind_"+n},
                           {"bindings",json::array()}};
        if(!codeSymbol.empty())metadata["symbol"]=codeSymbol;
        std::vector<std::pair<std::string,std::string>> outputs,inputs;
        bool needsLVGLNumber=false;
        for(const auto& target:Serialization::LVVariable::Bindings(v)) {
            auto ref=std::make_pair(target.screen,target.objectId);if(!handles.count(ref))throw std::runtime_error("Variable references a missing widget: "+v.name);
            auto object=handles.at(ref);const auto& widget=types.at(ref);auto path=target.property;
            const auto& bindingWidget=variableWidgetRecords.at(ref);
            Serialization::LVVariable::ValidateBinding(v,target,bindingWidget);
            std::string binding,getter,eventObject=object;
            auto slash=path.find_last_of('/');auto property=slash==std::string::npos?path:path.substr(slash+1);
            if(property=="text" && (widget=="label"||widget=="textarea"||widget=="checkbox")) {
                if(string)binding="lv_"+widget+"_set_text("+object+", value);";
                else {auto format=v.type==Serialization::VariableType::Float?"\"%.17g\", value":v.type==Serialization::VariableType::Boolean?"\"%s\", value ? \"true\" : \"false\"":"\"%\" PRId64, value";binding="{ char text[64]; snprintf(text, sizeof text, "+std::string(format)+"); lv_"+widget+"_set_text("+object+", text); }";}
                if(string&&widget=="textarea")getter="lv_textarea_get_text("+object+")";
            } else if(widget=="advancedKeyboard" && property=="text") {
                if(string) {
                    binding="lv_advanced_keyboard_set_text("+object+", value);";
                    getter="lv_advanced_keyboard_get_text("+object+")";
                } else {
                    const auto format=v.type==Serialization::VariableType::Float?"\"%.17g\", value":v.type==Serialization::VariableType::Boolean?"\"%s\", value ? \"true\" : \"false\"":"\"%\" PRId64, value";
                    binding="{ char text[64]; snprintf(text, sizeof text, "+std::string(format)+"); lv_advanced_keyboard_set_text("+object+", text); }";
                }
            } else if(!string&&(property=="checked"||property=="hidden"||property=="disabled")) {
                binding="lv_obj_set_"+property+"("+object+", value != 0);";if(property=="checked")getter="lv_obj_is_checked("+object+")";
            } else if(!string&&(property=="x"||property=="y"||property=="width"||property=="height"))binding="lv_obj_set_"+property+"("+object+", (int32_t)value);";
            else if(!string&&property=="opascale")binding="lv_obj_set_style_opa("+object+", (lv_opa_t)value, 0);";
            else if(!string&&property=="value"&&(widget=="arc"||widget=="bar"||widget=="slider"||widget=="spinbox")) {
                binding="lv_"+widget+"_set_value("+object+", (int32_t)value"+(widget=="bar"||widget=="slider"?", LV_ANIM_OFF":"")+");";getter="lv_"+widget+"_get_value("+object+")";
            } else if(!string&&property=="selected"&&(widget=="dropdown"||widget=="roller")) {
                binding="lv_"+widget+"_set_selected("+object+", (uint32_t)value"+(widget=="roller"?", LV_ANIM_OFF":"")+");";getter="lv_"+widget+"_get_selected("+object+")";
            } else if(!string&&property=="brightness"&&widget=="led")binding="lv_led_set_brightness("+object+", (uint8_t)value);";
            else if(widget=="textBox"&&property=="value"&&!string) {binding="ui_textbox_assign(&"+object+"_custom, (int64_t)value);";getter=object+"_custom.value";}
            else if(widget=="textBox"&&property=="text"&&string) {binding="lv_textarea_set_text("+object+"_custom.entry, value);";getter="lv_textarea_get_text("+object+"_custom.entry)";}
            else if(widget=="layerView"&&property=="selected"&&!string) {binding=object+"_custom.selected = (uint32_t)value; ui_layer_refresh(&"+object+"_custom);";getter=object+"_custom.selected";}
            else throw std::runtime_error("Unsupported variable binding: "+v.name+" -> "+path);
            const bool toWidget=target.direction!=Serialization::VariableBindingDirection::FromWidget,
                       fromWidget=target.direction!=Serialization::VariableBindingDirection::ToWidget;
            bool readable=false;
            for(const auto& option:Serialization::LVVariable::BindingProperties(bindingWidget,v.type)) {
                const auto position=option.path.find_last_of('/');
                if(option.path.substr(position==std::string::npos?0:position+1)==property) {readable=option.readable;break;}
            }
            if(fromWidget&&readable&&!getter.empty())inputs.emplace_back(eventObject,getter);
            else if(target.direction==Serialization::VariableBindingDirection::FromWidget)throw std::runtime_error("Variable input needs an interactive readable property: "+v.name+" -> "+path);
            if(toWidget) {
                outputs.emplace_back(object,binding);
                if(property!="text"&&property!="checked"&&property!="hidden"&&property!="disabled")needsLVGLNumber=true;
            }
            metadata["bindings"].push_back(Serialization::LVVariable::BindingToJSON(target));
        }
        manifest["variables"].push_back(std::move(metadata));
        auto& out=variableCode;
        out<<"static void ui_apply_"<<n<<'('<<type<<" value) {\n    (void)value; if (ui_syncing_vars) return; ui_syncing_vars=true;\n";
        for(const auto& binding:outputs)out<<"    if ("<<binding.first<<") { "<<binding.second<<" }\n";
        out<<"    ui_syncing_vars=false;\n}\n";
        out<<"bool ui_set_"<<n<<'('<<type<<" value) {\n    if (!ui_ready || ui_syncing_vars) return false;\n";
        if(string)out<<"    char *copy, *history_copy; size_t length; if (!value) return false; length=strlen(value); if ("<<external<<" && length >= "<<external<<"_capacity) return false; copy=lv_strdup(value); if (!copy) return false; history_copy=lv_strdup(value); if (!history_copy) { lv_free(copy); return false; }\n";
        else if(v.type==Serialization::VariableType::Float) {
            out<<"    if (!isfinite(value)) return false;\n";
            if(storageType=="float")out<<"    if (value < -FLT_MAX || value > FLT_MAX) return false; value=(double)(float)value;\n";
        } else if(storageType!="int64_t"&&storageType!="bool") {
            const std::map<std::string,std::pair<std::string,std::string>> limits={
                {"int8_t",{"INT8_MIN","INT8_MAX"}},{"int16_t",{"INT16_MIN","INT16_MAX"}},{"int32_t",{"INT32_MIN","INT32_MAX"}},
                {"uint8_t",{"0","UINT8_MAX"}},{"uint16_t",{"0","UINT16_MAX"}},{"uint32_t",{"0","UINT32_MAX"}}};
            const auto bounds=limits.at(storageType);out<<"    if (value < "<<bounds.first<<" || value > "<<bounds.second<<") return false;\n";
        }
        if(needsLVGLNumber&&v.type!=Serialization::VariableType::Boolean)out<<"    if (value < INT32_MIN || value > INT32_MAX) return false;\n";
        out<<"    if (!ui_reserve((void**)&"<<history<<", &"<<history<<"_capacity, "<<history<<"_count, "<<v.history<<", sizeof *"<<history<<")) {"<<(string?"lv_free(copy); lv_free(history_copy);":"")<<"return false;}\n";
        if(v.history)out<<"    if ("<<history<<"_count == "<<v.history<<") { "<<(string?"lv_free((void*)"+history+"[0]); ":"")<<"memmove("<<history<<", "<<history<<"+1, (--"<<history<<"_count)*sizeof *"<<history<<"); }\n";
        if(string)out<<"    lv_free((void*)"<<current<<"); "<<current<<" = copy; value=copy; "<<history<<'['<<history<<"_count++] = history_copy;\n";
        else out<<"    "<<current<<" = value; "<<history<<'['<<history<<"_count++] = value;\n";
        if(string)out<<"    if ("<<external<<") memmove("<<external<<", value, length+1);\n";
        else out<<"    if ("<<external<<") *"<<external<<"=("<<storageType<<")value;\n";
        out<<"    ui_apply_"<<n<<"(value);\n";
        out<<"    return true;\n}\nsize_t ui_history_"<<n<<"_count(void) { return "<<history<<"_count; }\nbool ui_history_"<<n<<"_get(size_t index, "<<type<<" *value) { if (!value || index >= "<<history<<"_count) return false; *value="<<history<<"[index]; return true; }\n";
        out<<"bool ui_sync_"<<n<<"(void) {\n    if (!ui_ready) return false; if (!"<<external<<") return true;\n";
        if(string)out<<"    if (!memchr("<<external<<", '\\0', "<<external<<"_capacity)) return false; if ("<<current<<" && strcmp("<<current<<", "<<external<<")==0) return true; return ui_set_"<<n<<'('<<external<<");\n";
        else out<<"    if ("<<current<<" == ("<<type<<")*"<<external<<") return true; return ui_set_"<<n<<"(("<<type<<")*"<<external<<");\n";
        out<<"}\nbool ui_bind_"<<n<<'('<<storageType<<" *storage"<<(string?", size_t capacity":"")<<") {\n    "<<storageType<<" *previous="<<external<<";\n";
        if(string)out<<"    size_t previous_capacity="<<external<<"_capacity; if (!ui_ready || ui_syncing_vars || !storage || !capacity || !memchr(storage, '\\0', capacity)) return false; "<<external<<"_capacity=capacity;\n";
        else out<<"    if (!ui_ready || ui_syncing_vars || !storage) return false;\n";
        out<<"    "<<external<<"=storage; if (ui_sync_"<<n<<"()) return true; "<<external<<"=previous;"<<(string?" "+external+"_capacity=previous_capacity;":"")<<" return false;\n}\nvoid ui_unbind_"<<n<<"(void) { "<<external<<"=NULL;"<<(string?" "+external+"_capacity=0;":"")<<" }\n";
        for(size_t i=0;i<inputs.size();++i) {
            const auto& input=inputs[i];const auto callback="ui_changed_"+n+"_"+std::to_string(i);
            out<<"static void "<<callback<<"(lv_event_t *event) { (void)event; if (ui_ready && "<<input.first<<" && !ui_syncing_vars) { if (!ui_set_"<<n<<"("<<input.second<<")) ui_apply_"<<n<<'('<<current<<"); } }\n";
            variableInit<<"    lv_obj_add_event_cb("<<input.first<<", "<<callback<<", LV_EVENT_VALUE_CHANGED, NULL);\n";
        }
        variableInit<<"    if (!ui_set_"<<n<<'('<<literal(v.defaultValue)<<")) { ui_deinit(); return false; }\n";
        // Defaults establish the current value, but are not an entry in saved history.
        if(string)variableInit<<"    lv_free((void*)"<<history<<"[0]);\n";
        variableInit<<"    "<<history<<"_count=0;\n";
        for(auto& value:v.values)variableInit<<"    if (!ui_set_"<<n<<'('<<literal(value)<<")) { ui_deinit(); return false; }\n";
        if(string)variableFree<<"    { size_t i; for (i=0;i<"<<history<<"_count;++i) lv_free((void*)"<<history<<"[i]); } lv_free((void*)"<<current<<"); "<<current<<"=NULL;\n";
        variableFree<<"    lv_free("<<history<<"); "<<history<<"=NULL; "<<history<<"_count=0; "<<history<<"_capacity=0;\n";
        variableFree<<"    ui_unbind_"<<n<<"();\n";
        const auto symbol=codeSymbol.empty()?"application_"+n:codeSymbol;
        if(codeSymbol.empty())
            variableWiring<<"static "<<storageType<<' '<<symbol<<(string?"["+std::to_string(std::max<size_t>(128,v.defaultValue.get<std::string>().size()+1))+"]":"")<<" = "<<literal(v.defaultValue)<<";\n";
        else if(string)
            variableWiring<<"/* Existing writable char array: "<<symbol<<". For a char * pointer,\n   replace sizeof "<<symbol<<" with its actual buffer capacity, including NUL. */\n";
        else
            variableWiring<<"/* Existing application storage: "<<storageType<<' '<<symbol<<"; do not redeclare it here. */\n";
        variableWiring<<"if (!ui_bind_"<<n<<'('<<(string?symbol:"&"+symbol)<<(string?", sizeof "+symbol:"")<<")) { /* handle failure */ }\n";
    }
    header<<"bool ui_sync_variables(void);\n";
    variableCode<<"bool ui_sync_variables(void) {\n    bool result=ui_ready;\n";
    for(const auto& n:variableNames)variableCode<<"    if (!ui_sync_"<<n<<"()) result=false;\n";
    variableCode<<"    return result;\n}\n";
    header<<"typedef struct {\n"<<(fields.str().empty()?"    uint8_t reserved;\n":fields.str())<<"} ui_variables_t;\nextern ui_variables_t ui_variables;\n";
    globals<<"ui_variables_t ui_variables;\n";
}

void Generator::interactions(const json& project) {
    auto documents=project.contains("screens")?project.at("screens"):json::array({project});
    bool any=false;
    auto variable=[&](const std::string& name)->const ActionVariable& {auto found=actionVariables.find(name);if(found==actionVariables.end())throw std::runtime_error("Action references a missing variable: "+name);return found->second;};
    auto literal=[&](const ActionVariable& v,const json& value,bool storageRange=true) {
        auto check=v.definition;check.defaultValue=value;check.values=json::array();
        if(!storageRange)check.extra.erase("code"); // A signed increment may target unsigned application storage.
        Serialization::LVVariable::ToJSON(check);
        if(v.definition.type==Serialization::VariableType::String)return quote(value.get<std::string>());
        if(v.definition.type==Serialization::VariableType::Boolean)return std::string(value.get<bool>()?"true":"false");
        if(v.definition.type==Serialization::VariableType::Float)return number(value);
        if(value.is_number_unsigned()&&value.get<uint64_t>()>INT64_MAX)throw std::runtime_error("Action integer exceeds int64_t");
        const auto text=number(value);return text=="-9223372036854775808"?std::string("INT64_MIN"):"INT64_C("+text+")";
    };
    for(size_t si=0;si<documents.size();++si) {int next=0;for(const auto& record:documents[si].value("obj",json::array())) {
        const int oid=record.value("oid",next++);
        const auto actions=Serialization::InteractionActions::Normalize(record.value("base",json::object()).value("events",json::array()));
        if(actions.empty())continue;
        if(!any){globals<<"static bool ui_executing_actions;\nstatic const char *ui_action_error;\n";header<<"const char *ui_last_action_error(void);\n";actionCode<<"const char *ui_last_action_error(void) { return ui_action_error ? ui_action_error : \"\"; }\n";any=true;}
        const auto source=handles.at({si,oid});const auto callback=unique("action");
        actionCode<<"static void "<<callback<<"(lv_event_t *event) {\n    lv_event_code_t code=lv_event_get_code(event);\n    if (ui_syncing_vars || ui_executing_actions || lv_event_get_target(event)!=lv_event_get_current_target(event)) return;\n    if (code!=LV_EVENT_CLICKED && code!=LV_EVENT_VALUE_CHANGED && code!=LV_EVENT_READY) return;\n    ui_executing_actions=true; ui_action_error=NULL;\n    do {\n";
        for(const auto& a:actions) {
            const auto event=a.at("event").get<std::string>(),action=a.at("action").get<std::string>();
            std::string condition="true";
            if(a.contains("condition")) {
                const auto& c=a.at("condition");const auto& v=variable(c.at("variable"));const auto current="ui_variables."+v.field,op=c.value("op",std::string("eq"));const auto value=c.value("value",json(true));
                if(op=="truthy")condition=v.definition.type==Serialization::VariableType::String?"("+current+" && "+current+"[0])":"("+current+" != 0)";
                else {
                    bool compatible=v.definition.type==Serialization::VariableType::String?value.is_string():v.definition.type==Serialization::VariableType::Boolean?value.is_boolean():value.is_number();
                    if(!compatible)condition=op=="ne"?"true":"false";
                    else if(v.definition.type==Serialization::VariableType::String)condition=(op=="eq"||op=="ne")?"("+current+" && strcmp("+current+", "+literal(v,value)+") "+(op=="eq"?"==":"!=")+" 0)":"false";
                    else condition="("+current+" "+(op=="eq"?"==":op=="ne"?"!=":op=="gt"?">":"<")+" "+(value.is_boolean()?std::string(value.get<bool>()?"true":"false"):number(value))+")";
                }
            }
            actionCode<<"        if (code=="<<(event=="clicked"?"LV_EVENT_CLICKED":event=="value_changed"?"LV_EVENT_VALUE_CHANGED":"LV_EVENT_READY")<<" && "<<condition<<") {\n";
            if(action=="navigate") {
                const auto screen=a.at("screen").get<size_t>();if(screen>=screenCount)throw std::runtime_error("Action references a missing screen");
                actionCode<<"            if (!ui_show_screen("<<screen<<")) {ui_action_error=\"Screen unavailable\";break;}\n";
            } else if(action=="set_variable"||action=="increment_variable") {
                const auto& v=variable(a.at("variable"));const auto current="ui_variables."+v.field;std::string expression;
                if(action=="set_variable")expression=literal(v,a.at("value"));
                else if(v.definition.type==Serialization::VariableType::Float) {expression=current+" + "+number(a.at("value"));}
                else if(v.definition.type==Serialization::VariableType::Integer||v.definition.type==Serialization::VariableType::DateTime) {
                    if(!a.at("value").is_number_integer())throw std::runtime_error("Integer increments require an integer value");
                    const auto temp=unique("increment");actionCode<<"            int64_t "<<temp<<"; if (!ui_interaction_increment_i64("<<current<<", "<<literal(v,a.at("value"),false)<<", &"<<temp<<")) {ui_action_error=\"Variable increment overflow\";break;}\n";expression=temp;
                } else throw std::runtime_error("Increment requires a numeric variable");
                actionCode<<"            if (!ui_set_"<<v.field<<"("<<expression<<")) {ui_action_error=\"Variable update failed\";break;}\n";
            } else {
                const int targetId=a.value("targetOid",-1);const size_t targetScreen=a.value("targetScreen",si);auto ref=std::make_pair(targetScreen,targetId==-1?oid:targetId);
                if(!handles.count(ref))throw std::runtime_error("Action references a missing widget");const auto target=handles.at(ref);
                actionCode<<"            if (!"<<target<<") {ui_action_error=\"Widget unavailable\";break;}\n";
                if(action=="show")actionCode<<"            lv_obj_remove_flag("<<target<<", LV_OBJ_FLAG_HIDDEN);\n";
                else if(action=="hide")actionCode<<"            lv_obj_add_flag("<<target<<", LV_OBJ_FLAG_HIDDEN);\n";
                else if(action=="toggle")actionCode<<"            if(lv_obj_has_flag("<<target<<", LV_OBJ_FLAG_HIDDEN))lv_obj_remove_flag("<<target<<", LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag("<<target<<", LV_OBJ_FLAG_HIDDEN);\n";
                else if(action=="validate") {
                    auto widget=types.at(ref);if(widget!="textarea"&&widget!="textBox")throw std::runtime_error("Validation target must be a text box");
                    requirements.insert("TEXTAREA");auto entry=widget=="textarea"?target:target+"_custom.entry";
                    actionCode<<"            if (!ui_interaction_validate("<<target<<", lv_textarea_get_text("<<entry<<"), "<<(a.value("required",true)?"true":"false")<<", "<<(a.contains("min")?"true":"false")<<", "<<(a.contains("min")?number(a.at("min")):"0")<<", "<<(a.contains("max")?"true":"false")<<", "<<(a.contains("max")?number(a.at("max")):"0")<<")) {ui_action_error=\"Please enter a valid value\";break;}\n";
                }
            }
            actionCode<<"        }\n";
        }
        actionCode<<"    } while (false);\n    ui_executing_actions=false;\n}\n";
        actionInit<<"    lv_obj_add_event_cb("<<source<<", "<<callback<<", LV_EVENT_ALL, NULL);\n";
        manifest["interactions"].push_back({{"screen",si},{"oid",oid},{"actions",actions}});
    }}
    if(any)actionInit<<"    ui_executing_actions=false; ui_action_error=NULL;\n";
}

std::map<std::string,std::string> Generator::files(const json& input) {
    auto project=input;Serialization::ProjectDesign::Materialize(project,true);Serialization::MigrateAdvancedKeyboardEditors(project);
    const auto catalog=Serialization::ProjectFonts::Normalize(project.value("fonts",json::array()));
    Serialization::ProjectFonts::References(project,catalog,true);
    Serialization::ProjectImages::References(project,project.value("images",json::array()),true);
    std::ostringstream fontAccess;fontAccess<<"const lv_font_t *ui_get_font(const char *id) { if (!id) return NULL;\n";
    manifest["fonts"]=json::array();
    for(const auto& record:catalog) {auto expression=font(record.at("font"));fontAccess<<"    if (!strcmp(id, "<<quote(record.at("id"))<<")) return "<<expression<<";\n";manifest["fonts"].push_back({{"id",record.at("id")},{"name",record.at("name")},{"symbol",expression.substr(1)}});}
    fontAccess<<"    return NULL;\n}\n";header<<"const lv_font_t *ui_get_font(const char *id);\n";
    fontAccess<<"const lv_image_dsc_t *ui_get_image(const char *id) { if (!id) return NULL;\n";
    manifest["projectImages"]=json::array();
    for(const auto& record:project.value("images",json::array())){auto im=image({{"asset",record.at("image")}});fontAccess<<"    if (!strcmp(id, "<<quote(record.at("id"))<<")) return "<<im.expression<<";\n";manifest["projectImages"].push_back({{"id",record.at("id")},{"name",record.at("name")}});}
    fontAccess<<"    return NULL;\n}\n";header<<"const lv_image_dsc_t *ui_get_image(const char *id);\n";

    if(!project.is_object())throw std::runtime_error("Load a project before exporting");
    std::set<std::string> requiredTries;
    auto collectTries=[&](const auto& self,const json& value)->void {
        if(value.is_object()) {
            if(value.contains("advancedKeyboard"))requiredTries.insert(value.at("advancedKeyboard").value("trieId",std::string("builtin-en-us")));
            for(const auto& field:value.items())if(field.key()!="tries")self(self,field.value());
        } else if(value.is_array())for(const auto& child:value)self(self,child);
    };
    collectTries(collectTries,project);
    std::map<std::string,TrieAsset> sharedTries;
    for(const auto& trie:Serialization::ProjectTries::Normalize(project.value("tries",json::array()))) {
        const auto id=trie.at("id").get<std::string>();if(!requiredTries.count(id))continue;
        const auto key=trie.at("data").get<std::string>()+":"+trie.at("indexData").get<std::string>();
        if(!sharedTries.count(key)) {
            const auto data=base64_decode(trie.at("data")),index=base64_decode(trie.at("indexData"));
            sharedTries[key]={bytes(data,false,true),bytes(index,false,true),data.size(),index.size()};
        }
        trieAssets[id]=sharedTries.at(key);
    }
    screens(project);linkKeyboards();variables(project.value("variables",json::array()));interactions(project);
    std::ostringstream requirementsFile;requirementsFile<<"#pragma once\n#include <lvgl.h>\n#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6\n#error This project requires LVGL 9.6.x\n#endif\n";
    for(auto& r:requirements) {const auto macro=r=="BIN_DECODER_RAM_LOAD"?"LV_BIN_DECODER_RAM_LOAD":r=="FLEX"||r=="GRID"?"LV_USE_"+r:r=="DRAW_SW_COMPLEX_GRADIENTS"?"LV_DRAW_SW_COMPLEX_GRADIENTS":"LV_USE_"+r;requirementsFile<<"#if !"<<macro<<"\n#error Enable "<<macro<<" in lv_conf.h for this project\n#endif\n";}
    std::string h="#pragma once\n#include <lvgl.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#ifdef __cplusplus\nextern \"C\" {\n#endif\nbool ui_init(void);\nvoid ui_deinit(void);\nsize_t ui_screen_count(void);\nlv_obj_t *ui_get_screen(size_t index);\nlv_obj_t *ui_get_object(size_t screen, int32_t object_id);\nbool ui_show_screen(size_t index);\n"+header.str()+"#ifdef __cplusplus\n}\n#endif\n";
    std::ostringstream c;c<<"/* Generated by LVGL UI Creator. LVGL and your display/input drivers must be initialized first. */\n#include \"ui.h\"\n#include \"ui_assets.h\"\n#include \"ui_requirements.h\"\n#include <math.h>\n#include <float.h>\n";
    if(usesAdvancedKeyboard)c<<"#include \"advanced_keyboard/include/lv_advanced_keyboard.h\"\n#include \"advanced_keyboard/include/lv_advanced_keyboard_prediction.h\"\n";
    c<<CGeneration::Runtime<<globals.str()<<focusCode()<<variableCode.str()<<actionCode.str()<<fontAccess.str();
    if(usesTransitions) {
        c<<R"TRANSITION(
static void ui_cancel_legacy_screen_transition(lv_obj_t *screen)
{
#if LVGL_VERSION_MAJOR == 8
    lv_disp_t *display; lv_obj_t *pending;
    if (!screen) return;
    display=lv_obj_get_disp(screen);
    if (!display || !display->scr_to_load) return;
    /* LVGL 8.3 clears scr_to_load during its own interruption path and then
       dereferences it. Cancel before calling that path. lv_disp_t is public. */
    pending=display->scr_to_load; display->scr_to_load=NULL;
    lv_anim_delete(pending,NULL);lv_obj_set_pos(pending,0,0);
    lv_obj_remove_local_style_prop(pending,LV_STYLE_OPA,0);
    if (display->act_scr) {
        lv_anim_delete(display->act_scr,NULL);lv_obj_set_pos(display->act_scr,0,0);
        lv_obj_remove_local_style_prop(display->act_scr,LV_STYLE_OPA,0);
    }
    if (display->prev_scr && display->prev_scr!=display->act_scr) {
        lv_anim_delete(display->prev_scr,NULL);lv_obj_set_pos(display->prev_scr,0,0);
        lv_obj_remove_local_style_prop(display->prev_scr,LV_STYLE_OPA,0);
    }
    display->prev_scr=NULL;display->draw_prev_over_act=false;display->del_prev=false;
#else
    (void)screen;
#endif
}
)TRANSITION";
        std::set<std::string> transitions;
        for(const auto& transition:screenTransitions)if(transition.first!="NONE"&&transition.second)transitions.insert(transition.first);
        c<<"#if LVGL_VERSION_MAJOR == 8 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 5)\n";
        for(const auto& transition:transitions)c<<"#define UI_SCREEN_ANIM_"<<transition<<" LV_SCR_LOAD_ANIM_"<<(transition=="FADE_IN"?"FADE_ON":transition)<<"\n";
        c<<"#else\n";
        for(const auto& transition:transitions)c<<"#define UI_SCREEN_ANIM_"<<transition<<" LV_SCREEN_LOAD_ANIM_"<<transition<<"\n";
        c<<"#endif\n";
    }
    c<<"size_t ui_screen_count(void) { return "<<screenCount<<"; }\nlv_obj_t *ui_get_screen(size_t index) { return index < "<<screenCount<<" ? ui_screens[index] : NULL; }\nbool ui_show_screen(size_t index) {\n    lv_obj_t *screen=ui_get_screen(index); if (!screen) return false;\n";
    if(usesTransitions)c<<"    ui_cancel_legacy_screen_transition(screen);\n";
    if(!focusOwners.empty())c<<"    ui_focus_activate(index);\n";
    if(usesTransitions) {
        c<<"    switch (index) {\n";
        for(size_t i=0;i<screenTransitions.size();++i)if(screenTransitions[i].first!="NONE"&&screenTransitions[i].second)
            c<<"    case "<<i<<": lv_screen_load_anim(screen,UI_SCREEN_ANIM_"<<screenTransitions[i].first<<","<<screenTransitions[i].second<<",0,false); break;\n";
        c<<"    default: lv_screen_load(screen); break;\n    }\n";
    }else c<<"    lv_screen_load(screen);\n";
    c<<"    return true;\n}\nlv_obj_t *ui_get_object(size_t screen, int32_t object_id) {\n";
    for(auto& hnd:handles)c<<"    if (screen == "<<hnd.first.first<<" && object_id == "<<hnd.first.second<<") return "<<hnd.second<<";\n";c<<"    return NULL;\n}\n";
    c<<"void ui_deinit(void) {\n    size_t i; lv_obj_t *active=lv_screen_active();\n";
    if(usesTransitions)c<<"    ui_cancel_legacy_screen_transition(active);\n";
    if(usesTransitions)c<<"    /* Cancel a pending first-screen transition even before it becomes active. */\n    if (active && active==ui_previous_screen) {\n        lv_obj_t *guard=lv_obj_create(NULL);\n        if (guard) {lv_screen_load(guard);lv_screen_load(ui_previous_screen);lv_obj_delete(guard);}\n    }\n";
    c<<"    for (i=0;i<"<<screenCount<<";++i) if (ui_screens[i] && active == ui_screens[i]) { if (!ui_previous_screen) ui_previous_screen=lv_obj_create(NULL); lv_screen_load(ui_previous_screen); break; }\n    for (i=0;i<"<<screenCount<<";++i) if (ui_screens[i]) lv_obj_delete(ui_screens[i]);\n";
    if(!focusOwners.empty())c<<"    ui_focus_free();\n";
    c<<keyboardFree.str()<<variableFree.str()<<"    if (ui_previous_screen) lv_obj_remove_event_cb_with_user_data(ui_previous_screen, ui_forget_object, &ui_previous_screen); ui_previous_screen=NULL; ui_ready=false; ui_syncing_vars=false;\n}\n";
    c<<"bool ui_init(void) {\n    if (ui_ready) return true;\n    if (!lv_display_get_default()) return false;\n    ui_previous_screen=lv_screen_active(); if (ui_previous_screen) ui_track_object(ui_previous_screen, &ui_previous_screen);\n    ui_ready=true;\n";
    if(!focusOwners.empty())c<<"    ui_focus_group=lv_group_get_default();\n";
    c<<body.str()<<variableInit.str()<<actionInit.str();
    if(!focusOwners.empty())c<<"    if (!ui_focus_init()) {ui_deinit();return false;}\n";
    c<<"    return ui_show_screen("<<project.value("activeScreen",0u)<<");\n}\n";
    const std::string cmake="cmake_minimum_required(VERSION 3.24)\nproject(GeneratedUI LANGUAGES C)\nadd_library(generated_ui STATIC ui.c ui_assets.c)\nset_target_properties(generated_ui PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)\ntarget_include_directories(generated_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})\nif(TARGET lvgl::lvgl)\n target_link_libraries(generated_ui PUBLIC lvgl::lvgl)\nelseif(TARGET lvgl)\n target_link_libraries(generated_ui PUBLIC lvgl)\nelse()\n message(FATAL_ERROR \"Add your LVGL target before adding this directory\")\nendif()\n";
    const std::string readme="# Generated LVGL project\n\nThese C11 sources target LVGL 9.6.x. No editor, C++, JSON, Protobuf, SDL, filesystem or image decoder is required on the device. PNG/JPEG/GIF assets are decoded during export to ARGB8888 arrays; GIF frames advance using LVGL timers. Fonts and widget maps are compiled into the assets. Canvas buffers are writable RAM. Budget flash for uncompressed images and RAM for screens, canvases and variable histories.\n\nAdd LVGL to your firmware build, configure the options listed in ui_requirements.h, then add_subdirectory(path/to/generated_ui) and link generated_ui. Your firmware supplies LVGL, the display/input drivers and periodic tick/timer handling. After initializing them:\n\n```c\n#include \"ui.h\"\nif (!ui_init()) { /* initialization failed */ }\n/* ui_init loads the initial screen. */\n```\n\nUse ui_get_screen(index), ui_show_screen(index), and ui_get_object(screen_index, saved_object_id). manifest.json maps original names and IDs to generated symbols. Variables are exposed in ui_variables; update them through ui_set_v_NAME to maintain bindings and history. String pointers are borrowed and expire on the next update or ui_deinit. History getters return oldest first; a configured history of zero grows until memory is exhausted. Setters report allocation failures. Interactive bindings update variables on VALUE_CHANGED. All calls must run on the LVGL thread.\n\nui_init is idempotent. ui_deinit releases generated screens, animation timers and variable storage, restoring the previous screen when available. Call it before deleting the display or deinitializing LVGL. Application-specific actions and hardware behavior should be attached by firmware after ui_init; editor-only drag settings are not exported. Re-export overwrites generated files, so keep application logic outside this directory.\n\nCLI: UICreatorCodeGen project.json output_directory (also accepts project.pb). The editor exports through File > Export C project. External image paths resolve relative to the saved project; embedded assets are preferred. Optional multi-screen project records use a screens array of {name,canvas,obj} plus activeScreen and shared variables (binding screen defaults to 0).\n";
    std::map<std::string,std::string> output={{"ui.h",h},{"ui.c",c.str()},{"ui_assets.h","#pragma once\n#include <lvgl.h>\n"+assetHeader.str()},{"ui_assets.c","#include \"ui_assets.h\"\n"+assets.str()},{"ui_requirements.h",requirementsFile.str()},{"CMakeLists.txt",cmake},{"README.md",readme},{"manifest.json",manifest.dump(2)+"\n"}};
    if(!variableWiring.str().empty())output["README.md"]+="\n## Application variable bindings\n\nEach variable also has a generated typed `ui_bind_v_NAME` function. Call it after `ui_init`, passing application-owned storage that stays alive until `ui_unbind_v_NAME` or `ui_deinit`. Binding imports the application's existing value; it does not replace that value with the editor default. The pointer can refer to a C global, a C++ namespace variable or a C++ object member. The optional code symbol is a wiring hint only: generated C never declares or links an application symbol. Keep the following wiring in your application, outside the generated directory:\n\n```c\n"+variableWiring.str()+"```\n\nChange application values on the LVGL thread, then call `ui_sync_variables()` (or just `ui_sync_v_NAME()`). Unchanged values do not redraw widgets or add history. The call returns false on allocation failure, a nonfinite number or an invalid string buffer; other variables are still synchronized. Use `ui_set_v_NAME` when setting through the generated UI API; it also writes bound application storage. Interactive widget changes update the same storage and every output/two-way widget for that variable. A `to-widget` binding only receives updates; a `from-widget` binding only publishes user edits; `two-way` does both. Binding targets can span screens.\n\nInteger pointer types are exact, including `int32_t`; narrower values are range checked before a setter changes anything. Float bindings use `float` or `double` as selected; float values are rounded once to the application's actual precision. Strings bind a writable `char` buffer and its capacity, including the terminating NUL. The buffer must contain a NUL within that capacity. Oversize strings fail without truncation or changing the value/history; a two-way widget is restored to the previous value when its edit cannot fit. Rebind a different buffer explicitly after resizing. `ui_deinit` drops bindings without freeing or modifying application storage. Bindings do not provide thread synchronization: pass data from worker tasks to the LVGL thread before syncing.\n";
    if(!variableWiring.str().empty())output["README.md"]+="\nSetters and binders return false when called again from a synchronous widget callback during variable propagation. Queue such application updates for the next LVGL iteration. Authored widget actions are suppressed while propagating programmatic variable updates; actions still run for user-generated events.\n";
    if(usesTransitions)output["README.md"]+="\nui_show_screen applies the destination screen's transition and duration. Transitions retain both screens and never automatically delete them; repeated navigation and ui_deinit safely cancel pending transitions.\n";
    if(!focusOwners.empty())output["README.md"]+="\nCreate and set the default LVGL input group before ui_init; keep it alive through ui_deinit. Authored tabIndex values sort explicit controls before natural controls. -1 excludes a widget and its internal controls. Only the active generated screen's controls remain in that group. Focus storage and deletion callbacks are omitted when no explicit focus metadata is authored.\n";
    if(usesAdvancedKeyboard) {
        auto support=CGeneration::AdvancedKeyboardSources();
        if(support.empty())throw std::runtime_error("Advanced Keyboard support sources were not bundled with this compiler");
        output.insert(support.begin(),support.end());
        output["CMakeLists.txt"]+="\nenable_language(CXX)\nfile(GLOB_RECURSE advanced_keyboard_sources CONFIGURE_DEPENDS advanced_keyboard/*.cpp advanced_keyboard/*.c)\ntarget_sources(generated_ui PRIVATE ${advanced_keyboard_sources})\ntarget_compile_features(generated_ui PRIVATE cxx_std_17)\nset_target_properties(generated_ui PROPERTIES CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)\ntarget_include_directories(generated_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/advanced_keyboard/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/advanced_keyboard)\n";
        output["CMakeLists.txt"]+="target_compile_definitions(generated_ui PRIVATE LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION="+std::string(usesPrediction?"1":"0")+" LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE="+std::string(requiredTries.count("builtin-en-us")?"1":"0")+")\n";
        if(!usesPrediction)output["CMakeLists.txt"]+="get_target_property(keyboard_export_sources generated_ui SOURCES)\nlist(FILTER keyboard_export_sources EXCLUDE REGEX \"/Prediction(Model|Prefix)?[.]cpp$\")\nset_property(TARGET generated_ui PROPERTY SOURCES ${keyboard_export_sources})\n";
        output["CMakeLists.txt"]+="if(MSVC)\n target_compile_options(generated_ui PRIVATE /utf-8)\nendif()\n";
        auto& instructions=output["README.md"];
        const auto cpp=instructions.find("No editor, C++, JSON");
        if(cpp!=std::string::npos)instructions.replace(cpp,std::string("No editor, C++, JSON").size(),"The bundled Advanced Keyboard requires C++17. No editor, JSON");
        instructions+="\nAdvanced Keyboard support sources, its ESPOS provenance and font license are included in advanced_keyboard/. Its optional suggestion callback can use an application-owned predictor; no dictionary or board driver is bundled.\n";
    }
    return output;
}
}

bool CCodeGenerator::Preflight(nlohmann::json& summary) {
    summary = nlohmann::json::object();
    try {
        Generator generator; generator.source = std::filesystem::u8path(sourceDirectory);
        auto prepared = project;
        CGeneration::EmbedLocalImages(prepared, generator.source);
        CGeneration::PrepareTarget(prepared, targetVersion);
        auto output = generator.files(prepared);
        if (targetVersion != "9.6") {
            output["ui_lvgl_compat.h"] = targetVersion == "8.3" ? CGeneration::Compat8 : CGeneration::Compat9;
            CGeneration::AdaptTarget(output, targetVersion);
        }
        size_t total = 0;
        for (const auto& file : output) total += file.second.size();
        summary = {{"target",targetVersion},{"files",output.size()},{"sourceBytes",total},
                   {"assetSourceBytes",output.at("ui_assets.c").size()},
                   {"manifest",nlohmann::json::parse(output.at("manifest.json"))},
                   {"requirements",output.at("ui_requirements.h")}};
        lastError.clear(); return true;
    } catch (const std::exception& error) { lastError = error.what(); return false; }
}

std::string CCodeGenerator::VariableIdentifier(const nlohmann::json& records,size_t index) {
    return variableIdentifiers(records).at(index);
}

bool CCodeGenerator::ExportProject(std::string directory) {
    try {
        if(directory.empty()) throw std::runtime_error("Choose an output directory");
        Generator generator;generator.source=std::filesystem::u8path(sourceDirectory);
        auto prepared=project;CGeneration::EmbedLocalImages(prepared,generator.source);CGeneration::PrepareTarget(prepared,targetVersion);
        auto output=generator.files(prepared);
        if(targetVersion!="9.6"){output["ui_lvgl_compat.h"]=targetVersion=="8.3"?CGeneration::Compat8:CGeneration::Compat9;CGeneration::AdaptTarget(output,targetVersion);} // Validate everything before touching the destination.
        auto path=std::filesystem::u8path(directory);std::filesystem::create_directories(path);
        for(const auto& file:output) {
            auto destination=path/file.first;std::filesystem::create_directories(destination.parent_path());auto temporary=destination;temporary+=".uicreator-tmp";
            std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);stream<<file.second;stream.close();
            if(!stream) throw std::runtime_error("Cannot write "+destination.u8string());
            std::error_code error;std::filesystem::rename(temporary,destination,error);
            if(error) {std::filesystem::remove(destination);std::filesystem::rename(temporary,destination);}
        }
        lastError.clear();return true;
    } catch(const std::exception& error) {lastError=error.what();return false;}
}
