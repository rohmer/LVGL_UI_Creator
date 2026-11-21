#pragma once
#include <filesystem>
class UI;
class SdlSimulator;
void CheckVariablesEditor(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory);
