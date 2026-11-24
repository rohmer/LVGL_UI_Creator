#pragma once
#include <atomic>
#include <filesystem>

namespace ExternalTool {
// Launch the project-creation helper directly, with cooperative cancellation.
int Run(const std::filesystem::path& runtime, const std::filesystem::path& script,
        const std::filesystem::path& request, const std::filesystem::path& log,
        const std::atomic<bool>* cancel = nullptr);
}
