#include "../../../UICreator/Utils/NativeFileDialog.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <sys/stat.h>
namespace fs=std::filesystem;
using namespace NativeFileDialog;
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Environment {
    std::string name,previous;bool existed;
    Environment(const char* key,const std::string& value):name(key),existed(std::getenv(key)!=nullptr){if(existed)previous=std::getenv(key);setenv(key,value.c_str(),1);}
    ~Environment(){if(existed)setenv(name.c_str(),previous.c_str(),1);else unsetenv(name.c_str());}
};
std::string Read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
std::vector<std::string> Arguments(const fs::path& path){const auto text=Read(path);std::vector<std::string> out;size_t start=0;while(start<text.size()){const auto end=text.find('\0',start);Require(end!=std::string::npos,"Fake backend did not receive complete argv");out.push_back(text.substr(start,end-start));start=end+1;}return out;}
bool Contains(const std::vector<std::string>& args,const std::string& value){for(const auto& argument:args)if(argument==value)return true;return false;}
}
int main(int argc,char** argv){try{
    Require(argc==2,"Pass an isolated writable workspace");const auto root=fs::absolute(fs::u8path(argv[1]));fs::create_directories(root/"fake backend with spaces");
    const auto backend=root/"fake backend with spaces/zenity";const auto log=root/"arguments.bin";const auto environmentLog=root/"environment.txt";
    {std::ofstream script(backend);script<<"#!/bin/sh\nprintf '%s\\0' \"$@\" >\"$NATIVE_DIALOG_LOG\"\nprintf '%s' \"${LD_LIBRARY_PATH-unset}\" >\"$NATIVE_DIALOG_ENV_LOG\"\nif [ -n \"${NATIVE_DIALOG_ERROR-}\" ]; then printf '%s' \"$NATIVE_DIALOG_ERROR\" >&2; fi\nif [ \"${NATIVE_DIALOG_EXIT-0}\" = 0 ]; then printf '%s\\n' \"${NATIVE_DIALOG_SELECTION-}\"; fi\nexit \"${NATIVE_DIALOG_EXIT-0}\"\n";}Require(chmod(backend.c_str(),0700)==0,"Could not make fake backend executable");
    Environment search("PATH",backend.parent_path().string()),arguments("NATIVE_DIALOG_LOG",log.string()),envLog("NATIVE_DIALOG_ENV_LOG",environmentLog.string()),library("LD_LIBRARY_PATH","/private/editor libraries"),error("NATIVE_DIALOG_ERROR",""),exit("NATIVE_DIALOG_EXIT","0");
    const auto chosen=root/fs::u8path(u8"café 模型 $(touch injected); file.ngtr");{std::ofstream out(chosen);out<<"model";}
    Environment selection("NATIVE_DIALOG_SELECTION",chosen.u8string());
    auto options=FromWindows(false,false,L"Select 模型 $(touch injected);",L"Models\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0",chosen);
    auto result=Show(options);Require(result.status==Status::Selected&&result.paths==std::vector<fs::path>{chosen},"Unicode/spaced selection was changed");
    auto received=Arguments(log);Require(Contains(received,"--filename="+chosen.u8string())&&Contains(received,u8"--title=Select 模型 $(touch injected);")&&Contains(received,"--file-filter=Models | *.ngtr *.trie *.bin"),"Dialog arguments were not passed literally");
    Require(!fs::exists(root/"injected"),"A dialog argument executed through a shell");Require(Read(environmentLog)=="unset"&&std::string(std::getenv("LD_LIBRARY_PATH"))=="/private/editor libraries","Private library lookup leaked into the system dialog or changed the editor environment");
    options.mode=Mode::Save;options.multiple=true;result=Show(options);received=Arguments(log);Require(result.status==Status::Selected&&Contains(received,"--save")&&Contains(received,"--confirm-overwrite")&&!Contains(received,"--multiple"),"Save picker options are incorrect");
    options.mode=Mode::Folder;result=Show(options);Require(result.status==Status::Selected&&Contains(Arguments(log),"--directory"),"Folder picker was not requested");
    const auto second=root/"second model.trie";{std::ofstream file(second);file<<"model";}setenv("NATIVE_DIALOG_SELECTION",(chosen.u8string()+std::string(1,'\x1f')+second.u8string()).c_str(),1);
    options.mode=Mode::Open;options.multiple=true;result=Show(options);Require(result.status==Status::Selected&&result.paths==std::vector<fs::path>{chosen,second}&&Contains(Arguments(log),"--multiple"),"Multiple paths were not decoded independently");
    setenv("NATIVE_DIALOG_EXIT","1",1);result=Show(options);Require(result.status==Status::Cancelled&&result.error.empty(),"Normal dialog cancellation was reported as a failure");
    setenv("NATIVE_DIALOG_ERROR","Gtk-WARNING: cannot open display",1);result=Show(options);Require(result.status==Status::Failed&&!result.error.empty(),"Display startup failure was reported as cancellation");
    setenv("NATIVE_DIALOG_EXIT","2",1);setenv("NATIVE_DIALOG_ERROR","backend failed",1);result=Show(options);Require(result.status==Status::Failed&&result.error.find("backend failed")!=std::string::npos,"Backend failure lost its diagnostic");
    setenv("NATIVE_DIALOG_EXIT","127",1);setenv("NATIVE_DIALOG_ERROR","",1);result=Show(options);Require(result.status==Status::Failed&&result.error.find("127")!=std::string::npos,"An existing backend's exit127 was misclassified as missing");
    setenv("NATIVE_DIALOG_EXIT","0",1);setenv("NATIVE_DIALOG_SELECTION","",1);result=Show(options);Require(result.status==Status::Failed,"Empty successful selection was accepted");
    fs::rename(backend,backend.parent_path()/"disabled-zenity");result=Show(options);Require(result.status==Status::Unavailable&&result.error.find("Zenity")!=std::string::npos,"Missing backend was silently ignored");
    std::cout<<"POSIX dialog argv/UTF-8/multiple/save/folder selection, cancel/error distinction, missing backend and child library environment isolation passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
