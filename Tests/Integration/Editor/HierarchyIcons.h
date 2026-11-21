#pragma once
#include <filesystem>
class UI;
class SdlSimulator;
void CheckHierarchyIcons(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory={});
