#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace NativeFileDialog {
enum class Mode { Open, Save, Folder };
enum class Status { Selected, Cancelled, Unavailable, Failed };
struct Filter {std::string name;std::vector<std::string> patterns;};
struct Options {
    Mode mode=Mode::Open;
    std::string title;
    std::filesystem::path initial;
    std::vector<Filter> filters;
    bool multiple=false;
};
struct Result {
    Status status=Status::Cancelled;
    std::vector<std::filesystem::path> paths;
    std::string error;
};
// Native macOS panels or the Linux desktop backend (argv, never shell text).
Result Show(const Options&);
// Existing desktop dialogs use NUL-separated Windows label/pattern pairs.
Options FromWindows(bool save,bool folder,const wchar_t* title,const wchar_t* filter=nullptr,
                    const std::filesystem::path& initial={},bool multiple=false);
}
