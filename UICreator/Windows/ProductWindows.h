#pragma once
#include <functional>
#include <string>
#include <vector>
#include <Serialization/Project.h>

namespace ProductWindows
{
void Close();
void Message(const char* title, const std::string& text);
void About();
void Shortcuts();
void Diagnostics(const json& document);
void Welcome(const std::vector<std::string>& recent, std::function<void(std::string)> action);
// Complete compiler validation; estimated sizes describe source, not firmware RAM/flash.
std::string Preflight(const json& document, const std::string& target, const std::string& sourceDirectory);
void ExportSuccess(const std::string& path, bool cProject);
}
