#include "LayoutDesigner.h"
#include <Serialization/ProjectDesign.h>
#include <lvgl/lvgl.h>
#include <algorithm>
#include <sstream>
#include <set>
#include <cctype>
namespace PreviewProfiles {
using json=nlohmann::json;
json ParseTracks(const std::string& value)
{
    json tracks=json::array();std::istringstream stream(value);std::string item;
    while(std::getline(stream,item,',')) {
        item.erase(std::remove_if(item.begin(),item.end(),[](unsigned char c){return std::isspace(c)!=0;}),item.end());
        if(item.empty())throw std::invalid_argument("Grid tracks cannot be empty; use a size, content, or 1fr");
        if(tracks.size()>=16)throw std::invalid_argument("Use at most 16 grid tracks");
        if(item=="content"){tracks.push_back(LV_GRID_CONTENT);continue;}
        const bool fraction=item.size()>2&&item.substr(item.size()-2)=="fr";if(fraction)item.resize(item.size()-2);
        if(item.empty()||item.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("Grid tracks use whole pixels, content, or positive fr weights");
        const int number=std::stoi(item);if(number<1||number>(fraction?100:4096))throw std::invalid_argument("Grid pixels must be 1-4096; fr weights must be 1-100");
        tracks.push_back(fraction?LV_GRID_FR(number):number);
    }
    if(tracks.empty())throw std::invalid_argument("Enter at least one grid column");return tracks;
}
void ApplyLayout(json& document,const std::vector<int>& selection,const LayoutOptions& options)
{
    if(int(options.mode)<0||int(options.mode)>3||options.gap<0||options.gap>256||options.padding<0||options.padding>256||options.mainAlign<0||options.mainAlign>5||options.crossAlign<0||options.crossAlign>2)
        throw std::invalid_argument("Layout spacing must be 0-256 and alignment must be a valid native LVGL alignment");
    auto candidate=document;auto* objects=&candidate["obj"];if(!objects->is_array())throw std::invalid_argument("The current screen has no widget list");
    std::set<int> ids(selection.begin(),selection.end());if(ids.empty())ids.insert(-1);
    for(int id:ids) {
        json* parent=nullptr;
        if(id<0)parent=&candidate["canvas"];
        else for(auto& object:*objects)if(object.value("oid",-2)==id){parent=&object;break;}
        if(!parent)throw std::invalid_argument("A selected layout target no longer exists");
        if(parent->contains("text")||parent->contains("img")||parent->contains("barcode")||parent->contains("qrcode"))throw std::invalid_argument("Select a container or the screen to edit its layout");
        auto& style=Serialization::ProjectDesign::Style(*parent);
        if(options.mode==LayoutMode::Manual){style["layout"]=LV_LAYOUT_NONE;continue;}
        style["pad_row"]=options.gap;style["pad_column"]=options.gap;
        for(const auto* property:{"pad_left","pad_right","pad_top","pad_bottom"})style[property]=options.padding;
        if(options.responsiveWidth&&id>=0)style["width"]=LV_PCT(100);
        std::vector<json*> children;for(auto& object:*objects)if(object.value("par",-1)==id)children.push_back(&object);
        if(options.mode==LayoutMode::Grid) {
            const auto columns=ParseTracks(options.columns);auto rows=options.rows.empty()?json::array():ParseTracks(options.rows);
            const size_t required=(std::max)(size_t(1),(children.size()+columns.size()-1)/columns.size());
            if(rows.empty())for(size_t i=0;i<required;++i)rows.push_back(LV_GRID_CONTENT);
            if(rows.size()<required)throw std::invalid_argument("Add enough row tracks for the children, or leave rows blank for automatic content rows");
            style["layout"]=LV_LAYOUT_GRID;style["grid_column_dsc_array"]=columns;style["grid_row_dsc_array"]=rows;
            for(size_t i=0;i<children.size();++i){auto& child=Serialization::ProjectDesign::Style(*children[i]);child["grid_cell_column_pos"]=i%columns.size();child["grid_cell_row_pos"]=i/columns.size();child["grid_cell_column_span"]=1;child["grid_cell_row_span"]=1;child["grid_cell_x_align"]=LV_GRID_ALIGN_STRETCH;child["grid_cell_y_align"]=LV_GRID_ALIGN_STRETCH;}
        } else {
            style["layout"]=LV_LAYOUT_FLEX;
            style["flex_flow"]=options.mode==LayoutMode::Row?(options.wrap?LV_FLEX_FLOW_ROW_WRAP:LV_FLEX_FLOW_ROW):(options.wrap?LV_FLEX_FLOW_COLUMN_WRAP:LV_FLEX_FLOW_COLUMN);
            style["flex_main_place"]=options.mainAlign;style["flex_cross_place"]=options.crossAlign;style["flex_track_place"]=options.crossAlign;
        }
        // Inspector edits to layout padding become local overrides of spacing
        // tokens; all other theme bindings remain live.
        Serialization::ProjectDesign::ResolveTheme(*parent,candidate);
    }
    document=std::move(candidate);
}
}
