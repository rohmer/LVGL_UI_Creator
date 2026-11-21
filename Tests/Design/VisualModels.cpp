#include "../../UICreator/PreviewProfiles/ProfileModels.h"
#include "../../UICreator/PreviewProfiles/LayoutDesigner.h"
#include "../../UICreator/ProjectThemes/ThemeEditing.h"
#include "../../UICreator/Components/ComponentReview.h"
#include <iostream>
#include <stdexcept>

namespace {
using json=nlohmann::json;namespace D=Serialization::ProjectDesign;namespace P=PreviewProfiles;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Fn> void Reject(Fn action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,message);}
json Record(int id,int parent,int x,int y,int width,int height){return {{"oid",id},{"par",parent},{"base",{{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}}};}
json Scene(){json doc={{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({Record(0,-1,0,0,300,200),Record(1,0,7,9,160,40),Record(2,0,10,90,160,40)})},{"variables",json::array()}};doc["obj"][1]["button"]={{"text","First"}};doc["obj"][2]["button"]={{"text","Second"}};return doc;}
}
void CheckVisualModels()
{
    auto doc=Scene();const auto original=doc;
    const auto portrait=P::AtResolution(doc,240,320);Require(doc==original&&portrait["obj"]==doc["obj"],"Preview resizing modified authored geometry");Require(portrait["canvas"]["width"]==240,"Preview resolution was not applied");
    Reject([&]{P::AtResolution(doc,0,320);},"Zero preview width was accepted");Reject([&]{P::AtResolution(doc,4096,4096);},"Oversized preview buffer was accepted");
    P::SaveProfile(doc,"Desk panel",640,480);Require(P::Profiles(doc).size()==P::Presets().size()+1,"Named profile was not added");P::SaveProfile(doc,"Desk panel",800,480);Require(P::Profiles(doc).size()==P::Presets().size()+1,"Updating a named profile duplicated it");Require(doc["previewProfiles"]["custom"][0]["width"]==800,"Named profile dimensions were not updated");
    const auto beforeProfile=doc;Reject([&]{P::SaveProfile(doc,"bad\nname",320,240);},"Invalid profile name was accepted");Require(doc==beforeProfile,"Rejected profile altered project");
    auto malformed=Scene();malformed["previewProfiles"]={{"custom",json::array({{{"id","broken"},{"name","Broken"},{"width","wide"},{"height",240}}})}};Require(P::Profiles(malformed).size()==P::Presets().size(),"Malformed imported profile was not ignored");
    P::LayoutOptions layout;layout.mode=P::LayoutMode::Row;layout.padding=5;layout.gap=10;D::ApplyTheme(doc,"ocean-dark");const auto coordinates=doc["obj"][1]["base"]["coords"];P::ApplyLayout(doc,{0},layout);Require(doc["obj"][1]["base"]["coords"]==coordinates,"Layout editor rewrote child pixel coordinates");Require(D::Style(doc["obj"][0])["width"]==LV_PCT(100),"Responsive container width was not configured");
    const auto row=doc;layout.mode=P::LayoutMode::Grid;layout.columns="1fr,2fr,120";layout.rows="";P::ApplyLayout(doc,{0},layout);Require(D::Style(doc["obj"][0])["grid_column_dsc_array"].size()==3,"Grid track model was not saved");Require(D::Style(doc["obj"][2])["grid_cell_column_pos"]==1,"Grid cells were not assigned");Require(P::ParseTracks("100,content,2fr").size()==3,"Mixed native grid tracks were rejected");Reject([&]{P::ParseTracks("0fr");},"Zero grid weight was accepted");Reject([&]{P::ParseTracks("1fr,,2fr");},"Empty grid track was accepted");
    const auto grid=doc;layout.columns="bad";Reject([&]{P::ApplyLayout(doc,{0},layout);},"Invalid layout was accepted");Require(doc==grid,"Rejected layout partially modified the design");
    doc=row;D::Style(doc["obj"][0])["bg_color"]=0xffaabbccu;D::Style(doc["obj"][0])["border_width"]=3;D::Materialize(doc);const auto badges=ProjectAuthoring::StyleBadges(doc,0);bool local=false;for(const auto& badge:badges)if(badge["property"]=="bg_color")local=!badge["inherited"].get<bool>();Require(local,"Style inspector did not distinguish a local override");ProjectAuthoring::ResetStyleOverride(doc,0,"bg_color");Require((D::Style(doc["obj"][0])["bg_color"].get<unsigned>()&0xffffff)==D::Preset("ocean-dark")["tokens"]["surface"],"Individual style reset did not restore its token");Require(D::Style(doc["obj"][0])["border_width"]==3,"Individual reset removed an unrelated override");
    auto multi=doc;multi["screens"]=json::array({{{"canvas",doc["canvas"]},{"obj",doc["obj"]}},{{"canvas",doc["canvas"]},{"obj",doc["obj"]}}});multi["editorScreen"]=0;multi["activeScreen"]=1;D::ApplyTheme(multi,"forest-dark");D::SetToken(multi,"forest-dark","surface",0x173a29);auto flattened=multi;D::Materialize(flattened,true);for(auto& screen:flattened["screens"])Require((D::Style(screen["obj"][0])["bg_color"].get<unsigned>()&0xffffff)==0x173a29,"Inactive screen lost shared project tokens");Require(!flattened.contains("previewProfiles")&&!flattened.contains("projectTheme"),"Device export retained preview authoring metadata");
    doc=Scene();const auto definition=D::Capture(doc,{0},"Panel");const auto first=D::Insert(doc,definition,-1,20,20),second=D::Insert(doc,definition,-1,20,20);doc["obj"][4]["button"]["text"]="Shared first";doc["obj"][5]["button"]["text"]="Shared second";doc["obj"][7]["button"]["text"]="Local first";
    const auto beforeReview=doc;const auto review=ComponentLibraryWindow::ReviewMaster(doc,first);Require(doc==beforeReview,"Reviewing a master mutated the design before Apply");Require(review.definitionChanges>0&&review.changed>0,"Master review omitted property changes");Require(review.candidate["obj"][7]["button"]["text"]=="Local first"&&review.candidate["obj"][8]["button"]["text"]=="Shared second","Master review lost local overrides or propagation");
    json other={{"obj",json::array()},{"components",doc["components"]}};D::Insert(other,definition);doc["screens"]=json::array({{{"obj",doc["obj"]},{"canvas",doc["canvas"]}},{{"obj",other["obj"]},{"canvas",doc["canvas"]}}});doc["editorScreen"]=0;Require(D::InstanceCount(doc,definition)==3,"Component instance count omitted an inactive screen");D::UpdateMaster(doc,first);Require(doc["screens"][1]["obj"][2]["button"]["text"]=="Shared second","Master update did not propagate to an inactive screen");
    std::cout<<"Visual models passed: safe device profiles, native responsive layouts, per-property inheritance, shared multiscreen tokens and reviewed component updates\n";
}
#ifdef VISUAL_MODELS_STANDALONE
int main(){try{CheckVisualModels();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
