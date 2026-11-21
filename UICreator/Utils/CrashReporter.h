#pragma once
#include <filesystem>
#include <memory>

class CrashReporter
{
public:
    CrashReporter();
    ~CrashReporter();
    CrashReporter(const CrashReporter&) = delete;
    CrashReporter& operator=(const CrashReporter&) = delete;
    bool IsEnabled() const;
    bool WriteDump();
    const std::filesystem::path& GetDumpDirectory() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
