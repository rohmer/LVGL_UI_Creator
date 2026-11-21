#pragma once
#include <filesystem>
class UI;
class SdlSimulator;
void CheckObjectIcons(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory={});
