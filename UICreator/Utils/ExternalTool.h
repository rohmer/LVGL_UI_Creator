#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>

namespace ExternalTool {
// Launch the project-creation helper directly, with cancellation and a finite
// operation-specific deadline. Ordinary helper operations retain three minutes.
int Run(const std::filesystem::path& runtime, const std::filesystem::path& script,
        const std::filesystem::path& request, const std::filesystem::path& log,
        const std::atomic<bool>* cancel = nullptr,
        std::chrono::milliseconds timeout = std::chrono::minutes(3));
}
