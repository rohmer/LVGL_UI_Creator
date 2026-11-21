#pragma once
#include <filesystem>
class UI;
class SdlSimulator;
// Writes static native framebuffer fixtures and a measured input-replay report.
// An optional outputDirectory/replay-input.json supplies the replay actions.
void ExportVisualRegressionFixtures(UI&, SdlSimulator&, const std::filesystem::path& outputDirectory);
