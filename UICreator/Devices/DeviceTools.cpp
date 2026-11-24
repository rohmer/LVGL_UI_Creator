#include "DeviceTools.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace DeviceTools {
namespace {
std::string Env(const char* key) { const auto* p=std::getenv(key);return p?p:""; }
bool Exists(const std::filesystem::path& p) {std::error_code ec;return !p.empty()&&std::filesystem::is_regular_file(p,ec);}
std::string Text(const std::filesystem::path& p) {return p.lexically_normal().make_preferred().u8string();}
void Require(bool condition,const char* message){if(!condition)throw std::invalid_argument(message);}
std::filesystem::path Find(const std::string& name) {
    const auto path=Env("PATH");
#ifdef _WIN32
    const char separator=';';const std::string suffix=".exe";
#else
    const char separator=':';const std::string suffix;
#endif
    size_t start=0;while(start<=path.size()) {const auto end=path.find(separator,start);auto dir=path.substr(start,end-start);if(dir.size()>1&&dir.front()=='"'&&dir.back()=='"')dir=dir.substr(1,dir.size()-2);if(!dir.empty()){auto p=std::filesystem::u8path(dir)/name;if(Exists(p))return p;if(Exists(p.string()+suffix))return p.string()+suffix;}if(end==std::string::npos)break;start=end+1;}return {};
}
void AddPath(std::string& path,const std::filesystem::path& p) {
    std::error_code ec;if(!p.empty()&&std::filesystem::is_directory(p,ec)) {
#ifdef _WIN32
        constexpr char separator=';';
#else
        constexpr char separator=':';
#endif
        if(!path.empty())path+=separator;path+=Text(p);
    }
}
std::map<std::string,std::string> Environment(const Options& o) {
    std::map<std::string,std::string> env;std::string path=o.toolPath;
    if(o.profile==Profile::EspIdf) {
        if(!o.idfRoot.empty())env["IDF_PATH"]=Text(o.idfRoot);
        if(!o.idfTools.empty())env["IDF_TOOLS_PATH"]=Text(o.idfTools);
        env["IDF_PYTHON_CHECK_CONSTRAINTS"]=o.checkIdfConstraints?"1":"0";
        // idf.py extension modules also expect the version that export normally
        // provides. Read it as data; never execute a user setup script in a shell.
        std::ifstream version(o.idfRoot/"tools/cmake/version.cmake");std::string line;int major=-1,minor=-1,patch=-1;
        while(std::getline(version,line)){int number=0;if(std::sscanf(line.c_str(),"set(IDF_VERSION_MAJOR %d)",&number)==1)major=number;else if(std::sscanf(line.c_str(),"set(IDF_VERSION_MINOR %d)",&number)==1)minor=number;else if(std::sscanf(line.c_str(),"set(IDF_VERSION_PATCH %d)",&number)==1)patch=number;}
        if(major>=0&&minor>=0&&patch>=0)env["ESP_IDF_VERSION"]=std::to_string(major)+"."+std::to_string(minor)+"."+std::to_string(patch);
        std::error_code ec;std::vector<std::filesystem::path> roms;for(const auto& entry:std::filesystem::directory_iterator(o.idfTools/"tools/esp-rom-elfs",ec))if(entry.is_directory(ec))roms.push_back(entry.path());std::sort(roms.rbegin(),roms.rend());if(!roms.empty())env["ESP_ROM_ELF_DIR"]=Text(roms.front());
        if(!o.python.empty()) {const auto scripts=o.python.parent_path();AddPath(path,scripts);env["IDF_PYTHON_ENV_PATH"]=Text(scripts.parent_path());}
    }
    if(!path.empty()) {
#ifdef _WIN32
        path+=';';
#else
        path+=':';
#endif
        env["PATH"]=path+Env("PATH");
    }
    return env;
}
Command Tool(const Options& o) {
    Command c;c.directory=o.project;c.environment=Environment(o);
    if(o.profile==Profile::EspIdf){Require(!o.python.empty(),"Select the ESP-IDF Python executable");Require(Exists(o.idfRoot/"tools/idf.py"),"Select an ESP-IDF root containing tools/idf.py");c.executable=o.python;c.arguments={Text(o.idfRoot/"tools/idf.py")};}
    else {Require(!o.executable.empty(),"Select or detect the tool executable");c.executable=o.executable;}
    return c;
}
#ifdef _WIN32
std::wstring Wide(const std::string& text) {
    if(text.empty())return {};const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8 in process arguments");std::wstring result(n,L'\0');MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),n);return result;
}
std::wstring Quote(const std::wstring& arg) {
    // CommandLineToArgvW/MSVC CRT inverse: preserve trailing slashes and quotes.
    std::wstring value=L"\"";size_t slashes=0;
    for(auto c:arg){if(c==L'\\'){++slashes;continue;}if(c==L'"'){value.append(slashes*2+1,L'\\');value+=c;}else{value.append(slashes,L'\\');value+=c;}slashes=0;}
    value.append(slashes*2,L'\\');value+=L'"';return value;
}
struct Handle {HANDLE value=nullptr;Handle()=default;explicit Handle(HANDLE h):value(h){}~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;};
struct Insensitive {bool operator()(const std::wstring& a,const std::wstring& b)const{return _wcsicmp(a.c_str(),b.c_str())<0;}};
std::vector<wchar_t> EnvironmentBlock(const Command& command) {
    std::map<std::wstring,std::wstring,Insensitive> values;auto* raw=GetEnvironmentStringsW();if(!raw)throw std::runtime_error("Cannot read process environment");
    for(auto* item=raw;*item;item+=wcslen(item)+1){std::wstring entry=item;const auto split=entry.find(L'=',entry.front()==L'='?1:0);if(split!=std::wstring::npos)values[entry.substr(0,split)]=entry.substr(split+1);}FreeEnvironmentStringsW(raw);
    for(const auto& e:command.environment)values[Wide(e.first)]=Wide(e.second);
    std::vector<wchar_t> result;for(const auto& e:values){result.insert(result.end(),e.first.begin(),e.first.end());result.push_back(L'=');result.insert(result.end(),e.second.begin(),e.second.end());result.push_back(L'\0');}result.push_back(L'\0');return result;
}
Result One(const Command& c,std::atomic_bool& cancel,const Output& output) {
    Result result;SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};HANDLE readRaw=nullptr,writeRaw=nullptr;
    if(!CreatePipe(&readRaw,&writeRaw,&security,0))throw std::runtime_error("Cannot create tool output pipe");Handle read(readRaw),write(writeRaw);SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0);
    Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr));
    Handle job(CreateJobObjectW(nullptr,nullptr));if(!job.value)throw std::runtime_error("Cannot create cancellable process job");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof limits))throw std::runtime_error("Cannot configure process job");
    std::wstring command=Quote(c.executable.wstring());for(const auto& a:c.arguments)command+=L" "+Quote(Wide(a));auto env=EnvironmentBlock(c);
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof startup;startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdOutput=write.value;startup.StartupInfo.hStdError=write.value;startup.StartupInfo.hStdInput=input.value;
    // Limit inherited handles to the pipe and NUL; serial ports and project/session
    // locks in the editor cannot accidentally leak into a compiler process.
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<uint8_t> attributes(bytes);startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes))throw std::runtime_error("Cannot initialize process handle list");
    struct AttributeCleanup {LPPROC_THREAD_ATTRIBUTE_LIST p;~AttributeCleanup(){DeleteProcThreadAttributeList(p);}} cleanup{startup.lpAttributeList};
    HANDLE inherited[]={write.value,input.value};if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr))throw std::runtime_error("Cannot constrain inherited process handles");
    PROCESS_INFORMATION process{};const auto directory=c.directory.wstring();
    if(!CreateProcessW(c.executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT,env.data(),directory.empty()?nullptr:directory.c_str(),&startup.StartupInfo,&process))throw std::runtime_error("Cannot start tool (Windows error "+std::to_string(GetLastError())+"): "+Text(c.executable));
    Handle child(process.hProcess),thread(process.hThread);
    if(!AssignProcessToJobObject(job.value,child.value)){TerminateProcess(child.value,1);WaitForSingleObject(child.value,5000);throw std::runtime_error("Cannot attach tool to cancellation job");}
    ResumeThread(thread.value);CloseHandle(write.value);write.value=nullptr;
    char buffer[4096];bool stopped=false;auto finishedAt=std::chrono::steady_clock::time_point{};
    for(;;) {
        if(cancel.load(std::memory_order_relaxed)&&!stopped){TerminateJobObject(job.value,130);stopped=true;result.cancelled=true;}
        DWORD available=0;if(PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr)&&available){DWORD got=0;if(ReadFile(read.value,buffer,(std::min)(DWORD(sizeof buffer),available),&got,nullptr)&&got)if(output)output(std::string(buffer,got));}
        const bool exited=WaitForSingleObject(child.value,0)==WAIT_OBJECT_0;
        if(exited&&available==0)break;
        if(exited){if(finishedAt==std::chrono::steady_clock::time_point{})finishedAt=std::chrono::steady_clock::now();if(std::chrono::steady_clock::now()-finishedAt>std::chrono::seconds(2))break;}
        if(!available)std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    DWORD code=0;GetExitCodeProcess(child.value,&code);result.exitCode=int(code);return result;
}
#else
Result One(const Command& c,std::atomic_bool& cancel,const Output& output) {
    int pipes[2];if(pipe(pipes))throw std::runtime_error("Cannot create tool output pipe");
    auto child=fork();if(child<0){close(pipes[0]);close(pipes[1]);throw std::runtime_error("Cannot fork tool");}
    if(child==0){setpgid(0,0);close(pipes[0]);dup2(pipes[1],STDOUT_FILENO);dup2(pipes[1],STDERR_FILENO);close(pipes[1]);int input=open("/dev/null",O_RDONLY);dup2(input,STDIN_FILENO);close(input);
        if(!c.directory.empty()&&chdir(c.directory.c_str()))_exit(126);for(const auto& e:c.environment)setenv(e.first.c_str(),e.second.c_str(),1);
        std::vector<std::string> args{Text(c.executable)};args.insert(args.end(),c.arguments.begin(),c.arguments.end());std::vector<char*> argv;for(auto& a:args)argv.push_back(a.data());argv.push_back(nullptr);execvp(argv[0],argv.data());_exit(127);}
    setpgid(child,child);close(pipes[1]);fcntl(pipes[0],F_SETFL,O_NONBLOCK);Result result;bool stopped=false;int status=0;char buffer[4096];bool exited=false;
    auto finished=std::chrono::steady_clock::time_point{};
    for(;;){if(cancel.load(std::memory_order_relaxed)&&!stopped){kill(-child,SIGKILL);result.cancelled=stopped=true;}const auto got=read(pipes[0],buffer,sizeof buffer);if(got>0&&output)output(std::string(buffer,size_t(got)));if(!exited&&waitpid(child,&status,WNOHANG)==child){exited=true;finished=std::chrono::steady_clock::now();}if(exited&&(got<=0||std::chrono::steady_clock::now()-finished>std::chrono::seconds(2)))break;if(got<=0){pollfd event{pipes[0],POLLIN,0};poll(&event,1,20);}}
    close(pipes[0]);kill(-child,SIGKILL);if(!exited)waitpid(child,&status,0);result.exitCode=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);return result;
}
#endif
}
std::vector<Command> BuildPlan(const Options& o,bool flash) {
    std::error_code error;Require(std::filesystem::is_directory(o.project,error),"Choose an existing generated project folder");
    Require(o.jobs>=1&&o.jobs<=256,"Choose between 1 and 256 build workers");
    const auto build=o.build.empty()?o.project/"build":o.build;
    if(o.profile==Profile::CMake&&!o.picoBoard.empty()) {
        Require(o.picoBoard=="pico"||o.picoBoard=="pico2","Unsupported Pico board; choose Pico 1 or Pico 2");
        Require(!flash,"Copy the built UF2 to the board's BOOTSEL drive to flash this Pico project");
        Require(!o.linuxHostWsl,"Use a native Pico toolchain on this host, or build the generated project directly inside Linux");
        auto configure=Tool(o);
        configure.arguments={"-S",Text(o.project),"-B",Text(build),"-G","Ninja","-DCMAKE_BUILD_TYPE=Release",
            "-DPICO_BOARD="+o.picoBoard,"-DPICO_PLATFORM="+std::string(o.picoBoard=="pico"?"rp2040":"rp2350-arm-s")};
        if(!o.picoSdk.empty()) {
            Require(Exists(o.picoSdk/"pico_sdk_init.cmake"),"Choose a Pico SDK folder containing pico_sdk_init.cmake");
            configure.arguments.push_back("-DPICO_SDK_PATH="+Text(o.picoSdk));
            const auto tools=o.picoSdk.parent_path()/"tools";
            for(const auto* name:{"picotool","pioasm"})if(Exists(tools/name/(std::string(name)+"Config.cmake")))
                configure.arguments.push_back(std::string("-D")+name+"_DIR="+Text(tools/name));
        }
        auto compile=configure;compile.arguments={"--build",Text(build),"--parallel",std::to_string(o.jobs)};
        return {configure,compile};
    }
    if(o.profile==Profile::CMake&&o.linuxHostWsl) {
        Require(!flash,"CMake projects have no universal flash action");
        Command configure;configure.executable=Find("wsl");Require(!configure.executable.empty(),"WSL is unavailable. Install a Linux toolchain or build this project on Linux.");configure.directory=o.project;
        configure.arguments={"--exec","cmake","-S",WslPath(o.project),"-B",WslPath(build),"-DCMAKE_BUILD_TYPE=Release"};
        auto compile=configure;compile.arguments={"--exec","cmake","--build",WslPath(build),"--parallel",std::to_string(o.jobs)};return {configure,compile};
    }
    auto c=Tool(o);
    if(o.profile==Profile::Arduino) {
        auto sketch=o.project;auto library=o.library;
        if(library.empty()){auto candidate=o.project;for(int i=0;i<4;++i){if(Exists(candidate/"library.properties")){library=candidate;break;}if(candidate==candidate.parent_path())break;candidate=candidate.parent_path();}}
        if(Exists(sketch/"library.properties")&&Exists(sketch/"examples/ProtobufDisplay/ProtobufDisplay.ino"))sketch/="examples/ProtobufDisplay";
        Require(!o.fqbn.empty(),"Enter a board FQBN");c.arguments={flash?"upload":"compile","--fqbn",o.fqbn};
        if(!o.config.empty()){c.arguments.push_back("--config-file");c.arguments.push_back(Text(o.config));}
        if(flash){Require(!o.port.empty(),"Choose a serial port before flashing");c.arguments.insert(c.arguments.end(),{"--port",o.port,"--input-dir",Text(build)});}
        else {c.arguments.insert(c.arguments.end(),{"--build-path",Text(build),"--jobs",std::to_string(o.jobs)});if(!library.empty())c.arguments.insert(c.arguments.end(),{"--library",Text(library)});}
        c.arguments.push_back(Text(sketch));return {c};
    }
    if(o.profile==Profile::EspIdf){c.arguments.insert(c.arguments.end(),{"-C",Text(o.project),"-B",Text(build)});if(flash){Require(!o.port.empty(),"Choose a serial port before flashing");c.arguments.insert(c.arguments.end(),{"-p",o.port,"-b",std::to_string(o.baud)});}c.arguments.push_back(flash?"flash":"build");return {c};}
    Require(!flash,"CMake projects have no universal flash action");c.arguments={"-S",Text(o.project),"-B",Text(build)};auto compile=c;compile.arguments={"--build",Text(build),"--parallel",std::to_string(o.jobs)};return {c,compile};
}
std::vector<Command> ProbePlan(const Options& o){if(o.profile==Profile::CMake&&o.linuxHostWsl){Command c;c.executable=Find("wsl");Require(!c.executable.empty(),"WSL is unavailable");c.arguments={"--exec","cmake","--version"};return {c};}auto c=Tool(o);c.arguments.push_back(o.profile==Profile::Arduino?"version":"--version");return {c};}
std::string WslPath(const std::filesystem::path& path){
    const auto value=path.lexically_normal().generic_u8string();
    if(value.size()>2&&std::isalpha(static_cast<unsigned char>(value[0]))&&value[1]==':'&&value[2]=='/')return "/mnt/"+std::string(1,char(std::tolower(static_cast<unsigned char>(value[0]))))+value.substr(2);
    if(!value.empty()&&value[0]=='/')return value;
    throw std::invalid_argument("WSL paths need an absolute local drive path or a Linux path");
}
std::vector<Command> RunPlan(const Options& o){
    Require(!o.runProgram.empty(),"Select a built executable or create a supported runnable project first");
    Command command;command.directory=o.project;
    if(o.linuxHostWsl){command.executable=Find("wsl");Require(!command.executable.empty(),"WSL is unavailable");command.arguments={"--exec",WslPath(o.runProgram)};for(const auto& argument:o.runArguments){const auto p=std::filesystem::u8path(argument);command.arguments.push_back(p.is_absolute()?WslPath(p):argument);}}
    else {Require(Exists(o.runProgram),"Build this project before running its executable");command.executable=o.runProgram;command.arguments=o.runArguments;}
    return {command};
}
std::string Describe(const Command& c){std::ostringstream s;s<<'"'<<Text(c.executable)<<'"';for(const auto& arg:c.arguments){s<<" \"";for(auto ch:arg){if(ch=='"')s<<'\\';if(ch=='\r'||ch=='\n')s<<' ';else s<<ch;}s<<'"';}return s.str();}
Result Run(const std::vector<Command>& commands,std::atomic_bool& cancel,Output output){Result result;try{for(const auto& c:commands){if(cancel.load()){result.cancelled=true;result.exitCode=130;break;}if(output)output("\n> "+Describe(c)+"\n");result=One(c,cancel,output);if(result.exitCode||result.cancelled)break;}}catch(const std::exception& e){result.error=e.what();}return result;}
Options Detect(Options o,const std::filesystem::path& workspace) {
    if(o.executable.empty())o.executable=Find(o.profile==Profile::Arduino?"arduino-cli":"cmake");
    if(o.profile==Profile::Arduino&&o.executable.empty()&&Exists(workspace/"build/arduino-tools/cli/arduino-cli.exe"))o.executable=workspace/"build/arduino-tools/cli/arduino-cli.exe";
    if(o.profile==Profile::Arduino&&o.config.empty()&&Exists(workspace/"build/arduino-tools/arduino-cli.yaml"))o.config=workspace/"build/arduino-tools/arduino-cli.yaml";
    if(o.profile==Profile::CMake&&!o.picoBoard.empty()&&o.picoSdk.empty()) {
        const auto inherited=std::filesystem::u8path(Env("PICO_SDK_PATH"));
        if(Exists(inherited/"pico_sdk_init.cmake"))o.picoSdk=inherited;
        else if(!workspace.empty())for(const auto& relative:{"build/pico-validation/tools/pico-sdk","build/pico-validation/pico-sdk"}) {
            const auto candidate=workspace/relative;
            if(Exists(candidate/"pico_sdk_init.cmake")){o.picoSdk=candidate;break;}
        }
    }
    if(o.profile==Profile::CMake&&!o.picoBoard.empty()&&o.toolPath.empty()&&!o.picoSdk.empty()) {
        const auto compiler=o.picoSdk.parent_path()/"tools/bin";
        if(Exists(compiler/"arm-none-eabi-gcc.exe")||Exists(compiler/"arm-none-eabi-gcc"))AddPath(o.toolPath,compiler);
    }
    if(o.profile==Profile::EspIdf) {
        if(o.idfRoot.empty()){const auto inherited=std::filesystem::u8path(Env("IDF_PATH"));if(Exists(inherited/"tools/idf.py"))o.idfRoot=inherited;}if(o.idfTools.empty())o.idfTools=std::filesystem::u8path(Env("IDF_TOOLS_PATH"));
#ifdef _WIN32
        if(o.idfRoot.empty()){const auto root=std::filesystem::u8path(Env("USERPROFILE"))/"esp";std::error_code ec;std::vector<std::filesystem::path> candidates;for(const auto& entry:std::filesystem::directory_iterator(root,ec))if(Exists(entry.path()/"esp-idf/tools/idf.py"))candidates.push_back(entry.path()/"esp-idf");std::sort(candidates.rbegin(),candidates.rend());if(!candidates.empty())o.idfRoot=candidates.front();}
        if(o.idfTools.empty()&&std::filesystem::is_directory("C:/Espressif"))o.idfTools="C:/Espressif";
#endif
        if(o.python.empty()){auto env=std::filesystem::u8path(Env("IDF_PYTHON_ENV_PATH"));if(!env.empty()){
#ifdef _WIN32
            o.python=env/"Scripts/python.exe";
#else
            o.python=env/"bin/python";
#endif
        }}
#ifdef _WIN32
        if(!Exists(o.python)&&Exists(o.idfTools/"tools/python/v6.1/venv/Scripts/python.exe"))o.python=o.idfTools/"tools/python/v6.1/venv/Scripts/python.exe";
        // This repository's previously validated local build uses this installed
        // v6.0 SDK / v6.1 Python snapshot without a constraints file. Reflect that
        // explicit setup in the visible checkbox rather than silently failing.
        if(o.idfRoot.parent_path().filename()=="v6.0"&&o.python==o.idfTools/"tools/python/v6.1/venv/Scripts/python.exe"&&!Exists(o.idfTools/"espidf.constraints.v6.0.txt"))o.checkIdfConstraints=false;
        if(o.toolPath.empty()) {AddPath(o.toolPath,o.idfTools/"tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin");AddPath(o.toolPath,o.idfTools/"tools/ninja/1.12.1");}
#endif
        if(o.python.empty())o.python=Find("python");
    }
    return o;
}
std::vector<std::string> SerialPorts(){std::vector<std::string> ports;
#ifdef _WIN32
    wchar_t target[2048];for(int i=1;i<=256;++i){const auto port="COM"+std::to_string(i);if(QueryDosDeviceW(Wide(port).c_str(),target,2048))ports.push_back(port);}
#elif defined(__APPLE__)
    std::error_code ec;for(const auto& entry:std::filesystem::directory_iterator("/dev",ec)){const auto n=entry.path().filename().string();if(n.rfind("cu.",0)==0||n.rfind("tty.",0)==0)ports.push_back(entry.path().string());}
    std::sort(ports.begin(),ports.end());
#else
    std::error_code ec;for(const auto& entry:std::filesystem::directory_iterator("/dev",ec)){const auto n=entry.path().filename().string();if(n.rfind("ttyUSB",0)==0||n.rfind("ttyACM",0)==0||n.rfind("ttyS",0)==0)ports.push_back(entry.path().string());}
#endif
    return ports;
}
Result Monitor(const std::string& port,unsigned baud,std::atomic_bool& cancel,Output output){Result result;try{Require(!port.empty(),"Choose a serial port");Require(baud>0&&baud<=4000000,"Choose a baud rate between 1 and 4000000");
#ifdef _WIN32
    const auto device=port.rfind("\\\\.\\",0)==0?port:"\\\\.\\"+port;Handle serial(CreateFileW(Wide(device).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(serial.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open "+port+" (Windows error "+std::to_string(GetLastError())+")");
    DCB config{};config.DCBlength=sizeof config;if(!GetCommState(serial.value,&config))throw std::runtime_error("Cannot read serial settings");config.BaudRate=baud;config.ByteSize=8;config.Parity=NOPARITY;config.StopBits=ONESTOPBIT;config.fBinary=TRUE;config.fParity=FALSE;config.fOutxCtsFlow=FALSE;config.fOutxDsrFlow=FALSE;config.fDtrControl=DTR_CONTROL_DISABLE;config.fDsrSensitivity=FALSE;config.fOutX=FALSE;config.fInX=FALSE;config.fRtsControl=RTS_CONTROL_DISABLE;if(!SetCommState(serial.value,&config))throw std::runtime_error("Cannot configure serial port");
    COMMTIMEOUTS timeouts{};timeouts.ReadIntervalTimeout=MAXDWORD;timeouts.ReadTotalTimeoutConstant=50;if(!SetCommTimeouts(serial.value,&timeouts))throw std::runtime_error("Cannot configure serial timeouts");char buffer[4096];while(!cancel.load()){DWORD got=0;if(!ReadFile(serial.value,buffer,sizeof buffer,&got,nullptr))throw std::runtime_error("Serial connection ended");if(got&&output)output(std::string(buffer,got));}
#else
    int serial=open(port.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK);if(serial<0)throw std::runtime_error("Cannot open "+port+": "+std::strerror(errno));struct CloseFd{int fd;~CloseFd(){close(fd);}} closeSerial{serial};termios config{};if(tcgetattr(serial,&config))throw std::runtime_error("Cannot read serial settings");cfmakeraw(&config);speed_t speed=0;switch(baud){case 9600:speed=B9600;break;case 19200:speed=B19200;break;case 38400:speed=B38400;break;case 57600:speed=B57600;break;case 115200:speed=B115200;break;case 230400:speed=B230400;break;default:throw std::runtime_error("This Linux serial backend supports 9600, 19200, 38400, 57600, 115200 and 230400 baud");}cfsetispeed(&config,speed);cfsetospeed(&config,speed);config.c_cflag|=CLOCAL|CREAD;config.c_cflag&=~CRTSCTS;if(tcsetattr(serial,TCSANOW,&config))throw std::runtime_error("Cannot configure serial port");char buffer[4096];while(!cancel.load()){pollfd event{serial,POLLIN,0};if(poll(&event,1,50)>0){auto got=read(serial,buffer,sizeof buffer);if(got>0&&output)output(std::string(buffer,size_t(got)));else if(got==0||(errno!=EAGAIN&&errno!=EINTR))throw std::runtime_error("Serial connection ended");}}
#endif
    result.cancelled=cancel.load();result.exitCode=0;
    }catch(const std::exception& e){result.error=e.what();}return result;}
}
