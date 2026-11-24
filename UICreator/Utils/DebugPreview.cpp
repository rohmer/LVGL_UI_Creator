#include "DebugPreview.h"
#include "ApplicationPaths.h"
#include "EditorSession.h"
#include "../../CodeGeneration/ProjectAssets.h"
#include <Serialization/Protobuf.h>
#include <Serialization/ProjectDesign.h>
#include <Serialization/ProjectScreens.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
struct DebugPreview::Impl {
    std::filesystem::path snapshot;
#ifdef _WIN32
    HANDLE process = nullptr;
    DWORD pid = 0;
#else
    mutable pid_t pid = -1;
#endif
};
DebugPreview::DebugPreview():state(std::make_unique<Impl>()) {}
DebugPreview::~DebugPreview() {
    Stop();
    if(!state->snapshot.empty()) {
        std::error_code ignored;
        std::filesystem::remove(state->snapshot,ignored);
        std::filesystem::remove(state->snapshot.parent_path(),ignored);
    }
}
bool DebugPreview::Running() const {
#ifdef _WIN32
    DWORD exitCode=0;
    return state->process && GetExitCodeProcess(state->process,&exitCode) && exitCode==STILL_ACTIVE;
#else
    if(state->pid<=0)return false;
    int status=0;pid_t result;
    do {result=waitpid(state->pid,&status,WNOHANG);}while(result<0&&errno==EINTR);
    if(result==0)return true;
    if(result==state->pid||(result<0&&errno==ECHILD))state->pid=-1;
    return false;
#endif
}
std::string DebugPreview::SnapshotPath() const {return state->snapshot.u8string();}
void DebugPreview::Stop() {
#ifdef _WIN32
    if(state->process) {
        if(Running()) {
            auto stop=state->snapshot;stop+=".stop";
            try { EditorSession::WriteAtomic(stop,"stop"); } catch(...) {}
            EnumWindows([](HWND window,LPARAM data)->BOOL {
            DWORD owner=0;GetWindowThreadProcessId(window,&owner);
            if(owner==static_cast<DWORD>(data))PostMessageW(window,WM_CLOSE,0,0);
            return TRUE;
            },static_cast<LPARAM>(state->pid));
            if(WaitForSingleObject(state->process,1500)==WAIT_TIMEOUT)TerminateProcess(state->process,1);
            std::error_code ignored;std::filesystem::remove(stop,ignored);
        }
        CloseHandle(state->process);state->process=nullptr;state->pid=0;
    }
#else
    if(Running()) {
        auto stop=state->snapshot;stop+=".stop";
        try {EditorSession::WriteAtomic(stop,"stop");}catch(...){}
        const auto deadline=SDL_GetTicks()+1500;
        while(Running()&&SDL_GetTicks()<deadline)SDL_Delay(10);
        if(Running()) {
            kill(state->pid,SIGTERM);
            const auto terminateDeadline=SDL_GetTicks()+200;
            while(Running()&&SDL_GetTicks()<terminateDeadline)SDL_Delay(10);
        }
        if(Running()) {
            kill(state->pid,SIGKILL);int status=0;
            while(waitpid(state->pid,&status,0)<0&&errno==EINTR){}
        }
        std::error_code ignored;std::filesystem::remove(stop,ignored);
    }
    state->pid=-1;
#endif
}
void DebugPreview::Start(nlohmann::json document,const std::string& sourceDirectory) {
#ifdef _WIN32
    const char* viewer="UICreatorViewer.exe";
#else
    const char* viewer="UICreatorViewer";
#endif
    const auto executable=ApplicationPaths::ExecutableDirectory()/viewer;
    if(!std::filesystem::is_regular_file(executable))throw std::runtime_error(std::string(viewer)+" is missing. Keep the display-library viewer beside the editor.");
    if(document.contains("screens"))document["activeScreen"]=document.value("editorScreen",document.value("activeScreen",size_t(0)));
    document=Serialization::ProjectScreens::Device(document);
    Serialization::ProjectDesign::Materialize(document,true);
    CGeneration::EmbedLocalImages(document,std::filesystem::u8path(sourceDirectory));
    document["runtimeTarget"]="9.6";
    const auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);
    if(state->snapshot.empty()) {
#ifdef _WIN32
        const auto processId=GetCurrentProcessId();
#else
        const auto processId=getpid();
#endif
        const auto directory="uicreator-debug-"+std::to_string(processId)+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        state->snapshot=std::filesystem::temp_directory_path()/directory/"screen.pb";
        std::filesystem::create_directories(state->snapshot.parent_path());
    }
    EditorSession::WriteAtomic(state->snapshot,std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    if(Running())return; // --watch reloads the atomically replaced snapshot.
    Stop();
    auto stop=state->snapshot;stop+=".stop";
    std::error_code ignored;std::filesystem::remove(stop,ignored);
#ifdef _WIN32
    auto command=L"\""+executable.wstring()+L"\" \""+state->snapshot.wstring()+L"\" --watch --stop-file \""+stop.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,executable.parent_path().c_str(),&startup,&child))throw std::runtime_error("Cannot launch Protobuf debug viewer (Windows error "+std::to_string(GetLastError())+")");
    CloseHandle(child.hThread);state->process=child.hProcess;state->pid=child.dwProcessId;
#else
    const auto executableText=executable.string(),snapshotText=state->snapshot.string(),stopText=stop.string();
    char* arguments[]={const_cast<char*>(executableText.c_str()),const_cast<char*>(snapshotText.c_str()),const_cast<char*>("--watch"),const_cast<char*>("--stop-file"),const_cast<char*>(stopText.c_str()),nullptr};
    pid_t pid=-1;
    const auto result=posix_spawn(&pid,executableText.c_str(),nullptr,nullptr,arguments,environ);
    if(result!=0)throw std::runtime_error("Cannot launch Protobuf debug viewer (POSIX error "+std::to_string(result)+")");
    state->pid=pid;
#endif
}
