#pragma once
#include <filesystem>
class UI;
class SdlSimulator;
void CheckAdvancedControlsEditor(UI& editor, SdlSimulator& simulator, const std::filesystem::path& capture);
