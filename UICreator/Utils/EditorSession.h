#pragma once

#include "../../3rdParty/JSON/json.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

// Session files are separate from projects. A process lock prevents a second
// editor from overwriting an existing editor's recovery snapshot.
class EditorSession
{
public:
    using Json = nlohmann::json;
    EditorSession() = default;
    ~EditorSession() { Close(); }
    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    static std::filesystem::path DefaultDirectory()
    {
#ifdef _WIN32
        auto environment = [](const wchar_t* name) {
            const auto size = GetEnvironmentVariableW(name, nullptr, 0);
            std::wstring value(size, L'\0');
            if (size) value.resize(GetEnvironmentVariableW(name, value.data(), size));
            return value;
        };
        auto directory = environment(L"UICREATOR_SESSION_DIR");
        if (!directory.empty()) return std::filesystem::path(directory);
        directory = environment(L"LOCALAPPDATA");
        if (!directory.empty()) return std::filesystem::path(directory) / "LVGL_UI_Creator" / "Session";
#else
        if (const auto* directory = std::getenv("UICREATOR_SESSION_DIR")) return directory;
#ifdef __APPLE__
        if (const auto* directory = std::getenv("HOME")) return std::filesystem::path(directory) / "Library/Application Support/LVGL UI Creator/Session";
#endif
        if (const auto* directory = std::getenv("XDG_STATE_HOME")) return std::filesystem::path(directory) / "LVGL_UI_Creator";
        if (const auto* directory = std::getenv("HOME")) return std::filesystem::path(directory) / ".local/state/LVGL_UI_Creator";
#endif
        return std::filesystem::temp_directory_path() / "LVGL_UI_Creator" / "Session";
    }

    void Open(const std::filesystem::path& directory)
    {
        Close();
        root = std::filesystem::absolute(directory);
        std::filesystem::create_directories(root);
#ifdef _WIN32
        lock = CreateFileW((root / "session.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock == INVALID_HANDLE_VALUE) throw std::runtime_error("Session storage is already in use or cannot be opened. Project editing remains available.");
#else
        lock = ::open((root / "session.lock").c_str(), O_CREAT | O_RDWR, 0600);
        if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0) { Close(); throw std::runtime_error("Session storage is already in use or cannot be opened."); }
#endif
    }
    void Close()
    {
#ifdef _WIN32
        if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
        lock = INVALID_HANDLE_VALUE;
#else
        if (lock >= 0) ::close(lock);
        lock = -1;
#endif
    }
    bool Enabled() const
    {
#ifdef _WIN32
        return lock != INVALID_HANDLE_VALUE;
#else
        return lock >= 0;
#endif
    }
    const std::filesystem::path& Directory() const { return root; }
    Json Read(const char* name) const
    {
        std::ifstream stream(root / name, std::ios::binary);
        if (!stream) return Json::object();
        return Json::parse(stream);
    }
    void Write(const char* name, const Json& value) const { WriteAtomic(root / name, value.dump(2)); }
    void RemoveRecovery() const
    {
        std::error_code error;
        std::filesystem::remove(root / "recovery.json", error);
        if (error) throw std::runtime_error("Could not remove the recovery snapshot: " + error.message());
    }
    static void WriteAtomic(const std::filesystem::path& path, const std::string& text)
    {
        if (path.empty() || path.filename().empty()) throw std::runtime_error("Choose a project file name.");
        auto temporary = path;
        temporary += ".uicreator-tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        try
        {
            std::ofstream stream(temporary, std::ios::binary);
            if (!stream) throw std::runtime_error("Cannot open file for writing: " + path.u8string());
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream.close();
            if (!stream) throw std::runtime_error("Could not finish writing: " + path.u8string());
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Could not replace file: " + path.u8string());
#else
            std::filesystem::rename(temporary, path);
#endif
        }
        catch (...)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw;
        }
    }
private:
    std::filesystem::path root;
#ifdef _WIN32
    HANDLE lock = INVALID_HANDLE_VALUE;
#else
    int lock = -1;
#endif
};
