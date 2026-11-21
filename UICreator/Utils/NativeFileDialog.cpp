#include "NativeFileDialog.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace NativeFileDialog {
namespace {
std::string Utf8(const wchar_t* source) {
    std::string output;if(!source)return output;
    while(*source){uint32_t value=uint32_t(*source++);
        if(sizeof(wchar_t)==2&&value>=0xd800&&value<=0xdbff){const auto next=uint32_t(*source++);if(next<0xdc00||next>0xdfff)throw std::invalid_argument("Invalid dialog text");value=0x10000+((value-0xd800)<<10)+(next-0xdc00);}
        if(value>=0xd800&&value<=0xdfff)throw std::invalid_argument("Invalid dialog text");
        if(value<=0x7f)output.push_back(char(value));
        else if(value<=0x7ff){output.push_back(char(0xc0|(value>>6)));output.push_back(char(0x80|(value&63)));}
        else if(value<=0xffff){output.push_back(char(0xe0|(value>>12)));output.push_back(char(0x80|((value>>6)&63)));output.push_back(char(0x80|(value&63)));}
        else if(value<=0x10ffff){output.push_back(char(0xf0|(value>>18)));output.push_back(char(0x80|((value>>12)&63)));output.push_back(char(0x80|((value>>6)&63)));output.push_back(char(0x80|(value&63)));}
        else throw std::invalid_argument("Invalid dialog text");
    }return output;
}
Result Failure(std::string error,Status status=Status::Failed){return {status,{},std::move(error)};}
#ifndef _WIN32
struct Pipe {int read=-1,write=-1;~Pipe(){if(read>=0)close(read);if(write>=0)close(write);}bool Open(){int descriptors[2];if(pipe(descriptors)!=0)return false;read=descriptors[0];write=descriptors[1];fcntl(read,F_SETFD,FD_CLOEXEC);fcntl(write,F_SETFD,FD_CLOEXEC);return true;}void CloseWrite(){close(write);write=-1;}};
struct Execution {int exit=-1,spawnError=0;bool truncated=false;std::string output,error;};
std::string ExecutablePath(const char* name) {
    const auto* configured=std::getenv("PATH");const std::string search=configured?configured:"/bin:/usr/bin";
    size_t start=0;
    do{const auto end=search.find(':',start);auto directory=search.substr(start,end==std::string::npos?end:end-start);if(directory.empty())directory=".";
        const auto candidate=std::filesystem::u8path(directory)/name;std::error_code error;
        if(std::filesystem::is_regular_file(candidate,error)&&access(candidate.c_str(),X_OK)==0)return std::filesystem::absolute(candidate).u8string();
        if(end==std::string::npos)break;
        start=end+1;
    }while(start<=search.size());
    return {};
}
Execution Execute(std::vector<std::string> arguments) {
    Execution out;Pipe standard,error;
    if(!standard.Open()||!error.Open()){out.spawnError=errno;return out;}
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,standard.write,STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions,error.write,STDERR_FILENO);
    posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    std::vector<char*> argv;for(auto& argument:arguments)argv.push_back(argument.data());argv.push_back(nullptr);
    // System GTK dialogs must use host libraries, not the editor's private
    // FreeType/PNG/X11 dependency directory. The editor environment is intact.
    std::vector<char*> environment;for(auto** item=environ;item&&*item;++item)if(std::strncmp(*item,"LD_LIBRARY_PATH=",16)!=0)environment.push_back(*item);environment.push_back(nullptr);
    pid_t child=-1;out.spawnError=posix_spawnp(&child,argv[0],&actions,nullptr,argv.data(),environment.data());posix_spawn_file_actions_destroy(&actions);
    if(out.spawnError)return out;
    standard.CloseWrite();error.CloseWrite();
    pollfd descriptors[2]={{standard.read,POLLIN,0},{error.read,POLLIN,0}};
    int remaining=2;
    while(remaining){
        int ready;do{ready=poll(descriptors,2,-1);}while(ready<0&&errno==EINTR);
        if(ready<0)break;
        for(int index=0;index<2;++index){auto& descriptor=descriptors[index];if(descriptor.fd<0||!(descriptor.revents&(POLLIN|POLLHUP|POLLERR)))continue;
            char buffer[4096];ssize_t count;do{count=read(descriptor.fd,buffer,sizeof(buffer));}while(count<0&&errno==EINTR);
            if(count<=0){descriptor.fd=-1;--remaining;continue;}
            auto& text=index?out.error:out.output;const size_t limit=index?65536:1048576;
            const auto retained=(std::min)(size_t(count),limit-text.size());text.append(buffer,retained);if(!index&&retained<size_t(count))out.truncated=true;
        }
    }
    int status=0;pid_t waited;do{waited=waitpid(child,&status,0);}while(waited<0&&errno==EINTR);
    if(waited==child&&WIFEXITED(status))out.exit=WEXITSTATUS(status);
    return out;
}
#endif
}
Options FromWindows(bool save,bool folder,const wchar_t* title,const wchar_t* filter,const std::filesystem::path& initial,bool multiple) {
    Options options;options.mode=folder?Mode::Folder:save?Mode::Save:Mode::Open;options.title=Utf8(title);options.initial=initial;options.multiple=multiple&&!save&&!folder;
    for(auto* item=filter;item&&*item;){Filter next;next.name=Utf8(item);item+=std::wcslen(item)+1;if(!*item)break;const auto patterns=Utf8(item);item+=std::wcslen(item)+1;size_t start=0;
        while(start<patterns.size()){auto end=patterns.find(';',start);auto pattern=patterns.substr(start,end==std::string::npos?end:end-start);if(pattern=="*.*")pattern="*";if(!pattern.empty())next.patterns.push_back(std::move(pattern));if(end==std::string::npos)break;start=end+1;}
        options.filters.push_back(std::move(next));
    }return options;
}
Result Show(const Options& options) {
#ifdef _WIN32
    (void)options;return Failure("Use the existing native Windows file dialog.",Status::Unavailable);
#else
    // Some POSIX spawn implementations/emulators report an absent executable
    // through child exit127 instead of ENOENT. Resolve a real executable first;
    // still handle a removal race at spawn time below.
    const auto executable=ExecutablePath("zenity");
    if(executable.empty())return Failure("File selection requires Zenity. Install the zenity package and try again.",Status::Unavailable);
    std::vector<std::string> arguments={executable,"--file-selection","--title="+options.title};
    if(options.mode==Mode::Folder)arguments.push_back("--directory");
    else if(options.mode==Mode::Save){arguments.push_back("--save");arguments.push_back("--confirm-overwrite");}
    const bool multiple=options.multiple&&options.mode==Mode::Open;
    if(multiple){arguments.push_back("--multiple");arguments.emplace_back("--separator=\x1f");}
    if(!options.initial.empty())arguments.push_back("--filename="+options.initial.u8string());
    if(options.mode!=Mode::Folder)for(const auto& filter:options.filters){std::string value="--file-filter="+filter.name+" |";for(const auto& pattern:filter.patterns)value+=' '+pattern;arguments.push_back(std::move(value));}
    for(const auto& argument:arguments)if(argument.find('\0')!=std::string::npos)return Failure("File selection options contain an invalid NUL character.");
    auto execution=Execute(std::move(arguments));
    if(execution.spawnError==ENOENT)return Failure("File selection requires Zenity. Install the zenity package and try again.",Status::Unavailable);
    if(execution.spawnError)return Failure("Cannot start file selection: "+std::string(std::strerror(execution.spawnError)));
    if(execution.exit==1&&execution.output.empty()){
        if(execution.error.find("cannot open display")==std::string::npos&&execution.error.find("cannot connect")==std::string::npos)return {Status::Cancelled,{},{}};
    }
    if(execution.exit!=0)return Failure("File selection failed"+(execution.error.empty()?std::string(" (exit ")+std::to_string(execution.exit)+")":": "+execution.error.substr(0,1024)));
    if(execution.truncated)return Failure("Too many selected file paths; choose fewer files.");
    auto text=std::move(execution.output);if(!text.empty()&&text.back()=='\n')text.pop_back();if(!text.empty()&&text.back()=='\r')text.pop_back();
    if(text.empty())return Failure("File selection returned no path.");
    if(text.find('\0')!=std::string::npos)return Failure("File selection returned an invalid path.");
    Result result;result.status=Status::Selected;
    size_t start=0;do{const auto end=multiple?text.find('\x1f',start):std::string::npos;const auto path=text.substr(start,end==std::string::npos?end:end-start);if(path.empty())return Failure("File selection returned an empty path.");result.paths.push_back(std::filesystem::u8path(path));if(end==std::string::npos)break;start=end+1;}while(start<=text.size());
    return result;
#endif
}
}
