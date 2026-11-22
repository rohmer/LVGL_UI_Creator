#include "CCodeGenerator.h"
#include "MicroPythonCodeGenerator.h"
#include "XmlCodeGenerator.h"
#include "ProjectAssets.h"
#include "Targets/Target.h"
#include <Serialization/Protobuf.h>
#include <Serialization/RuntimeFeatures.h>
#include <Serialization/Xml.h>
#include <Serialization/ProjectFonts.h>
#include <Serialization/ProjectImages.h>
#include <Serialization/LVVariable.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cctype>

namespace {
using json=nlohmann::json;
void Usage(std::ostream& out) {
    out<<"LVGLCompiler - compile LVGL UI Creator projects for LVGL 9.6\n"
       <<"Usage: LVGLCompiler project.json|project.pb|project.xml --format c|protobuf|micropython|xml --output path\n"
       <<"  -f, --format FORMAT     c, protobuf (pb alias), micropython, xml (default: c)\n"
       <<"  -o, --output path       Source directory or Protobuf output file\n"
       <<"  --target VERSION        LVGL target: 9.6, 9.5, 9.3, 9.2, 9.1, 8.3 (default 9.6)\n"
       <<"                          MicroPython is fixed to the official binding's LVGL 9.3\n"
       <<"  -h, --help              Show this help\n";
}
json Load(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot open input project: "+path.u8string());
    auto extension=path.extension().u8string();for(auto& c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
    json project;
    if(extension==".pb") {
        Serialization::Binary bytes((std::istreambuf_iterator<char>(file)),{});auto record=Serialization::Protobuf::Decode(bytes);
        if(record.kind!=Serialization::EntityKind::Project||record.type!="project")throw std::invalid_argument("Input must be a project Protobuf record");
        project=std::move(record.data);
    }else if(extension==".xml") {
#if LVSERIAL_USE_XML
        const std::string bytes((std::istreambuf_iterator<char>(file)),{});auto record=Serialization::Xml::Decode(bytes.data(),bytes.size());
        if(record.kind!=Serialization::EntityKind::Project||record.type!="project")throw std::invalid_argument("Input must be a UI Creator XML project");
        project=std::move(record.data);
#else
        throw std::invalid_argument("XML support is disabled in this build");
#endif
    }else project=json::parse(file);
    if(!project.is_object())throw std::invalid_argument("Project must be a JSON object");
    const auto version=project.value("formatVersion",1);if(version<1||version>2)throw std::invalid_argument("Unsupported project format version");
    auto screen=[](const json& value){if(!value.is_object()||!value.contains("obj")||!value["obj"].is_array())throw std::invalid_argument("Each screen must contain an obj array");for(const auto& widget:value["obj"])if(!widget.is_object()||!widget.contains("base")||!widget["base"].is_object())throw std::invalid_argument("Each widget must contain a base object");};
    if(project.contains("screens")) {
        const auto& screens=project["screens"];if(!screens.is_array()||screens.empty())throw std::invalid_argument("Project screens must be a nonempty array");
        for(const auto& item:screens)screen(item);
        if(project.value("activeScreen",0)<0||project.value("activeScreen",0)>=screens.size())throw std::invalid_argument("Invalid active screen index");
    }else screen(project);
    Serialization::LVVariable::NormalizeList(project.value("variables",json::array()));
    Serialization::ProjectFonts::References(project,project.value("fonts",json::array()),false);
    Serialization::ProjectImages::References(project,project.value("images",json::array()),false);
    return project;
}
int Run(const std::vector<std::string>& args) {
    std::string format="c",output,target="9.6";std::vector<std::string> positional;bool options=true,targetSelected=false;
    for(size_t i=1;i<args.size();++i) {
        const auto& arg=args[i];
        if(options&&(arg=="--help"||arg=="-h")){Usage(std::cout);return 0;}
        if(options&&arg=="--"){options=false;continue;}
        if(options&&(arg=="--target"||arg=="--format"||arg=="-f"||arg=="--output"||arg=="-o")) {
            if(i+1==args.size()){std::cerr<<"Missing value for "<<arg<<'\n';return 2;}
            if(arg=="--target"){target=args[++i];targetSelected=true;}else if(arg=="--format"||arg=="-f")format=args[++i];else {if(!output.empty()){std::cerr<<"Specify output once\n";return 2;}output=args[++i];}continue;
        }
        if(options&&!arg.empty()&&arg[0]=='-'){std::cerr<<"Unknown option: "<<arg<<'\n';return 2;}
        positional.push_back(arg);
    }
    if(positional.size()==2&&output.empty()){output=positional.back();positional.pop_back();}
    if(positional.size()!=1||output.empty()||(format!="c"&&format!="protobuf"&&format!="pb"&&format!="micropython"&&format!="xml")){Usage(std::cerr);return 2;}
    try {
        CGeneration::ValidateTarget(target);
        auto input=std::filesystem::u8path(positional[0]),destination=std::filesystem::u8path(output);
        std::error_code error;if(std::filesystem::equivalent(input,destination,error))throw std::invalid_argument("Output cannot overwrite the input project");
        auto project=Load(input);
        if(format=="xml") {
            XmlCodeGenerator generator;generator.SetTargetVersion(target);generator.SetSourceDirectory(std::filesystem::absolute(input).parent_path().u8string());
            if(!generator.LoadProject(std::move(project))||!generator.ExportProject(output))throw std::runtime_error(generator.GetLastError());
            std::cout<<"Generated UI Creator XML project in "<<output<<'\n';
        }else if(format=="micropython") {
            if(targetSelected&&target!="9.3")throw std::invalid_argument("MicroPython uses the official binding's LVGL 9.3 target");
            MicroPythonCodeGenerator generator;generator.SetSourceDirectory(std::filesystem::absolute(input).parent_path().u8string());
            if(!generator.LoadProject(std::move(project))||!generator.ExportProject(output))throw std::runtime_error(generator.GetLastError());
            std::cout<<"Generated MicroPython project in "<<output<<'\n';
        }else if(format=="c") {
            CCodeGenerator generator;generator.SetTargetVersion(target);generator.SetSourceDirectory(std::filesystem::absolute(input).parent_path().u8string());
            if(!generator.LoadProject(std::move(project))||!generator.ExportProject(output))throw std::runtime_error(generator.GetLastError());
            std::cout<<"Generated C project in "<<output<<'\n';
        }else {
            CGeneration::EmbedLocalImages(project,std::filesystem::absolute(input).parent_path());
            project["runtimeTarget"]=target;
            auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",project);
            if(!destination.parent_path().empty())std::filesystem::create_directories(destination.parent_path());
            std::ofstream file(destination,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Cannot open Protobuf output: "+output);
            file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();if(!file)throw std::runtime_error("Could not write Protobuf output: "+output);
            std::cout<<"Generated "<<bytes.size()<<" Protobuf bytes in "<<output<<'\n';
        }
        return 0;
    }catch(const std::exception& error){std::cerr<<"LVGLCompiler: "<<error.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(std::filesystem::path(argv[i]).u8string());return Run(args);}
#else
int main(int argc,char** argv){return Run(std::vector<std::string>(argv,argv+argc));}
#endif
