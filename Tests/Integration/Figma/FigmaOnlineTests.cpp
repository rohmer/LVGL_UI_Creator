#include "../../../UICreator/Imports/FigmaOnline.h"
#include <FigmaImporter.h>
#include <CCodeGenerator.h>
#include <MicroPythonCodeGenerator.h>
#include <Serialization/Project.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/Protobuf.h>
#include <lvgl_runtime.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(value) do { if(!(value)) throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " #value); } while(0)
namespace {
using json = nlohmann::json;
namespace Online = FigmaOnline;
namespace fs = std::filesystem;
constexpr const char* Token = "figd_test_token_that_must_never_be_saved";
constexpr const char* Key = "AbC123xyz";
std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary); CHECK(input);
    return {std::istreambuf_iterator<char>(input), {}};
}
json Box(int x, int y, int width, int height) { return {{"x",x},{"y",y},{"width",width},{"height",height}}; }
json Vector(const std::string& id, const std::string& name, int x=112, int y=218) {
    return {{"id",id},{"type","VECTOR"},{"name",name},{"absoluteBoundingBox",Box(x,y,32,24)}};
}
json Source() {
    auto first = json{{"id","1:1"},{"type","FRAME"},{"name","Home"},{"absoluteBoundingBox",Box(100,200,320,240)},
        {"children",json::array({
            {{"id","2:1"},{"type","TEXT"},{"name","Heading"},{"characters","Online design"},
                {"absoluteBoundingBox",Box(116,216,150,28)},{"style",{{"fontFamily","Inter"},{"fontSize",14}}}},
            Vector("2:3","Logo")})}};
    auto hidden = Vector("2:5","Hidden logo"); hidden["visible"] = false; first["children"].push_back(hidden);
    auto second = json{{"id","1:2"},{"type","FRAME"},{"name","About"},{"absoluteBoundingBox",Box(500,200,320,240)},
        {"children",json::array({Vector("4:1","About logo",512,218)})}};
    return {{"name","Online fixture"},{"version","version-17"},{"document",{{"id","0:0"},{"type","DOCUMENT"},{"name","Design"},
        {"children",json::array({{{"id","0:1"},{"type","CANVAS"},{"name","Page 1"},{"children",json::array({first,second})}}})}}}};
}
std::string Decode(const std::string& value) {
    std::string output;
    auto hex=[](char c) { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; };
    for(size_t i=0;i<value.size();++i) {
        if(value[i]=='%' && i+2<value.size() && hex(value[i+1])>=0 && hex(value[i+2])>=0) {
            output.push_back(static_cast<char>((hex(value[i+1])<<4)|hex(value[i+2])));i+=2;
        } else output.push_back(value[i]=='+'?' ':value[i]);
    }
    return output;
}
std::string Query(const std::string& url, const std::string& name) {
    const auto query = url.find('?'); if(query==std::string::npos)return {};
    size_t start=query+1;
    while(start<url.size()) {
        const auto end=url.find('&',start); const auto equal=url.find('=',start);
        const auto stop=end==std::string::npos?url.size():end;
        if(equal<stop && Decode(url.substr(start,equal-start))==name) return Decode(url.substr(equal+1,stop-equal-1));
        start=stop+1;
    }
    return {};
}
std::vector<std::string> Ids(const std::string& url) {
    const auto value=Query(url,"ids");std::vector<std::string> result;size_t start=0;
    while(start<value.size()) {const auto end=value.find(',',start);result.push_back(value.substr(start,end==std::string::npos?value.size()-start:end-start));start=end==std::string::npos?value.size():end+1;}
    return result;
}
Online::HttpResponse Response(int status, const std::string& body, std::map<std::string,std::string> headers={}) {
    Online::HttpResponse response;response.status=status;response.body=body;response.headers=std::move(headers);return response;
}
struct Server {
    json source=Source();
    std::vector<Online::HttpRequest> requests;
    std::string png=Read(fs::u8path(UICREATOR_FIGMA_SOURCE_ROOT)/"Tests/Fixtures/Images/sample.png");
    Online::HttpResponse operator()(const Online::HttpRequest& request,const std::atomic<bool>*) {
        requests.push_back(request);
        if(request.url.find("https://api.figma.com/v1/files/")==0) {
            CHECK(request.token==Token);
            if(request.url.find("/nodes?")!=std::string::npos) {
                const auto selected=Ids(request.url);CHECK(selected.size()==1);
                const auto& first=source["document"]["children"][0]["children"][0];
                return Response(200,json{{"name","Selected node"},{"version","version-17"},{"nodes",{{selected[0],{{"document",first}}}}}}.dump());
            }
            return Response(200,source.dump());
        }
        if(request.url.find("https://api.figma.com/v1/images/")==0) {
            CHECK(request.token==Token);json images=json::object();
            const auto ids=Ids(request.url);CHECK(!ids.empty());CHECK(ids.size()<=50);
            for(const auto& id:ids)images[id]="https://figma-render.example.com/"+id+".png?signature=temporary";
            return Response(200,json{{"images",images}}.dump());
        }
        CHECK(request.url.find("https://figma-render.example.com/")==0);
        CHECK(request.token.empty());
        return Response(200,png);
    }
    Online::Transport Transport() { return [this](const auto& request,const auto* cancel){return (*this)(request,cancel);}; }
};
template<class Action> std::string Rejected(Action action) {
    try { action(); } catch(const std::exception& error) { return error.what(); }
    throw std::runtime_error("Expected the online import to reject invalid input");
}
void NoSecret(const std::string& text) { CHECK(text.find(Token)==std::string::npos);CHECK(text.find("RAW_RESPONSE_SECRET")==std::string::npos); }
void Urls() {
    for(const auto* url:{"https://www.figma.com/design/AbC123xyz/Screen","https://figma.com/file/AbC123xyz/Screen?mode=design","https://www.figma.com/proto/AbC123xyz/Screen"}) {
        const auto location=Online::ParseUrl(url);CHECK(location.fileKey==Key);CHECK(location.nodeId.empty());
    }
    CHECK(Online::ParseUrl("https://www.figma.com/design/AbC123xyz/Screen?node-id=2-3&t=foo").nodeId=="2:3");
    CHECK(Online::ParseUrl("https://www.figma.com/design/AbC123xyz/Screen?node-id=2%3A3").nodeId=="2:3");
    for(const auto* url:{"","http://www.figma.com/design/AbC123xyz/Screen","https://figma.com.evil.example/design/AbC123xyz/Screen",
        "https://www.figma.com@evil.example/design/AbC123xyz/Screen","https://user@www.figma.com/design/AbC123xyz/Screen",
        "https://www.figma.com:8443/design/AbC123xyz/Screen","file:///tmp/figma.json","https://www.figma.com/design//Screen",
        "https://www.figma.com/design/AbC123xyz/Screen?node-id=%0d%0aX-Test:yes","https://www.figma.com/design/AbC123xyz/Screen\nInjected"})
        CHECK(!Rejected([&]{Online::ParseUrl(url);}).empty());
    // Native HTTPS policy checks fail before a socket is opened.
    for(const auto* url:{"http://api.figma.com/v1/files/a","file:///tmp/file","https://api.figma.com:444/v1/files/a",
        "https://person@api.figma.com/v1/files/a","https://api.figma.com/v1/files/a#fragment","https:///missing-host"}) {
        Online::HttpRequest request;request.url=url;request.token=Token;
        NoSecret(Rejected([&]{Online::Get(request);}));
    }
    Online::HttpRequest request;request.url="https://api.figma.com/v1/files/a";request.token="secret\r\nInjected: yes";
    CHECK(!Rejected([&]{Online::Get(request);}).empty());
}
void Workflow() {
    Server server;std::vector<std::string> progress;
    auto design=Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,
        [&](const auto& message){NoSecret(message);progress.push_back(message);},server.Transport());
    CHECK(design.location.fileKey==Key);CHECK(FigmaImport::Frames(design.document).size()==2);
    CHECK(server.requests.size()==1);CHECK(server.requests[0].url.find("/v1/files/AbC123xyz")!=std::string::npos);
    FigmaImport::Options options;options.nodeIds={"1:1"};
    const auto bundle=Online::AddRenders(design,Token,options,nullptr,{},server.Transport());
    CHECK(server.requests.size()==3);CHECK(bundle.at("uicreatorAssets").size()==1);
    CHECK(bundle.at("uicreatorAssets").contains("2:3"));
    const auto& render=server.requests[1];CHECK(Ids(render.url)==std::vector<std::string>{"2:3"});
    CHECK(Query(render.url,"format")=="png");CHECK(Query(render.url,"version")=="version-17");
    CHECK(Query(render.url,"use_absolute_bounds")=="true");
    CHECK(render.maxBytes>0 && server.requests[2].maxBytes>0);
    const auto result=FigmaImport::Convert(bundle,options);CHECK(result.project.at("screens").size()==1);CHECK(result.project.at("images").size()==1);
    bool label=false,image=false;
    for(const auto& object:result.project.at("obj")) {label|=object.contains("text");image|=object.contains("img");}
    CHECK(label&&image);NoSecret(bundle.dump());NoSecret(result.project.dump());
    design.document=bundle;
    const auto before=server.requests.size();const auto again=Online::AddRenders(design,Token,options,nullptr,{},server.Transport());
    CHECK(server.requests.size()==before);CHECK(again.at("uicreatorAssets")==bundle.at("uicreatorAssets"));
    options.includeHidden=true;
    const auto all=Online::AddRenders(design,Token,options,nullptr,{},server.Transport());
    CHECK(all.at("uicreatorAssets").contains("2:5"));CHECK(server.requests.size()==before+2);
    Server selected;
    const auto node=Online::Load("https://www.figma.com/design/AbC123xyz/Screen?node-id=1-1",Token,nullptr,{},selected.Transport());
    CHECK(selected.requests.size()==1);CHECK(selected.requests[0].url.find("/nodes?")!=std::string::npos);CHECK(Ids(selected.requests[0].url)==std::vector<std::string>{"1:1"});
    CHECK(FigmaImport::Frames(node.document).size()==1);CHECK(!progress.empty());
}
void Failures() {
    for(const int code:{401,403,404,429,500}) {
        size_t requests=0;
        const auto error=Rejected([&]{Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,{},
            [&](const auto&,const auto*){++requests;return Response(code,std::string("RAW_RESPONSE_SECRET ")+Token,{{"retry-after","120"}});});});
        CHECK(requests==1);NoSecret(error);CHECK(!error.empty());
        if(code==429)CHECK(error.find("120")!=std::string::npos);
    }
    NoSecret(Rejected([&]{Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,{},
        [](const auto&,const auto*){return Response(200,std::string("{RAW_RESPONSE_SECRET ")+Token);});}));
    Server server;const auto design=Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,{},server.Transport());
    FigmaImport::Options options;options.nodeIds={"1:1"};
    for(const auto& body:{json{{"images",{{"2:3",nullptr}}}}.dump(),json{{"images",json::object()}}.dump(),std::string("not JSON RAW_RESPONSE_SECRET")}) {
        const auto error=Rejected([&]{Online::AddRenders(design,Token,options,nullptr,{},[&](const auto&,const auto*){return Response(200,body);});});
        NoSecret(error);
    }
    auto missing=design;missing.location.fileKey.clear();
    CHECK(!Rejected([&]{Online::AddRenders(missing,Token,options,nullptr,{},server.Transport());}).empty());
    size_t calls=0;
    CHECK(!Rejected([&]{Online::Load("https://www.figma.com/design/AbC123xyz/Screen","",nullptr,{},
        [&](const auto&,const auto*){++calls;return Response(200,Source().dump());});}).empty());
    CHECK(calls==0);
}
void Cancellation() {
    std::atomic<bool> cancel{true};size_t requests=0;
    auto transport=[&](const auto&,const auto*){++requests;return Response(200,Source().dump());};
    CHECK(!Rejected([&]{Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,&cancel,{},transport);}).empty());CHECK(requests==0);
    Online::HttpRequest request;request.url="https://api.figma.com/v1/files/a";request.token=Token;
    CHECK(!Rejected([&]{Online::Get(request,&cancel);}).empty());
    cancel=false;
    CHECK(!Rejected([&]{Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,&cancel,{},[&](const auto&,const auto*){cancel=true;++requests;return Response(200,Source().dump());});}).empty());
    CHECK(requests==1);
    Server server;auto design=Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,{},server.Transport());
    cancel=false;FigmaImport::Options options;options.nodeIds={"1:1"};requests=0;
    CHECK(!Rejected([&]{Online::AddRenders(design,Token,options,&cancel,{},[&](const auto&,const auto*){++requests;cancel=true;return Response(200,json{{"images",{{"2:3","https://figma-render.example.com/a.png"}}}}.dump());});}).empty());
    CHECK(requests==1);
}
void Batching() {
    Server server;auto& children=server.source["document"]["children"][0]["children"][0]["children"];
    children=json::array();for(int i=0;i<51;++i)children.push_back(Vector("10:"+std::to_string(i),"Icon "+std::to_string(i)));
    const auto design=Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,{},server.Transport());
    FigmaImport::Options options;options.nodeIds={"1:1"};const auto bundle=Online::AddRenders(design,Token,options,nullptr,{},server.Transport());
    CHECK(bundle.at("uicreatorAssets").size()==51);
    size_t renderRequests=0,downloads=0;std::set<std::string> rendered;
    for(const auto& request:server.requests) {
        if(request.url.find("/v1/images/")!=std::string::npos) {++renderRequests;for(const auto& id:Ids(request.url))CHECK(rendered.insert(id).second);}
        else if(request.token.empty())++downloads;
    }
    CHECK(renderRequests==2 && downloads==51 && rendered.size()==51);
}
void Limits() {
    const auto link="https://www.figma.com/design/AbC123xyz/Screen";
    CHECK(!Rejected([&]{Online::Load(link,Token,nullptr,{},[](const auto& request,const auto*) {
        return Response(200,std::string(request.maxBytes+1,'x'));
    });}).empty());
    Server server;const auto design=Online::Load(link,Token,nullptr,{},server.Transport());
    FigmaImport::Options options;options.nodeIds={"1:1"};
    CHECK(!Rejected([&]{Online::AddRenders(design,Token,options,nullptr,{},[&](const auto& request,const auto* cancel) {
        if(request.token.empty()) return Response(200,std::string(request.maxBytes+1,'x'));
        return server(request,cancel);
    });}).empty());
    auto oversized=server.png;CHECK(oversized.size()>24);oversized[16]=0;oversized[17]=0;oversized[18]=0x10;oversized[19]=0x01;
    CHECK(!Rejected([&]{Online::AddRenders(design,Token,options,nullptr,{},[&](const auto& request,const auto* cancel) {
        if(request.token.empty()) return Response(200,oversized);
        return server(request,cancel);
    });}).empty());
    const auto before=server.requests.size();options.scale=std::numeric_limits<double>::quiet_NaN();
    CHECK(!Rejected([&]{Online::AddRenders(design,Token,options,nullptr,{},server.Transport());}).empty());CHECK(server.requests.size()==before);
}
struct Display {
    lv_display_t* value=nullptr;std::vector<uint32_t> pixels=std::vector<uint32_t>(640*480);
    Display() {value=lv_display_create(640,480);CHECK(value);lv_display_set_color_format(value,LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_buffers(value,pixels.data(),nullptr,static_cast<uint32_t>(pixels.size()*4),LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(value,[](lv_display_t* display,const lv_area_t*,uint8_t*){lv_display_flush_ready(display);});}
    ~Display(){lv_display_delete(value);}
};
void RuntimeExports(const fs::path& directory) {
    Server server;const auto design=Online::Load("https://www.figma.com/design/AbC123xyz/Screen",Token,nullptr,{},server.Transport());
    const auto bundle=Online::AddRenders(design,Token,{},nullptr,{},server.Transport());const auto result=FigmaImport::Convert(bundle);
    CHECK(result.project.at("screens").size()==2);Display display;
    const auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",result.project);
    lvgl_runtime_t* raw=nullptr;char error[1024]{};
    CHECK(lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&raw,error,sizeof error));
    std::unique_ptr<lvgl_runtime_t,decltype(&lvgl_runtime_destroy)> runtime(raw,lvgl_runtime_destroy);
    CHECK(lvgl_runtime_screen_count(runtime.get())==2);lv_refr_now(display.value);runtime.reset();
    fs::create_directories(directory);
    CCodeGenerator c;CHECK(c.LoadProject(result.project));if(!c.ExportProject((directory/"c").u8string()))throw std::runtime_error(c.GetLastError());
    MicroPythonCodeGenerator python;CHECK(python.LoadProject(result.project));if(!python.ExportProject((directory/"micropython").u8string()))throw std::runtime_error(python.GetLastError());
    CHECK(fs::file_size(directory/"c/ui.c")>0);CHECK(fs::file_size(directory/"micropython/ui.py")>0);
    NoSecret(Read(directory/"c/ui.c"));NoSecret(Read(directory/"micropython/ui.json"));
}
}
int main(int argc,char** argv) {
    try {
        CHECK(argc>=2);lv_init();const std::string test=argv[1];
        if(test=="urls")Urls();else if(test=="workflow")Workflow();else if(test=="failures")Failures();
        else if(test=="cancellation")Cancellation();else if(test=="batching")Batching();else if(test=="limits")Limits();
        else if(test=="exports" && argc==3)RuntimeExports(fs::u8path(argv[2]));else throw std::runtime_error("Unknown Figma online test case");
        lv_deinit();std::cout<<"Figma online "<<test<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
