#include "ExternalTool.h"
#include <chrono>
#include <cerrno>
#include <stdexcept>
#include <string>
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

namespace ExternalTool {
#ifdef _WIN32
static std::wstring Quote(const std::wstring& text) {
    std::wstring result=L"\"";size_t slashes=0;
    for(auto c:text) {if(c==L'\\'){++slashes;continue;}if(c==L'"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}
    result.append(slashes*2,L'\\');return result+L'"';
}
#endif
int Run(const std::filesystem::path& runtime,const std::filesystem::path& script,const std::filesystem::path& request,const std::filesystem::path& log,const std::atomic<bool>* cancel,std::chrono::milliseconds timeout) {
    const auto now=std::chrono::steady_clock::now();
    if(timeout<=std::chrono::milliseconds::zero() || timeout>std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::time_point::max()-now))
        throw std::invalid_argument("Project tool timeout must be positive and finite");
    const auto deadline=now+timeout;
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
    if(!started)throw std::runtime_error("Cannot start the bundled project tool (Windows error "+std::to_string(startError)+")");
    // The converter should also stop if the editor itself exits during a job.
    HANDLE processJob=CreateJobObjectW(nullptr,nullptr);
    if(processJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(processJob,JobObjectExtendedLimitInformation,&limits,sizeof limits)||!AssignProcessToJobObject(processJob,process.hProcess)) {CloseHandle(processJob);processJob=nullptr;}
    }
    CloseHandle(process.hThread);bool stopped=false;
    while(WaitForSingleObject(process.hProcess,100)==WAIT_TIMEOUT) if((cancel&&cancel->load())||std::chrono::steady_clock::now()>deadline) {TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);stopped=true;break;}
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);if(processJob)CloseHandle(processJob);
    if(stopped)throw std::runtime_error(cancel&&cancel->load()?"Conversion cancelled":"Project tool timed out");return static_cast<int>(code);
#else
    auto child=fork();if(child<0)throw std::runtime_error("Cannot start converter");
    if(!child) {int fd=open(log.c_str(),O_CREAT|O_WRONLY|O_TRUNC,0600);if(fd<0)_exit(127);dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);close(fd);execl(runtime.c_str(),runtime.c_str(),script.c_str(),request.c_str(),nullptr);_exit(127);}
    int status=0;
    for (;;) {
        const auto waited=waitpid(child,&status,WNOHANG);
        if(waited==child)break;
        if(waited<0){if(errno==EINTR)continue;throw std::runtime_error("Cannot wait for project tool");}
        if((cancel&&cancel->load())||std::chrono::steady_clock::now()>deadline){
            kill(child,SIGKILL);
            while(waitpid(child,&status,0)<0&&errno==EINTR){}
            throw std::runtime_error(cancel&&cancel->load()?"Project creation cancelled":"Project tool timed out");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return WIFEXITED(status)?WEXITSTATUS(status):1;
#endif
}
}
