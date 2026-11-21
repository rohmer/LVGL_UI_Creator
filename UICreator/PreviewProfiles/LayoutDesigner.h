#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <string>
#include <vector>
namespace PreviewProfiles {
enum class LayoutMode { Manual, Row, Column, Grid };
struct LayoutOptions {
    LayoutMode mode=LayoutMode::Row;
    bool wrap=true,responsiveWidth=true;
    int gap=8,padding=8,mainAlign=0,crossAlign=0;
    std::string columns="1fr,1fr",rows="content";
};
nlohmann::json ParseTracks(const std::string& value);
void ApplyLayout(nlohmann::json& document,const std::vector<int>& selection,const LayoutOptions& options);
}
