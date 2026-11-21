#include "VisualAuthoring.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/PreviewProfiles/PreviewProfiles.h"
#include "../../../UICreator/PreviewProfiles/PreviewSurface.h"
#include "../../../UICreator/PreviewProfiles/LayoutDesigner.h"
#include "../../../UICreator/ProjectThemes/ThemeGallery.h"
#include "../../../UICreator/Components/ComponentLibrary.h"
#include <stdexcept>
#include <iostream>

namespace {
namespace P=PreviewProfiles;namespace D=Serialization::ProjectDesign;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
lv_obj_t* Label(lv_obj_t* root,const char* caption){if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root))==caption)return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=Label(lv_obj_get_child(root,i),caption))return found;return nullptr;}
lv_obj_t* Window(const char* caption){auto* title=Label(lv_layer_top(),caption);Require(title,"Expected visual authoring window is missing");return lv_obj_get_parent(lv_obj_get_parent(title));}
void Click(lv_obj_t* root,const char* text){auto* label=Label(root,text);Require(label,"Expected visual authoring action is missing");lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);}
lv_obj_t* Placeholder(lv_obj_t* root,const char* caption){if(lv_obj_has_class(root,&lv_textarea_class)){const auto* text=lv_textarea_get_placeholder_text(root);if(text&&std::string(text)==caption)return root;}for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=Placeholder(lv_obj_get_child(root,i),caption))return found;return nullptr;}
json Record(int oid,int parent,int x,int y,int width,int height){return {{"oid",oid},{"par",parent},{"base",{{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}}};}
json Scene(){json doc={{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({Record(0,-1,0,0,300,190),Record(1,0,0,0,160,44),Record(2,0,0,60,160,44)})},{"variables",json::array()}};doc["obj"][1]["button"]={{"text","First"}};doc["obj"][2]["button"]={{"text","Second"}};D::ApplyTheme(doc,"ocean-dark");P::LayoutOptions options;options.padding=5;options.gap=10;P::ApplyLayout(doc,{0},options);return doc;}
ProjectAuthoring::DesignContext Context(UI& editor,SdlSimulator& simulator){ProjectAuthoring::DesignContext context;context.document=[&editor]{return editor.ProjectDocument();};context.apply=[&editor](json doc){editor.ApplyDocument(std::move(doc));};context.selection=[&editor]{return editor.SelectedObjectIds();};context.select=[&editor](std::vector<int> ids){editor.SelectObjectIds(ids);};context.keyboardGroup=lv_indev_get_group(simulator.GetKeyboard());context.applyMetadata=[&editor](json patch){editor.ApplyMetadata(std::move(patch));};context.revision=[&editor]{return editor.DocumentRevision();};return context;}
}
void CheckVisualAuthoringEditor(UI& editor,SdlSimulator& simulator)
{
    CheckVisualModels();auto doc=Scene();const auto authored=doc;auto* display=lv_display_get_default();auto* screen=lv_screen_active();auto* group=lv_group_get_default();
    {
        auto* host=lv_obj_create(lv_layer_top());lv_obj_set_size(host,700,350);lv_obj_set_style_pad_all(host,0,0);lv_obj_set_scrollable(host,false);
        P::PreviewSurface surface(host);surface.Load(doc,320,240);auto* first=lvgl_runtime_object(surface.Runtime(),0,1);auto* second=lvgl_runtime_object(surface.Runtime(),0,2);Require(first&&second,"Responsive preview lost native objects");lv_obj_update_layout(lvgl_runtime_screen(surface.Runtime(),0));Require(lv_obj_get_y(second)>lv_obj_get_y(first),"Flex did not wrap on a small preview");
        const auto small=surface.PixelHash();surface.Load(doc,640,240);first=lvgl_runtime_object(surface.Runtime(),0,1);second=lvgl_runtime_object(surface.Runtime(),0,2);lv_obj_update_layout(lvgl_runtime_screen(surface.Runtime(),0));Require(lv_obj_get_y(second)==lv_obj_get_y(first),"Flex did not reflow on a wide preview");Require(surface.PixelHash()!=small,"Different device previews produced the same pixel buffer");
        const auto hash=surface.PixelHash();surface.Freeze();Require(!surface.Runtime()&&surface.PixelHash()==hash,"Freezing a thumbnail invalidated its pixels");Require(doc==authored,"Native preview mutated the authored scene");Require(lv_display_get_default()==display&&lv_screen_active()==screen&&lv_group_get_default()==group,"Native preview replaced editor display or group");
        surface.Clear();lv_obj_delete(host);
    }
    editor.ApplyDocument(doc);editor.SelectObjectIds({0});auto context=Context(editor,simulator);const auto baseline=editor.ProjectDocument();ProjectThemeWindow::Open(context);auto* gallery=Window("Theme gallery");Click(gallery,"Sage Studio");Require(editor.ProjectDocument()==baseline,"Choosing a theme preview changed the project");Click(gallery,"Preview and apply...");auto* review=Window("Preview project theme");Click(review,"Cancel preview");Require(editor.ProjectDocument()==baseline,"Cancelling a theme preview changed the project");
    Click(gallery,"Preview and apply...");review=Window("Preview project theme");Click(review,"Apply preview");Require(D::ThemeId(editor.ProjectDocument())=="forest-light","Confirmed theme preview was not applied");ProjectThemeWindow::Close();Require(editor.Undo(),"Confirmed theme is not undoable");
    doc=editor.ProjectDocument();const auto definition=D::Capture(doc,{0},"Responsive panel");const auto first=D::Insert(doc,definition),second=D::Insert(doc,definition);doc["obj"][4]["button"]["text"]="Updated first";doc["obj"][5]["button"]["text"]="Updated second";doc["obj"][7]["button"]["text"]="Local first";editor.ApplyDocument(doc);editor.SelectObjectIds(first);ComponentLibraryWindow::Open(context);auto* library=Window("Component library");
    auto* search=Placeholder(library,"Search saved components");Require(search,"Component search is missing");const auto beforeSearch=editor.ProjectDocument();lv_textarea_set_text(search,"not a component");Require(editor.ProjectDocument()==beforeSearch,"Component searching changed authored content");lv_textarea_set_text(search,"");
    Click(library,"Update selected master");review=Window("Review component master update");Require(editor.ProjectDocument()==beforeSearch,"Opening master review committed it");Click(review,"Cancel preview");Require(editor.ProjectDocument()==beforeSearch,"Cancelling master review modified the project");Click(library,"Update selected master");review=Window("Review component master update");Click(review,"Apply master update");doc=editor.ProjectDocument();Require(doc["components"][0]["revision"]==2,"Confirmed master update did not advance the revision");Require(doc["obj"][7]["button"]["text"]=="Local first"&&doc["obj"][8]["button"]["text"]=="Updated second","Reviewed update did not preserve local overrides");ComponentLibraryWindow::Close();
    P::Open(context);auto* comparison=Window("Device preview comparison");const auto beforeProfile=editor.ProjectDocument();auto* profileName=Placeholder(comparison,"Custom profile name");auto* width=Placeholder(comparison,"Width");auto* height=Placeholder(comparison,"Height");Require(profileName&&width&&height,"Custom device profile controls are missing");lv_textarea_set_text(profileName,"Desk panel");lv_textarea_set_text(width,"640");lv_textarea_set_text(height,"360");Click(comparison,"Save profile");Require(P::Profiles(editor.ProjectDocument()).size()==P::Presets().size()+1,"Named device profile was not persisted");Require(editor.ProjectDocument()["obj"]==beforeProfile["obj"],"Saving a preview profile altered widget geometry or styles");Click(comparison,"Remember comparison");Require(editor.ProjectDocument()["previewProfiles"].contains("left"),"Device comparison settings were not saved");P::Close();
    std::cout<<"Native responsive previews, pixel preservation, theme confirmation/cancel, component search/review/overrides and persisted device profiles passed\n";
    P::Open(context);for(int i=0;i<3;++i)simulator.ProcessEvents();
}
