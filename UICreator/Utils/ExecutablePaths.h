#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace ApplicationPaths {
inline std::filesystem::path ExecutableDirectory()
{
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) throw std::runtime_error("Cannot locate the application folder");
    path.resize(length);
    return std::filesystem::path(path).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 1024;
    std::vector<char> path(size);
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        if (!size || size > 1024 * 1024) throw std::runtime_error("Invalid application path length");
        path.resize(size);
        if (_NSGetExecutablePath(path.data(), &size) != 0) throw std::runtime_error("Cannot locate the application folder");
    }
    return std::filesystem::weakly_canonical(path.data()).parent_path();
#else
    std::vector<char> path(4096);
    for (;;) {
        const auto length = readlink("/proc/self/exe", path.data(), path.size());
        if (length < 0) throw std::runtime_error("Cannot locate the application folder");
        if (static_cast<size_t>(length) < path.size())
            return std::filesystem::path(std::string(path.data(), static_cast<size_t>(length))).parent_path();
        if (path.size() >= 1024 * 1024) throw std::runtime_error("Invalid application path length");
        path.resize(path.size() * 2);
    }
#endif
}

}
