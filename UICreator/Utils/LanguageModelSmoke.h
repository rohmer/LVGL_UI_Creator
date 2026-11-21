#pragma once
#include <filesystem>

// Runs the installed generator and keyboard predictor without opening a window.
// The destination must be new or empty; successful runs write report.json.
int RunLanguageModelSmoke(const std::filesystem::path& destination);
