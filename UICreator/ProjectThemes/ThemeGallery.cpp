#include "ThemeGallery.h"
#include "AuthoringControls.h"
#include "ThemeEditing.h"
#include "../PreviewProfiles/PreviewSurface.h"
#include "../PreviewProfiles/PreviewDialog.h"
#include <iomanip>
#include <sstream>

namespace ProjectThemeWindow {
namespace {
using namespace ProjectAuthoring;
namespace Design=Serialization::ProjectDesign;
struct Window {
    DesignContext context;
    lv_obj_t *root=nullptr,*cards=nullptr,*details=nullptr,*name=nullptr,*description=nullptr,*role=nullptr,*value=nullptr,*status=nullptr,*screenHost=nullptr,*styleChoice=nullptr,*styleStatus=nullptr;
    std::string chosen;
    std::vector<lv_obj_t*> tiles;
    std::unique_ptr<PreviewProfiles::PreviewSurface> screen;
    std::unique_ptr<PreviewProfiles::PreviewDialog> confirmation;
    json badges=json::array();int styleOid=-1;
    ~Window(){confirmation.reset();screen.reset();}
};
Window* active=nullptr;
const char* roles[]={"background","surface","text","muted","primary","accent","success","warning","error","border","onPrimary","radius","spacing"};
void ActualPreview(Window* w)
{
    try{auto doc=w->context.document();Design::ApplyTheme(doc,w->chosen);const auto canvas=doc.value("canvas",json::object());lv_obj_update_layout(w->root);if(!w->screen)w->screen=std::make_unique<PreviewProfiles::PreviewSurface>(w->screenHost);w->screen->Load(std::move(doc),canvas.value("width",320),canvas.value("height",240),w->context.sourceDirectory);w->screen->Freeze();}
    catch(const std::exception& e){if(w->status)lv_label_set_text(w->status,e.what());}
}
void RefreshBadges(Window* w)
{
    try{const auto ids=w->context.selection();w->styleOid=ids.empty()?-1:ids.front();w->badges=StyleBadges(w->context.document(),w->styleOid);std::string labels;
        for(const auto& badge:w->badges){if(!labels.empty())labels+='\n';labels+=badge.at("property").get<std::string>()+"  ["+(badge.at("inherited").get<bool>()?"shared "+badge.at("role").get<std::string>():"local override")+"]";}
        lv_dropdown_set_options(w->styleChoice,labels.empty()?"No themed selection":labels.c_str());
        const auto label=w->styleOid<0?"Screen":("Widget "+std::to_string(w->styleOid));lv_label_set_text(w->styleStatus,(label+": shared values follow their token; local overrides stay independent.").c_str());
    }catch(const std::exception& e){lv_label_set_text(w->styleStatus,e.what());}
}
void Color(lv_obj_t* object,unsigned value){lv_obj_set_style_bg_color(object,lv_color_hex(value),0);}
void Text(lv_obj_t* object,unsigned value){lv_obj_set_style_text_color(object,lv_color_hex(value),0);}
void Passive(lv_obj_t* object){lv_obj_set_clickable(object,false);for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)Passive(lv_obj_get_child(object,i));}
lv_obj_t* Label(lv_obj_t* parent,const char* text,int x,int y,unsigned color)
{auto* l=lv_label_create(parent);lv_label_set_text(l,text);lv_obj_set_pos(l,x,y);Text(l,color);return l;}
void Miniature(lv_obj_t* parent,const json& tokens,int width,int height)
{
    auto c=[&](const char* role){return tokens.at(role).get<unsigned>();};
    lv_obj_set_size(parent,width,height);Color(parent,c("background"));lv_obj_set_style_border_width(parent,0,0);
    lv_obj_set_style_radius(parent,8,0);lv_obj_set_style_pad_all(parent,0,0);lv_obj_set_scrollable(parent,false);
    Label(parent,"Workspace",12,9,c("text"));
    auto* card=lv_obj_create(parent);lv_obj_set_pos(card,10,33);lv_obj_set_size(card,width-20,height-43);
    Color(card,c("surface"));lv_obj_set_style_border_color(card,lv_color_hex(c("border")),0);
    lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_radius(card,tokens.at("radius").get<int>(),0);lv_obj_set_style_pad_all(card,0,0);lv_obj_set_scrollable(card,false);
    Label(card,"Your next great screen",9,8,c("text"));
    Label(card,"Designed for clarity",9,28,c("muted"));
    auto* action=lv_obj_create(card);lv_obj_set_pos(action,9,52);lv_obj_set_size(action,88,24);Color(action,c("primary"));
    lv_obj_set_style_border_width(action,0,0);lv_obj_set_style_radius(action,5,0);lv_obj_set_style_pad_all(action,0,0);lv_obj_set_scrollable(action,false);
    auto* label=lv_label_create(action);lv_label_set_text(label,"Continue");Text(label,c("onPrimary"));lv_obj_center(label);
    auto* indicator=lv_obj_create(card);lv_obj_set_size(indicator,10,10);lv_obj_set_pos(indicator,width-52,59);Color(indicator,c("success"));lv_obj_set_style_border_width(indicator,0,0);lv_obj_set_style_radius(indicator,5,0);
    // Previews never take focus or act as editor widgets.
    Passive(parent);
}
void RefreshPreview(Window* w)
{
    const auto tokens=Design::Tokens(w->context.document(),w->chosen);size_t index=0;
    for(const auto& preset:Design::Presets()) {
        auto* tile=w->tiles[index++];if(preset.at("id")!=w->chosen)continue;
        auto* preview=lv_obj_get_child(tile,1);lv_obj_clean(preview);Miniature(preview,tokens,228,130);
        auto* swatches=lv_obj_get_child(tile,2);unsigned swatch=0;
        for(const auto* role:{"primary","accent","success","warning","error","border"})Color(lv_obj_get_child(swatches,swatch++),tokens.at(role).get<unsigned>());
    }
}
void LoadToken(Window* w)
{
    const unsigned index=lv_dropdown_get_selected(w->role);if(index>=sizeof roles/sizeof *roles)return;
    const auto tokens=Design::Tokens(w->context.document(),w->chosen);std::ostringstream text;
    if(index<11)text<<'#'<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(6)<<tokens.at(roles[index]).get<unsigned>();
    else text<<tokens.at(roles[index]).get<int>();
    lv_textarea_set_text(w->value,text.str().c_str());
}
void Choose(Window* w,const std::string& id)
{
    w->chosen=id;const auto& preset=Design::Preset(id);
    lv_label_set_text(w->name,preset.at("name").get_ref<const std::string&>().c_str());
    lv_label_set_text(w->description,preset.at("description").get_ref<const std::string&>().c_str());
    size_t index=0;for(const auto& p:Design::Presets()) {auto* tile=w->tiles[index++];const bool selected=p.at("id")==id;lv_obj_set_style_border_color(tile,lv_color_hex(selected?EditorStyle::Accent:EditorStyle::Border),0);lv_obj_set_style_border_width(tile,selected?2:1,0);}
    LoadToken(w);
    ActualPreview(w);
}
void Apply(Window* w,bool selection)
{
    try {
        auto context=w->context;auto doc=context.document();auto ids=selection?context.selection():std::vector<int>{};
        if(selection&&ids.empty())throw std::invalid_argument("Select widgets on the canvas before applying to selection");
        Design::ApplyTheme(doc,w->chosen,ids);context.apply(std::move(doc));
        if(active==w){RefreshBadges(w);lv_label_set_text(w->status,selection?"Theme applied to the selected widgets. Undo is available.":"Project theme applied. New widgets inherit this palette. Undo is available.");}
    }catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}
}
void UpdateToken(Window* w)
{
    try {
        const unsigned index=lv_dropdown_get_selected(w->role);if(index>=sizeof roles/sizeof *roles)return;
        std::string value=lv_textarea_get_text(w->value);if(value.empty())throw std::invalid_argument("Enter a hex color or numeric size");
        int base=10;if(value[0]=='#'){value.erase(0,1);base=16;}
        size_t used=0;const auto number=std::stoul(value,&used,base);if(used!=value.size()||number>0xffffff)throw std::invalid_argument("Use #RRGGBB for colors or a size from 0 to 128");
        auto context=w->context;auto doc=context.document();Design::SetToken(doc,w->chosen,roles[index],static_cast<int>(number));context.apply(std::move(doc));
        if(active==w){RefreshPreview(w);ActualPreview(w);RefreshBadges(w);lv_label_set_text(w->status,"Shared token updated. Inspector overrides stay unchanged. Undo is available.");}
    }catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}
}
}
void Close(){if(active){auto* root=active->root;EditorWindow::ReleaseFocus(root);lv_obj_delete(root);}}
void Open(ProjectAuthoring::DesignContext context)
{
    Close();auto* w=new Window;w->context=std::move(context);active=w;
    w->root=EditorWindow::Create(lv_layer_top());lv_obj_set_size(w->root,LV_PCT(92),LV_PCT(92));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Theme gallery");
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* state=static_cast<Window*>(lv_event_get_user_data(e));if(active==state)active=nullptr;delete state;},LV_EVENT_DELETE,w);
    EditorWindow::SetCloseHandler(w->root,[]{Close();});
    auto* content=EditorWindow::Content(w->root);lv_obj_set_flex_flow(content,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_all(content,0,0);lv_obj_set_style_pad_column(content,0,0);lv_obj_set_scrollable(content,false);
    w->cards=lv_obj_create(content);lv_obj_set_width(w->cards,LV_PCT(68));lv_obj_set_height(w->cards,LV_PCT(100));Column(w->cards);lv_obj_set_flex_flow(w->cards,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(w->cards,12,0);
    w->details=lv_obj_create(content);lv_obj_set_width(w->details,LV_PCT(32));lv_obj_set_height(w->details,LV_PCT(100));Column(w->details);
    w->name=lv_label_create(w->details);lv_obj_set_width(w->name,LV_PCT(100));lv_label_set_long_mode(w->name,LV_LABEL_LONG_MODE_WRAP);
    w->description=EditorStyle::Help(w->details,"");
    EditorStyle::Help(w->details,"Your current screen (preview only)");
    w->screenHost=lv_obj_create(w->details);lv_obj_set_size(w->screenHost,LV_PCT(100),190);lv_obj_set_scrollable(w->screenHost,false);lv_obj_set_style_pad_all(w->screenHost,0,0);
    EditorStyle::Help(w->details,"Choose a palette to preview your screen without changing it. Apply commits; Cancel closes the gallery with no preview changes.");
    Button(w->details,"Preview and apply...",[w]{try{auto before=w->context.document();auto doc=before;Design::ApplyTheme(doc,w->chosen);w->confirmation=std::make_unique<PreviewProfiles::PreviewDialog>(w->root,w->context,doc,"Preview project theme","Current screen with the selected palette. Fonts, content, placement and code-widget contrast are preserved. Apply commits one undoable change.","Apply preview",[w,before,doc]{if(w->context.document()!=before){lv_label_set_text(w->status,"The project changed. Preview again before applying.");w->confirmation.reset();return;}auto context=w->context;context.apply(doc);if(active==w){w->confirmation.reset();RefreshBadges(w);lv_label_set_text(w->status,"Preview applied. Undo is available.");}},[w]{w->confirmation.reset();});}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}},220,w->context.keyboardGroup);
    Button(w->details,"Apply to project",[w]{Apply(w,false);},200,w->context.keyboardGroup);
    Button(w->details,"Apply to selection",[w]{Apply(w,true);},200,w->context.keyboardGroup);
    EditorStyle::Help(w->details,"Shared design tokens\nChange a token to update every widget using it. Values edited directly in the inspector remain local overrides.");
    w->role=lv_dropdown_create(w->details);lv_obj_set_width(w->role,LV_PCT(100));EditorStyle::Control(w->role);
    lv_dropdown_set_options(w->role,"Background\nSurface\nText\nMuted text\nPrimary\nAccent\nSuccess\nWarning\nError\nBorder\nOn primary\nCorner radius\nSpacing");Focus(w->role,w->context.keyboardGroup);
    w->value=Field(w->details,"#RRGGBB or size",w->context.keyboardGroup);lv_textarea_set_max_length(w->value,12);
    lv_obj_add_event_cb(w->role,[](lv_event_t* e){LoadToken(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    Button(w->details,"Update shared token",[w]{UpdateToken(w);},200,w->context.keyboardGroup);
    EditorStyle::Help(w->details,"Selected style inheritance");w->styleChoice=lv_dropdown_create(w->details);lv_obj_set_width(w->styleChoice,LV_PCT(100));EditorStyle::Control(w->styleChoice);Focus(w->styleChoice,w->context.keyboardGroup);
    w->styleStatus=EditorStyle::Help(w->details,"");
    Button(w->details,"Refresh selected styles",[w]{RefreshBadges(w);},220,w->context.keyboardGroup);
    Button(w->details,"Reset selected override",[w]{try{const auto index=lv_dropdown_get_selected(w->styleChoice);if(index>=w->badges.size())throw std::invalid_argument("Select a themed widget or screen first");auto doc=w->context.document();ResetStyleOverride(doc,w->styleOid,w->badges[index].at("property"));w->context.apply(std::move(doc));if(active==w){RefreshBadges(w);ActualPreview(w);lv_label_set_text(w->status,"Property restored to its shared token. Other overrides are preserved.");}}catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}},230,w->context.keyboardGroup);
    Button(w->details,"Cancel preview",[]{Close();},180,w->context.keyboardGroup);
    const auto overview=std::to_string(Design::Presets().size())+" coordinated palettes, including light, dark and high-contrast styles. Theme changes are included in project files and device exports.";
    w->status=EditorStyle::Help(w->details,overview.c_str());
    const auto document=w->context.document();
    for(const auto& p:Design::Presets()) {
        const auto id=p.at("id").get<std::string>();const auto tokens=Design::Tokens(document,id);
        auto* tile=lv_obj_create(w->cards);lv_obj_set_size(tile,248,204);lv_obj_set_style_pad_all(tile,9,0);lv_obj_set_style_pad_row(tile,5,0);
        lv_obj_set_style_radius(tile,10,0);Color(tile,EditorStyle::Input);lv_obj_set_flex_flow(tile,LV_FLEX_FLOW_COLUMN);lv_obj_set_scrollable(tile,false);w->tiles.push_back(tile);
        auto* title=lv_label_create(tile);lv_label_set_text(title,p.at("name").get_ref<const std::string&>().c_str());Text(title,EditorStyle::Text);
        auto* preview=lv_obj_create(tile);Miniature(preview,tokens,228,130);
        auto* swatches=Row(tile);lv_obj_set_height(swatches,13);lv_obj_set_style_pad_column(swatches,5,0);
        for(const auto* role:{"primary","accent","success","warning","error","border"}) {auto* swatch=lv_obj_create(swatches);lv_obj_set_size(swatch,30,9);Color(swatch,tokens.at(role).get<unsigned>());lv_obj_set_style_border_width(swatch,0,0);lv_obj_set_style_radius(swatch,3,0);lv_obj_set_clickable(swatch,false);}
        auto* callback=new std::function<void()>([w,id]{Choose(w,id);});lv_obj_add_event_cb(tile,[](lv_event_t* e){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(e)==LV_EVENT_CLICKED)(*fn)();},LV_EVENT_ALL,callback);
        Focus(tile,w->context.keyboardGroup);
    }
    std::string initial=Design::ThemeId(document);try{Design::Preset(initial);}catch(...){initial="slate-light";}Choose(w,initial);RefreshBadges(w);
}
}
