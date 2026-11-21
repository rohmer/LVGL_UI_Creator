#include "catch.hpp"
#include "CCodeGenerator.h"
#include <filesystem>
#include <fstream>
using json=nlohmann::json;
namespace {
json ScreenProject() {
    std::ifstream input(std::filesystem::path(UICREATOR_SOURCE_ROOT)/"Tests/Integration/CodeGeneration/ScreenFocus/project.json");
    return json::parse(input);
}
}
TEST_CASE("Generated screen metadata validates before touching output", "[targets][screens][accessibility]") {
    for(const char* target:{"9.6","9.5","9.3","9.2","9.1","8.3"}) {
        CCodeGenerator generator;generator.SetTargetVersion(target);json summary;
        REQUIRE(generator.LoadProject(ScreenProject()));INFO(generator.GetLastError());CHECK(generator.Preflight(summary));
        for(const auto& invalid:std::vector<json>{-1,2001,1.5,"80"}) {
            auto project=ScreenProject();project["screens"][0]["transition"]["duration"]=invalid;
            REQUIRE(generator.LoadProject(project));CHECK_FALSE(generator.Preflight(summary));
        }
        auto project=ScreenProject();project["screens"][0]["transition"]["type"]="unknown";
        REQUIRE(generator.LoadProject(project));CHECK_FALSE(generator.Preflight(summary));
        for(const auto& invalid:std::vector<json>{-2,INT64_C(2147483648),1.5,"1"}) {
            project=ScreenProject();project["screens"][0]["obj"][0]["base"]["accessibility"]["tabIndex"]=invalid;
            REQUIRE(generator.LoadProject(project));CHECK_FALSE(generator.Preflight(summary));
        }
    }
}
TEST_CASE("Plain generated screens omit optional focus storage and transition helpers", "[targets][screens][minimal]") {
    auto project=ScreenProject();for(auto& screen:project["screens"]) {
        screen.erase("transition");for(auto& object:screen["obj"])object["base"].erase("accessibility");
    }
    const auto directory=std::filesystem::path(UICREATOR_UNIT_OUTPUT)/"plain-screens";
    CCodeGenerator generator;REQUIRE(generator.LoadProject(project));REQUIRE(generator.ExportProject(directory.u8string()));
    std::ifstream input(directory/"ui.c");const std::string source((std::istreambuf_iterator<char>(input)),{});
    CHECK(source.find("ui_focus_entries")==std::string::npos);
    CHECK(source.find("UI_SCREEN_ANIM_")==std::string::npos);
    CHECK(source.find("lv_screen_load(screen)")!=std::string::npos);
}
