#pragma once
#include "../ProjectThemes/DesignContext.h"
#include <sstream>
#include <map>
namespace ComponentLibraryWindow {
struct MasterReview {nlohmann::json candidate;std::string summary;size_t added=0,removed=0,changed=0,definitionChanges=0;};
inline MasterReview ReviewMaster(const nlohmann::json& document,const std::vector<int>& selection)
{
    namespace D=Serialization::ProjectDesign;using json=nlohmann::json;
    MasterReview review;review.candidate=document;const auto instance=D::SelectedInstance(document,selection);std::string definition;
    for(const auto& record:document.at("obj"))if(D::ComponentInfo(record).value("instance",std::string())==instance){definition=D::ComponentInfo(record).at("definition");break;}
    D::UpdateMaster(review.candidate,selection);
    const auto oldDefinition=[&]{for(const auto& value:document.at("components"))if(value.value("id",std::string())==definition)return value;throw std::invalid_argument("Component definition is missing");}();
    const auto& newDefinition=D::Definition(review.candidate,definition);
    const auto changes=json::diff(oldDefinition.at("records"),newDefinition.at("records"));review.definitionChanges=changes.size();
    auto collect=[](const json& doc){std::map<std::string,json> result;auto screen=[&](const json& records,const std::string& prefix){for(const auto& record:records){auto clean=D::CleanRecord(record);clean["par"]=record.value("par",-1);result[prefix+std::to_string(record.value("oid",-1))]=std::move(clean);}};
        if(doc.contains("screens")){const auto editor=doc.value("editorScreen",doc.value("activeScreen",size_t(0)));for(size_t i=0;i<doc["screens"].size();++i)screen(i==editor&&doc.contains("obj")?doc["obj"]:doc["screens"][i].at("obj"),std::to_string(i)+":");}
        else screen(doc.at("obj"),"");return result;};
    const auto before=collect(document),after=collect(review.candidate);
    for(const auto& pair:before){auto next=after.find(pair.first);if(next==after.end())++review.removed;else if(next->second!=pair.second)++review.changed;}
    for(const auto& pair:after)if(!before.count(pair.first))++review.added;
    std::ostringstream text;text<<"Master: "<<newDefinition.value("name",definition)<<"\nDefinition revision "<<oldDefinition.value("revision",1)<<" -> "<<newDefinition.value("revision",1)
        <<"\n"<<review.definitionChanges<<" definition changes; "<<review.changed<<" existing widgets change, "<<review.added<<" added, "<<review.removed<<" removed."
        <<"\nOther instances retain local text/style overrides. Placement stays local. Apply creates one undoable change.\n";
    auto brief=[](const json& value){if(value.is_object())return std::string("{widget / properties}");if(value.is_array())return std::string("[items]");auto label=value.dump();if(label.size()>80)label=label.substr(0,77)+"...";return label;};
    size_t count=0;for(const auto& change:changes){if(count++==16){text<<"Additional changes omitted from this summary.\n";break;}const auto path=change.value("path",std::string());text<<change.value("op",std::string())<<"  "<<path;if(change.contains("value")){json previous;try{previous=oldDefinition.at("records").at(json::json_pointer(path));}catch(...){}text<<": "<<brief(previous)<<" -> "<<brief(change["value"]);}text<<'\n';}
    review.summary=text.str();return review;
}
}
