#include "FontConverter.h"
#include "../Utils/ApplicationPaths.h"
#include "../../3rdParty/JSON/json.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#endif

namespace FontConversion {
namespace {
bool Identifier(const std::string& name) {
    if(name.empty() || !((name[0]>='A'&&name[0]<='Z')||(name[0]>='a'&&name[0]<='z')||name[0]=='_'))return false;
    for(unsigned char c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
    static const std::set<std::string> reserved={"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","inline","int","long","register","restrict","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while","bool","true","false","NULL"};
    return !reserved.count(name) && name.rfind("__",0)!=0;
}
template<class T> bool OneOf(T value,std::initializer_list<T> values) {return std::find(values.begin(),values.end(),value)!=values.end();}
std::string Read(const std::filesystem::path& path) { std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read "+path.u8string());return {(std::istreambuf_iterator<char>(in)),{}}; }
void Write(const std::filesystem::path& path,const std::string& data) {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(data.data(),data.size());out.close();if(!out)throw std::runtime_error("Cannot write "+path.u8string());}
std::filesystem::path ExecutableDirectory() {
    return ApplicationPaths::ResourceDirectory();
}
#ifdef _WIN32
std::wstring Quote(const std::wstring& text) {
    std::wstring result=L"\"";size_t slashes=0;
    for(auto c:text) {if(c==L'\\'){++slashes;continue;}if(c==L'"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}
    result.append(slashes*2,L'\\');return result+L'"';
}
#endif
int Run(const std::filesystem::path& runtime,const std::filesystem::path& script,const std::filesystem::path& request,const std::filesystem::path& log,const std::atomic<bool>* cancel) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
#ifdef _WIN32
    SECURITY_ATTRIBUTES attributes{sizeof attributes,nullptr,TRUE};
    HANDLE output=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&attributes,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create conversion log");
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOW startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    startup.hStdOutput=output;startup.hStdError=output;startup.hStdInput=input;
    PROCESS_INFORMATION process{};auto command=Quote(runtime.wstring())+L" "+Quote(script.wstring())+L" "+Quote(request.wstring());
    const bool started=CreateProcessW(runtime.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE;
    const auto startError=started?0:GetLastError();
    CloseHandle(output);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
    if(!started)throw std::runtime_error("Cannot start the bundled font converter (Windows error "+std::to_string(startError)+")");
    // The converter should also stop if the editor itself exits during a job.
    HANDLE processJob=CreateJobObjectW(nullptr,nullptr);
    if(processJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(processJob,JobObjectExtendedLimitInformation,&limits,sizeof limits)||!AssignProcessToJobObject(processJob,process.hProcess)) {CloseHandle(processJob);processJob=nullptr;}
    }
    CloseHandle(process.hThread);bool stopped=false;
    while(WaitForSingleObject(process.hProcess,100)==WAIT_TIMEOUT) if((cancel&&cancel->load())||std::chrono::steady_clock::now()>deadline) {TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);stopped=true;break;}
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);if(processJob)CloseHandle(processJob);
    if(stopped)throw std::runtime_error(cancel&&cancel->load()?"Conversion cancelled":"Font conversion timed out");return static_cast<int>(code);
#else
    auto child=fork();if(child<0)throw std::runtime_error("Cannot start converter");
    if(!child) {int fd=open(log.c_str(),O_CREAT|O_WRONLY|O_TRUNC,0600);if(fd<0)_exit(127);dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);close(fd);execl(runtime.c_str(),runtime.c_str(),script.c_str(),request.c_str(),nullptr);_exit(127);}
    int status=0;while(waitpid(child,&status,WNOHANG)==0) {if((cancel&&cancel->load())||std::chrono::steady_clock::now()>deadline){kill(child,SIGKILL);waitpid(child,&status,0);throw std::runtime_error("Conversion cancelled or timed out");}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
    return WIFEXITED(status)?WEXITSTATUS(status):1;
#endif
}
}
std::string SymbolName(const std::filesystem::path& source,int size) {
    std::string name="ui_"+source.stem().u8string()+"_"+std::to_string(size);for(char& c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')))c='_';return name;
}
int RunTool(const std::filesystem::path& runtime,const std::filesystem::path& script,const std::filesystem::path& request,const std::filesystem::path& log,const std::atomic<bool>* cancel) {return Run(runtime,script,request,log,cancel);}
std::vector<std::string> Arguments(const Options& o,bool preview) {
    if(o.sources.empty())throw std::invalid_argument("Add at least one source font");
    if(o.size<1||o.size>4096)throw std::invalid_argument("Font size must be between 1 and 4096 pixels");
    if(!OneOf(o.bpp,{1,2,3,4,8}))throw std::invalid_argument("Choose 1, 2, 3, 4, or 8 bits per pixel");
    if(!OneOf(o.stride,{0,1,4,8,16,32,64})||!OneOf(o.align,{1,4,8,16,32,64,128,256,512,1024}))throw std::invalid_argument("Invalid bitmap alignment");
    if(o.stride&&(o.compress||o.bpp==3))throw std::invalid_argument("Stride alignment requires compression off and 1, 2, 4, or 8 bpp");
    if(!OneOf(o.smoothing,{0,1,2}))throw std::invalid_argument("Invalid subpixel rendering mode");
    if(o.format!="lvgl"&&o.format!="bin"&&o.format!="dump")throw std::invalid_argument("Choose LVGL C, binary, or glyph dump output");
    if(o.output.empty())throw std::invalid_argument("Choose an output file or folder");
    if(!Identifier(o.name)||(!o.fallback.empty()&&!Identifier(o.fallback)))throw std::invalid_argument("Font and fallback names must be valid C identifiers");
    if(o.include.empty()||o.include.find_first_of("\"\r\n")!=std::string::npos)throw std::invalid_argument("Invalid LVGL include path");
    auto output=std::filesystem::absolute(o.output).lexically_normal();
    if(o.format=="lvgl" && output.extension()!=".c")throw std::invalid_argument("LVGL C output must have a .c extension");
    std::vector<std::string> args;
    auto add=[&](const char* flag,const std::string& value){args.emplace_back(flag);args.push_back(value);};
    for(const auto& source:o.sources) {
        if(!std::filesystem::is_regular_file(source.path))throw std::invalid_argument("Font file not found: "+source.path.u8string());
        if(std::filesystem::exists(output)&&std::filesystem::equivalent(output,source.path))throw std::invalid_argument("The output would overwrite a source font");
        if(source.ranges.empty()&&source.symbols.empty())throw std::invalid_argument("Each source needs Unicode ranges or symbols");
        if(source.hinting<0||source.hinting>2)throw std::invalid_argument("Invalid font hinting mode");
        add("--font",std::filesystem::absolute(source.path).u8string());
        if(!source.ranges.empty())add("--range",source.ranges);
        if(!source.symbols.empty())add("--symbols",source.symbols);
        if(source.hinting==1)args.emplace_back("--autohint-off");if(source.hinting==2)args.emplace_back("--autohint-strong");
    }
    add("--size",std::to_string(o.size));add("--bpp",std::to_string(preview&&o.bpp==3?4:o.bpp));add("--format",preview?"bin":o.format);
    add("--output",preview?"preview.bin":output.u8string());
    if(preview||!o.compress)args.emplace_back("--no-compress");
    if(!o.prefilter)args.emplace_back("--no-prefilter");if(!o.kerning)args.emplace_back("--no-kerning");
    if(o.fastKerning)args.emplace_back("--force-fast-kern-format");if(o.colorInfo)args.emplace_back("--use-color-info");
    if(o.smoothing)args.emplace_back(o.smoothing==1?"--lcd":"--lcd-v");
    add("--stride",std::to_string(preview?0:o.stride));add("--align",std::to_string(preview?1:o.align));
    if(!preview&&o.format=="lvgl") {add("--lv-font-name",o.name);add("--lv-include",o.include);if(!o.fallback.empty())add("--lv-fallback",o.fallback);}
    if(!preview&&!o.relativeRoot.empty())add("--relative-project-path",o.relativeRoot.u8string());
    if(!preview&&o.fullInfo)args.emplace_back("--full-info");return args;
}
Result Convert(const Options& options,const std::atomic<bool>* cancel) {
    Result result;std::filesystem::path job;bool ownsJob=false;
    try {
        auto args=Arguments(options);auto preview=Arguments(options,true);
        auto tools=ExecutableDirectory()/"font-converter";
        if(!std::filesystem::exists(tools/"bridge.js"))throw std::runtime_error("Font converter files are missing. Rebuild UICreator with font converter assets enabled.");
        auto runtime=tools/
#ifdef _WIN32
            "node.exe";
#else
            "node";
#endif
        if(!std::filesystem::is_regular_file(runtime))throw std::runtime_error("The bundled Node runtime is missing");
        static std::atomic<uint64_t> sequence{0};
        job=std::filesystem::temp_directory_path()/("uicreator-font-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++));
        if(!std::filesystem::create_directory(job))throw std::runtime_error("Cannot create font conversion workspace");
        ownsJob=true;
        Write(job/"request.json",nlohmann::json{{"args",args},{"previewArgs",preview}}.dump());
        if(Run(runtime,tools/"bridge.js",job/"request.json",job/"conversion.log",cancel)!=0) {
            auto log=Read(job/"conversion.log");if(log.size()>6000)log=log.substr(log.size()-6000);throw std::runtime_error(log.empty()?"Font conversion failed":log);
        }
        if(cancel&&cancel->load())throw std::runtime_error("Conversion cancelled");
        auto manifest=nlohmann::json::parse(Read(job/"result.json"));
        const auto previewData=Read(job/"preview.bin");result.preview.assign(previewData.begin(),previewData.end());
        const auto output=std::filesystem::absolute(options.output).lexically_normal();
        for(const auto& file:manifest.at("files")) {
            const auto destination=std::filesystem::u8path(file.at("destination").get<std::string>()).lexically_normal();
            const auto relative=destination.lexically_relative(output);
            if(options.format=="dump") {if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Invalid glyph dump destination");}
            else if(destination!=output)throw std::runtime_error("Unexpected font output destination");
            auto data=Read(job/file.at("staged").get<std::string>());
            std::filesystem::create_directories(destination.parent_path());Write(destination,data);result.files.push_back(destination);result.bytes+=data.size();
        }
        if(options.format=="lvgl") {
            auto header=output;header.replace_extension(".h");
            Write(header,"#pragma once\n#include \""+options.include+"\"\n#ifdef __cplusplus\nextern \"C\" {\n#endif\nLV_FONT_DECLARE("+options.name+");\n#ifdef __cplusplus\n}\n#endif\n");result.files.push_back(header);
        }
        result.success=true;result.message="Converted "+std::to_string(options.sources.size())+" font source(s), "+std::to_string(result.bytes)+" output bytes.";
    } catch(const std::exception& error) {result.message=error.what();}
    if(ownsJob && job.parent_path()==std::filesystem::temp_directory_path() && job.filename().u8string().rfind("uicreator-font-",0)==0) {
        std::error_code ignored;std::filesystem::remove_all(job,ignored);
    }
    return result;
}
}
