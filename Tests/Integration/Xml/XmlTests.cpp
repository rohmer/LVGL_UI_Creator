#include <Serialization/Xml.h>
#include <Serialization/Protobuf.h>
#include <Serialization/ProjectTries.h>
#include <Serialization/LVFont.h>
#include <Serialization/Base64.h>
#include <lvgl_runtime.h>
#include <lv_advanced_keyboard.h>
#include <lv_advanced_keyboard_prediction.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#define CHECK(value) do { if (!(value)) throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " #value); } while (0)
namespace {
using namespace Serialization;
using json = nlohmann::json;
namespace fs = std::filesystem;
using Runtime = std::unique_ptr<lvgl_runtime_t, decltype(&lvgl_runtime_destroy)>;
constexpr uint8_t model[] = {'N','G','T','R',3,0,0,0,4,0,0,0,0,0,0,0,0,2,0,2,'a',1,1,'a',1,1};

template<class Action> void Reject(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}
std::string Replace(std::string value, const std::string& before, const std::string& after) {
    const auto at = value.find(before); CHECK(at != std::string::npos);
    value.replace(at, before.size(), after); return value;
}
Binary Bytes(const std::string& text) { return Binary(text.begin(), text.end()); }
std::string WithoutDeclaration(const std::string& text) {
    CHECK(text.find("<?xml") == 0); const auto end=text.find("?>"); CHECK(end!=std::string::npos);
    return text.substr(end+2);
}
std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary); CHECK(input);
    return std::string(std::istreambuf_iterator<char>(input), {});
}
void Write(const fs::path& path, const void* bytes, size_t size) {
    std::ofstream file(path, std::ios::binary); CHECK(file);
    file.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size)); CHECK(file);
}
json Widget(int id, const char* type, json value, int x, int y, int width, int height) {
    return {{"oid", id}, {"par", -1}, {"base", {{"coords", {{"x", x}, {"y", y}, {"width", width}, {"height", height}}}}}, {type, std::move(value)}};
}
json Project() {
    std::string trie;
    auto tries = ProjectTries::Import(json::array(), "XML custom.ngtr", model, sizeof model, &trie);
    const auto image = Read(fs::u8path(UICREATOR_XML_SOURCE_ROOT) / "Tests/Fixtures/Images/sample.png");
    auto slider = Widget(1, "slider", {{"min",0},{"max",100},{"value",0}}, 8,8,240,30);
    auto label = Widget(2, "text", "0", 260,8,120,30);
    label["base"]["nativeStyles"] = {{{"selector",0},{"values",{{"text_font",{{"projectFont","fixture_font"}}}}}}};
    auto next = Widget(8, "button", {{"text","Next & >"}}, 400,8,120,36);
    next["base"]["events"] = json::array({{{"event","clicked"},{"action","increment_variable"},{"variable","level"},{"value",1}},
        {{"event","clicked"},{"action","navigate"},{"screen",1}}});
    json variables = json::array({{{"var",{{"name","level"},{"dataType",1},{"default",3},{"history",3},{"value",{4,7}},
        {"bindings",{{{"screen",0},{"oid",1},{"property","/slider/value"},{"direction","two-way"}},
                     {{"screen",0},{"oid",2},{"property","/text"},{"direction","to-widget"}},
                     {{"screen",1},{"oid",20},{"property","/text"},{"direction","to-widget"}}}}}}},
        {{"var",{{"name","caption"},{"dataType",0},{"default","A & B <xml> \"quote\"\nEspa\xc3\xb1ol"},{"history",2},
                 {"oid",3},{"property","/textarea/text"}}}}});
    return {{"formatVersion",2},{"runtimeTarget","9.6"},{"lvglVersion","9.6.0"},{"activeScreen",0},
        {"tries",tries},{"fonts",{{{"id","fixture_font"},{"name","XML font"},{"font",LVFont::ToJSON(lv_font_montserrat_14)}}}},
        {"images",{{{"id","fixture_image"},{"name","XML image"},{"image",{{"name","fixture.png"},
            {"sourceData",base64_encode(reinterpret_cast<const unsigned char*>(image.data()),static_cast<unsigned int>(image.size()))}}}}}},
        {"variables",variables},{"screens",{
            {{"name","Main & input"},{"canvas",{{"width",640},{"height",400}}},{"obj",{
                slider,label,Widget(3,"textarea",{{"text","Initial"}},8,48,300,48),
                Widget(4,"img",{{"asset",{{"projectImage","fixture_image"}}}},320,48,64,48),
                Widget(5,"barcode",{{"data","12345678"},{"scale",1}},400,48,180,48),
                Widget(6,"qrcode",{{"data","XML & Protobuf"},{"size",56},{"quietZone",true}},560,100,64,64),
                Widget(7,"advancedKeyboard",{{"trieId",trie},{"targetOid",-1},{"autoFocus",true},{"autocorrect",true}},8,112,540,280),
                next,Widget(9,"textarea",{{"text","Second field"}},8,100,300,40)}}},
            {{"name","Monitor"},{"canvas",{{"width",640},{"height",400}}},{"obj",{
                Widget(20,"text","0",8,8,160,36),Widget(21,"textarea",{{"text","Persistent field"}},8,48,300,48)}}}
        }}};
}
struct Display {
    lv_display_t* previous = lv_display_get_default();
    lv_display_t* display = nullptr;
    std::vector<uint32_t> pixels;
    size_t flushes = 0;
    Display() : pixels(640 * 400) {
        display = lv_display_create(640,400); CHECK(display); lv_display_set_default(display);
        lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_buffers(display,pixels.data(),nullptr,uint32_t(pixels.size()*sizeof(uint32_t)),LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_user_data(display,this);
        lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*) {
            ++static_cast<Display*>(lv_display_get_user_data(d))->flushes; lv_display_flush_ready(d);
        });
    }
    ~Display() { lv_display_delete(display); if (previous) lv_display_set_default(previous); }
    std::vector<uint32_t> Render() {
        std::fill(pixels.begin(),pixels.end(),0x13579bdfu);
        lv_obj_invalidate(lv_screen_active()); lv_refr_now(display); CHECK(flushes);
        CHECK(std::any_of(pixels.begin(),pixels.end(),[](uint32_t p){return p != 0x13579bdfu;}));
        return pixels;
    }
};
Runtime Load(Binary& bytes, uint32_t flags = 0, const lvgl_runtime_options_t* options = nullptr) {
    lvgl_runtime_t* runtime = nullptr; char error[1024]{};
    if (!lvgl_runtime_load_memory_ex(bytes.data(),bytes.size(),options,flags,&runtime,error,sizeof error))
        throw std::runtime_error(error);
    CHECK(runtime); return Runtime(runtime,lvgl_runtime_destroy);
}
void Release(Binary& bytes) { std::fill(bytes.begin(),bytes.end(),0xA5); bytes.clear(); bytes.shrink_to_fit(); }
void Timers() { for (int tick=0; tick<6; ++tick) { lv_tick_inc(25); lv_timer_handler(); } }
void Predictor(lv_obj_t* keyboard) {
    CHECK(lv_advanced_keyboard_is_instance(keyboard));
    auto* predictor = lv_advanced_keyboard_get_predictor(keyboard); CHECK(predictor);
    CHECK(lv_advanced_keyboard_predictor_node_count(predictor) == 4);
    lv_advanced_keyboard_suggestion_t words[3]{};
    const auto count = lv_advanced_keyboard_predictor_suggest_folded(predictor,"a",1,nullptr,0,nullptr,0,words,3);
    CHECK(count == 1 && words[0].length == 1 && words[0].text[0] == 'a');
}
void Codec() {
    json original={{"signed",std::numeric_limits<int64_t>::min()},{"unsigned",std::numeric_limits<uint64_t>::max()},
        {"real",1.25},{"negative_zero",-0.0},{"true",true},{"false",false},{"null",nullptr},
        {"text","A & B <tag> \"quote\" 'apostrophe'\n\t\r Unicode \xc3\xa9"},
        {"property & < \" \xc3\xa9",json::array({1,"two",false,nullptr,json::object(),json::array()})},
        {"bytes",json::binary({0,1,127,128,255},73)},{"empty_bytes",json::binary({})}};
    for (const auto kind : {EntityKind::Project,EntityKind::Screen,EntityKind::Widget,EntityKind::Variable,EntityKind::Style,EntityKind::Font,EntityKind::Asset}) {
        const auto text=Xml::Encode(kind,"record & \" \xc3\xa9",original);
        auto record=Xml::Decode(text); CHECK(record.kind==kind); CHECK(record.type=="record & \" \xc3\xa9");
        CHECK(record.data==original); CHECK(record.data["unsigned"].is_number_unsigned());
        CHECK(record.data["bytes"].is_binary()&&record.data["bytes"].get_binary().subtype()==73);
        CHECK(std::signbit(record.data["negative_zero"].get<double>())); CHECK(record.borrowedAssets.empty());
        CHECK(Xml::Encode(record.kind,record.type,record.data)==text);
        CHECK(Xml::Decode(std::string("\xef\xbb\xbf")+text).data==original);
        CHECK(Xml::Decode(std::string("\xef\xbb\xbf \t\r\n")+WithoutDeclaration(text)).data==original);
    }
    const std::string empty=Xml::Encode(EntityKind::Project,"project",json::object());
    const auto complete=empty.rfind("</uicreator>")+std::strlen("</uicreator>");
    CHECK(complete<=empty.size());
    for(size_t n=0;n<complete;++n) Reject([&]{Xml::Decode(empty.data(),n);});
    CHECK(Xml::Decode(empty.data(),complete).data==json::object());
    XmlLimits limits;
    limits.maxBytes=8; Reject([&]{Xml::Encode(EntityKind::Project,"project",original,limits);});
    Reject([&]{Xml::Decode(empty,limits);});
    limits=XmlLimits{}; limits.maxTextBytes=3;
    Reject([&]{Xml::Encode(EntityKind::Project,"project",json("too much text"),limits);});
    Reject([&]{Xml::Decode(Xml::Encode(EntityKind::Project,"project",json("too much text")),limits);});
    limits=XmlLimits{}; limits.maxNodes=4;
    const auto many=json::array({0,1,2,3,4,5,6,7,8,9});
    Reject([&]{Xml::Encode(EntityKind::Project,"project",many,limits);});
    Reject([&]{Xml::Decode(Xml::Encode(EntityKind::Project,"project",many),limits);});
    limits=XmlLimits{}; limits.maxDepth=3; json deep=0;
    for(int n=0;n<12;++n) deep=json::array({deep});
    Reject([&]{Xml::Encode(EntityKind::Project,"project",deep,limits);});
    Reject([&]{Xml::Decode(Xml::Encode(EntityKind::Project,"project",deep),limits);});
    Reject([&]{Xml::Encode(EntityKind::Project,"project",json(std::numeric_limits<double>::infinity()));});
    Reject([&]{Xml::Encode(EntityKind::Project,"project",json("invalid\x01text"));});
    Reject([&]{Xml::Encode(EntityKind::Project,"",json::object());});
    Reject([&]{Xml::Decode(Replace(empty,"type=\"project\"","type=\"\""));});
    Reject([&]{Xml::Decode(Replace(empty,"encoding=\"UTF-8\"","encoding=\"UTF-16\""));});
    for(const std::string invalidUtf8:{std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),std::string("\xf4\x90\x80\x80",4)}) {
        Reject([&]{Xml::Encode(EntityKind::Project,"project",json(invalidUtf8));});
        Reject([&]{Xml::Decode(Replace(Xml::Encode(EntityKind::Project,"project",json("payload")),"payload",invalidUtf8));});
    }
}
void Malformed() {
    Display display; auto* previous=lv_screen_active();
    const auto good=Xml::Encode(EntityKind::Project,"project",{{"canvas",{{"width",640},{"height",400}}},{"obj",json::array()}});
    std::vector<std::string> invalid={
        "", "<project/>", "<uicreator/>", "<!DOCTYPE uicreator [<!ENTITY x SYSTEM 'file:///not-read'>]><uicreator/>",
        Replace(good,"format_version=\"1\"","format_version=\"2\""), Replace(good,"kind=\"project\"","kind=\"unknown\""),
        Replace(good,"<uicreator ","<uicreator unknown=\"x\" "),
        Replace(good,"<uicreator ","<uicreator xmlns=\"https://invalid.example/\" "),
        Xml::Encode(EntityKind::Widget,"project",{{"obj",json::array()}}),
        Xml::Encode(EntityKind::Project,"other",{{"obj",json::array()}}),
        Xml::Encode(EntityKind::Project,"project",{{"obj",{{{"oid",1},{"par",1},{"base",json::object()}}}}}),
    };
    const auto complete=good.rfind("</uicreator>")+std::strlen("</uicreator>");
    for(size_t n=0;n<complete;++n) invalid.push_back(good.substr(0,n));
    for(const auto& text:invalid) {
        lvgl_runtime_t* runtime=reinterpret_cast<lvgl_runtime_t*>(1); char error[64]{};
        CHECK(!lvgl_runtime_load_memory(text.data(),text.size(),nullptr,&runtime,error,sizeof error));
        CHECK(runtime==nullptr); CHECK(error[0]); CHECK(lv_screen_active()==previous);
    }
    const auto envelope=[](const char* value){return std::string("<uicreator format_version=\"1\" kind=\"project\" type=\"project\">")+value+"</uicreator>";};
    for(const auto* value:{"<integer>9223372036854775808</integer>","<unsigned>-1</unsigned>",
        "<unsigned>18446744073709551616</unsigned>","<real>NaN</real>","<real>1e9999</real>","<boolean>yes</boolean>",
        "<binary encoding=\"base64\">A</binary>","<binary encoding=\"base64\">AA=A</binary>",
        "<string>&external;</string>","<string><![CDATA[unsupported]]></string>","<null>text</null>",
        "<object><property name=\"duplicate\"><integer>1</integer></property><property name=\"duplicate\"><integer>2</integer></property></object>",
        "<object><property name=\"x\"><integer>1</integer><integer>2</integer></property></object>",
        "<array><property name=\"x\"><integer>1</integer></property></array>"}) Reject([&]{Xml::Decode(envelope(value));});
    auto bytes=Bytes(good); lvgl_runtime_options_t options{display.display,bytes.size()-1,true};
    lvgl_runtime_t* runtime=nullptr; char error[128]{};
    CHECK(!lvgl_runtime_load_memory(bytes.data(),bytes.size(),&options,&runtime,error,sizeof error)); CHECK(!runtime);
    CHECK(lv_screen_active()==previous);
    char guarded[]={'L','X','R'};
    CHECK(!lvgl_runtime_load_memory("<invalid>",9,nullptr,&runtime,guarded+1,1));
    CHECK(guarded[0]=='L'&&guarded[1]=='\0'&&guarded[2]=='R'); CHECK(!runtime);
}
std::vector<std::vector<uint32_t>> Exercise(Binary bytes, Display& display) {
    auto* previous=lv_screen_active(); auto runtime=Load(bytes); Release(bytes);
    CHECK(lvgl_runtime_screen_count(runtime.get())==2); CHECK(lvgl_runtime_variable_count(runtime.get())==2);
    CHECK(lvgl_runtime_history_count(runtime.get(),"level")==2);
    auto object=[&](int id){auto* value=lvgl_runtime_object(runtime.get(),0,id);CHECK(value);return value;};
    CHECK(lv_slider_get_value(object(1))==7); CHECK(std::string(lv_label_get_text(object(2)))=="7");
    CHECK(lv_obj_get_style_text_font(object(2),LV_PART_MAIN)==lvgl_runtime_font(runtime.get(),"fixture_font"));
    CHECK(lv_image_get_src(object(4))==lvgl_runtime_image(runtime.get(),"fixture_image")); Predictor(object(7));
    lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_INTEGER;value.integer_value=45;
    CHECK(lvgl_runtime_set_variable(runtime.get(),"level",&value));
    CHECK(std::string(lv_label_get_text(object(2)))=="45");
    lv_slider_set_value(object(1),61,LV_ANIM_OFF);lv_obj_send_event(object(1),LV_EVENT_VALUE_CHANGED,nullptr);
    CHECK(lvgl_runtime_get_variable(runtime.get(),"level",&value)&&value.integer_value==61);
    CHECK(lvgl_runtime_history_get(runtime.get(),"level",0,&value)&&value.integer_value==7);
    lv_textarea_set_text(object(3),"Typed \xc3\xa9 & < >");lv_obj_send_event(object(3),LV_EVENT_VALUE_CHANGED,nullptr);
    CHECK(lvgl_runtime_get_variable(runtime.get(),"caption",&value)&&std::string(value.string_value)=="Typed \xc3\xa9 & < >");
    lv_obj_send_event(object(3),LV_EVENT_FOCUSED,nullptr);
    CHECK(lv_advanced_keyboard_get_textarea(object(7))==object(3));
    lv_obj_send_event(object(9),LV_EVENT_FOCUSED,nullptr);
    CHECK(lv_advanced_keyboard_get_textarea(object(7))==object(9));lv_advanced_keyboard_close(object(7));
    std::vector<std::vector<uint32_t>> frames{display.Render()};
    lv_obj_send_event(object(8),LV_EVENT_CLICKED,nullptr);
    CHECK(lvgl_runtime_active_screen(runtime.get())==1);
    CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime.get(),1,20)))=="62");frames.push_back(display.Render());
    runtime.reset();CHECK(lv_screen_active()==previous);return frames;
}
void RuntimeParity() {
    Display display;const auto document=Project();
    const auto protobuf=Protobuf::Encode(EntityKind::Project,"project",document);
    const auto xml=Bytes(Xml::Encode(EntityKind::Project,"project",document));
    const auto baseline=Exercise(protobuf,display);CHECK(Exercise(xml,display)==baseline);
    CHECK(Exercise(Bytes(std::string("\xef\xbb\xbf \r\n")+WithoutDeclaration(std::string(xml.begin(),xml.end()))),display)==baseline);
}
// Optimized linkers may fold this into LVGL's own temporary deletion tracker.
// Screen loading must remove only the tracker registered for its stack slot.
void TrackedDelete(lv_event_t* event) { *static_cast<lv_obj_t**>(lv_event_get_user_data(event))=nullptr; }
void ScreenTracking() {
    auto* initial=lv_screen_active();auto* tracked=lv_obj_create(nullptr);
    lv_obj_add_event_cb(tracked,TrackedDelete,LV_EVENT_DELETE,&tracked);
    auto count=[&] {
        size_t matching=0;
        for(uint32_t i=0;i<lv_obj_get_event_count(tracked);++i) {
            auto* descriptor=lv_obj_get_event_dsc(tracked,i);
            if(lv_event_dsc_get_cb(descriptor)==TrackedDelete && lv_event_dsc_get_user_data(descriptor)==&tracked)++matching;
        }
        return matching;
    };
    lv_screen_load(tracked);CHECK(count()==1);
    lv_screen_load(initial);CHECK(count()==1);
    lv_obj_delete(tracked);CHECK(!tracked);
}
void Lazy() {
    Display display;auto* previous=lv_screen_active();
    ScreenTracking();
    for(bool xml:{false,true}) {
    for(uint32_t flags:{uint32_t(LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE),
        uint32_t(LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE|LVGL_RUNTIME_BORROW_ASSETS)}) {
        if(!xml&&(flags&LVGL_RUNTIME_BORROW_ASSETS))continue;
        auto bytes=xml?Bytes(Xml::Encode(EntityKind::Project,"project",Project())):
            Protobuf::Encode(EntityKind::Project,"project",Project());
        auto runtime=Load(bytes,flags);Release(bytes);
        CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==1);CHECK(!lvgl_runtime_screen(runtime.get(),1));
        Predictor(lvgl_runtime_object(runtime.get(),0,7));
        for(int cycle=0;cycle<3;++cycle) {
            auto* entry=lvgl_runtime_object(runtime.get(),0,9);CHECK(entry);lv_textarea_set_text(entry,"Retained after unload");
            CHECK(lvgl_runtime_show_screen(runtime.get(),1));Timers();
            if(lvgl_runtime_screen(runtime.get(),0))
                throw std::runtime_error(std::string(xml?"XML":"Protobuf")+" lazy unload did not complete: "+lvgl_runtime_error(runtime.get())+
                    " (active screen "+std::to_string(lvgl_runtime_active_screen(runtime.get()))+")");
            lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_INTEGER;value.integer_value=20+cycle;
            CHECK(lvgl_runtime_set_variable(runtime.get(),"level",&value));
            CHECK(lvgl_runtime_show_screen(runtime.get(),0));Timers();CHECK(!lvgl_runtime_screen(runtime.get(),1));
            CHECK(lv_slider_get_value(lvgl_runtime_object(runtime.get(),0,1))==20+cycle);
            CHECK(std::string(lv_textarea_get_text(lvgl_runtime_object(runtime.get(),0,9)))=="Retained after unload");
            Predictor(lvgl_runtime_object(runtime.get(),0,7));display.Render();
        }
        CHECK(!lvgl_runtime_unload_screen(runtime.get(),0));CHECK(lvgl_runtime_ensure_screen(runtime.get(),1));
        CHECK(lvgl_runtime_unload_screen(runtime.get(),1));runtime.reset();CHECK(lv_screen_active()==previous);
    }
    }
    lvgl_runtime_options_t options{display.display,0,false};
    auto bytes=Bytes(Xml::Encode(EntityKind::Project,"project",Project()));
    auto runtime=Load(bytes,LVGL_RUNTIME_LAZY_SCREENS,&options);Release(bytes);
    CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==0);CHECK(lv_screen_active()==previous);
    CHECK(lvgl_runtime_show_screen(runtime.get(),1));runtime.reset();CHECK(lv_screen_active()==previous);
}
void Filesystem() {
    Display display;auto* previous=lv_screen_active();
    auto bytes=Bytes(Xml::Encode(EntityKind::Project,"project",Project()));
    lv_fs_path_ex_t path;
    lv_fs_make_path_from_buffer(&path,LV_FS_MEMFS_LETTER,bytes.data(),uint32_t(bytes.size()),"xml");
    lvgl_runtime_t* raw=nullptr;char error[1024]{};
    if(!lvgl_runtime_load_file(reinterpret_cast<const char*>(&path),nullptr,&raw,error,sizeof error))throw std::runtime_error(error);
    Runtime runtime(raw,lvgl_runtime_destroy);Release(bytes);Predictor(lvgl_runtime_object(raw,0,7));display.Render();
    runtime.reset();CHECK(lv_screen_active()==previous);
}
void Fixture(const fs::path& output) {
    fs::create_directories(output);const auto document=Project();const auto text=document.dump(2);
    Write(output/"project.json",text.data(),text.size());
    const auto protobuf=Protobuf::Encode(EntityKind::Project,"project",document);Write(output/"project.pb",protobuf.data(),protobuf.size());
    const auto xml=Xml::Encode(EntityKind::Project,"project",document);Write(output/"project.xml",xml.data(),xml.size());
}
}
int main(int argc,char** argv) {
    try {
        CHECK(argc>=2);lv_init();
        const std::string test=argv[1];
        if(test=="codec")Codec();else if(test=="malformed")Malformed();else if(test=="runtime")RuntimeParity();
        else if(test=="lazy")Lazy();else if(test=="filesystem")Filesystem();
        else if(test=="fixture"&&argc==3)Fixture(fs::u8path(argv[2]));else throw std::runtime_error("Unknown XML test case");
        lv_deinit();std::cout<<"XML "<<test<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
