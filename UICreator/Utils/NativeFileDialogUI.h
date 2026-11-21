#pragma once
#include "NativeFileDialog.h"
#include "../Windows/ProductWindows.h"
#include <exception>
namespace NativeFileDialogUI {
inline std::vector<std::filesystem::path> Pick(bool save,bool folder,const wchar_t* title,const wchar_t* filter=nullptr,const std::filesystem::path& initial={},bool multiple=false){
    try{auto result=NativeFileDialog::Show(NativeFileDialog::FromWindows(save,folder,title,filter,initial,multiple));
        if(result.status==NativeFileDialog::Status::Failed||result.status==NativeFileDialog::Status::Unavailable)ProductWindows::Message("File selection",result.error);
        return result.paths;
    }catch(const std::exception& error){ProductWindows::Message("File selection",error.what());return {};}
}
inline std::filesystem::path PickOne(bool save,bool folder,const wchar_t* title,const wchar_t* filter=nullptr,const std::filesystem::path& initial={}){
    const auto paths=Pick(save,folder,title,filter,initial);return paths.empty()?std::filesystem::path{}:paths.front();
}
}
