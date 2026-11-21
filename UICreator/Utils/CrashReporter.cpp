#include "CrashReporter.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../../3rdParty/Breakpad/src/client/windows/handler/exception_handler.h"
#endif

struct CrashReporter::Impl
{
    std::filesystem::path directory;
#ifdef _WIN32
    std::unique_ptr<google_breakpad::ExceptionHandler> handler;
    std::terminate_handler previousTerminate = nullptr;
    Impl* previous = nullptr;
    inline static Impl* active = nullptr;
#endif
};

#ifdef _WIN32
namespace
{
    std::wstring Environment(const wchar_t* name)
    {
        const auto size = GetEnvironmentVariableW(name, nullptr, 0);
        if (!size) return {};
        std::wstring value(size, L'\0');
        const auto length = GetEnvironmentVariableW(name, value.data(), size);
        if (!length || length >= size) return {};
        value.resize(length);
        return value;
    }

    bool DumpWritten(const wchar_t* path, const wchar_t* id, void*, EXCEPTION_POINTERS*, MDRawAssertionInfo*, bool succeeded)
    {
        wchar_t message[2048];
        if (succeeded) _snwprintf_s(message, _countof(message), _TRUNCATE, L"LVGL UI Creator crash dump: %s\\%s.dmp\n", path, id);
        else wcscpy_s(message, L"LVGL UI Creator could not write its crash dump\n");
        OutputDebugStringW(message);
        // No uploader or crash-generation server is required for local dumps.
        return succeeded;
    }
}
#endif

CrashReporter::CrashReporter() : impl(std::make_unique<Impl>())
{
#ifdef _WIN32
    const auto overrideDirectory = Environment(L"UICREATOR_DUMP_DIR");
    const auto local = Environment(L"LOCALAPPDATA");
    std::error_code error;
    if (!overrideDirectory.empty()) impl->directory = overrideDirectory;
    else if (!local.empty()) impl->directory = std::filesystem::path(local) / "LVGL_UI_Creator" / "CrashDumps";
    else impl->directory = std::filesystem::temp_directory_path(error) / "LVGL_UI_Creator" / "CrashDumps";
    impl->directory = std::filesystem::absolute(impl->directory, error);
    if (!error) std::filesystem::create_directories(impl->directory, error);
    if (error)
    {
        fprintf(stderr, "Could not create crash dump directory: %s\n", error.message().c_str());
        return;
    }
    impl->handler = std::make_unique<google_breakpad::ExceptionHandler>(impl->directory.wstring(), nullptr,
        DumpWritten, nullptr, google_breakpad::ExceptionHandler::HANDLER_ALL,
        static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules), static_cast<const wchar_t*>(nullptr), nullptr);
    impl->previous = Impl::active;
    Impl::active = impl.get();
    impl->previousTerminate = std::set_terminate([]() {
        if (Impl::active && Impl::active->handler) Impl::active->handler->WriteMinidump();
        std::_Exit(EXIT_FAILURE);
    });
#endif
}

CrashReporter::~CrashReporter()
{
#ifdef _WIN32
    if (impl->previousTerminate)
    {
        std::set_terminate(impl->previousTerminate);
        Impl::active = impl->previous;
    }
#endif
}
bool CrashReporter::IsEnabled() const
{
#ifdef _WIN32
    return impl->handler != nullptr;
#else
    return false;
#endif
}
bool CrashReporter::WriteDump()
{
#ifdef _WIN32
    return impl->handler && impl->handler->WriteMinidump();
#else
    return false;
#endif
}
const std::filesystem::path& CrashReporter::GetDumpDirectory() const { return impl->directory; }
