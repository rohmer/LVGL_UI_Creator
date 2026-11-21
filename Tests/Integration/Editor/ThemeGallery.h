#pragma once
#include <filesystem>
class UI;
class SdlSimulator;
void CheckThemeGallery(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory);
