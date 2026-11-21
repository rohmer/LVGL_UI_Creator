#include "LayoutDesigner.h"
#include "PreviewProfiles.h"
#include "PreviewSurface.h"
#include "../ProjectThemes/AuthoringControls.h"
#include <algorithm>
#include <sstream>
#include <set>
#include <cctype>

namespace PreviewProfiles {
using json=nlohmann::json;
namespace {
using namespace ProjectAuthoring;
struct Window {
    DesignContext context;
    lv_obj_t *root=nullptr,*mode=nullptr,*wrap=nullptr,*responsive=nullptr,*gap=nullptr,*padding=nullptr,*main=nullptr,*cross=nullptr,*columns=nullptr,*rows=nullptr,*status=nullptr,*host=nullptr;
    std::unique_ptr<PreviewSurface> surface;
};
Window* layout=nullptr;
lv_obj_t* Choice(lv_obj_t* parent,const char* caption,const char* options,lv_group_t* group){EditorStyle::Help(parent,caption);auto* choice=lv_dropdown_create(parent);lv_dropdown_set_options(choice,options);lv_obj_set_width(choice,LV_PCT(100));EditorStyle::Control(choice);Focus(choice,group);return choice;}
int Number(lv_obj_t* field){std::string text=lv_textarea_get_text(field);size_t used=0;const auto value=std::stoi(text,&used);if(used!=text.size())throw std::invalid_argument("Enter a whole-number spacing value");return value;}
LayoutOptions Options(Window* w){LayoutOptions value;value.mode=static_cast<LayoutMode>(lv_dropdown_get_selected(w->mode));value.wrap=lv_obj_is_checked(w->wrap);value.responsiveWidth=lv_obj_is_checked(w->responsive);value.gap=Number(w->gap);value.padding=Number(w->padding);const int main[]={LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_SPACE_AROUND,LV_FLEX_ALIGN_SPACE_EVENLY};const int cross[]={LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_END};value.mainAlign=main[(std::min)(lv_dropdown_get_selected(w->main),uint32_t(5))];value.crossAlign=cross[(std::min)(lv_dropdown_get_selected(w->cross),uint32_t(2))];value.columns=lv_textarea_get_text(w->columns);value.rows=lv_textarea_get_text(w->rows);return value;}
json Candidate(Window* w){auto doc=w->context.document();ApplyLayout(doc,w->context.selection(),Options(w));return doc;}
void Preview(Window* w){try{auto doc=Candidate(w);const auto canvas=doc.value("canvas",json::object());lv_obj_update_layout(w->root);if(!w->surface)w->surface=std::make_unique<PreviewSurface>(w->host);w->surface->Load(std::move(doc),canvas.value("width",320),canvas.value("height",240),w->context.sourceDirectory);w->surface->Freeze();lv_label_set_text(w->status,"Preview only. Apply layout commits one undoable change; Cancel keeps the design unchanged.");}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}}
}
void CloseLayout(){if(layout){auto* root=layout->root;EditorWindow::ReleaseFocus(root);lv_obj_delete(root);}}
void OpenLayout(ProjectAuthoring::DesignContext context)
{
    PreviewSurface::Blur();
    CloseLayout();auto* w=new Window;w->context=std::move(context);layout=w;
    w->root=EditorWindow::Create(lv_layer_top());lv_obj_set_size(w->root,LV_PCT(85),LV_PCT(90));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Flex and Grid layout designer");
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));if(layout==w)layout=nullptr;delete w;},LV_EVENT_DELETE,w);EditorWindow::SetCloseHandler(w->root,[]{CloseLayout();});
    auto* body=EditorWindow::Content(w->root);Column(body);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_ROW);lv_obj_set_scrollable(body,false);
    auto* controls=lv_obj_create(body);Column(controls);lv_obj_set_width(controls,LV_PCT(45));lv_obj_set_height(controls,LV_PCT(100));
    EditorStyle::Help(controls,"Applies to selected containers, or to the screen when no widget is selected. Preview changes without moving the authored design.");
    w->mode=Choice(controls,"Layout","Manual positions\nFlex row\nFlex column\nGrid",w->context.keyboardGroup);lv_dropdown_set_selected(w->mode,1);
    w->wrap=lv_checkbox_create(controls);lv_checkbox_set_text(w->wrap,"Wrap children");lv_obj_add_state(w->wrap,LV_STATE_CHECKED);Focus(w->wrap,w->context.keyboardGroup);
    w->responsive=lv_checkbox_create(controls);lv_checkbox_set_text(w->responsive,"Container fills available width");lv_obj_add_state(w->responsive,LV_STATE_CHECKED);Focus(w->responsive,w->context.keyboardGroup);
    auto* row=Row(controls);w->gap=Field(row,"Gap (px)",w->context.keyboardGroup);lv_obj_set_width(w->gap,120);lv_textarea_set_text(w->gap,"8");w->padding=Field(row,"Padding (px)",w->context.keyboardGroup);lv_obj_set_width(w->padding,140);lv_textarea_set_text(w->padding,"8");
    w->main=Choice(controls,"Main-axis alignment","Start\nCenter\nEnd\nSpace between\nSpace around\nSpace evenly",w->context.keyboardGroup);
    w->cross=Choice(controls,"Cross-axis alignment","Start\nCenter\nEnd",w->context.keyboardGroup);
    EditorStyle::Help(controls,"Grid tracks: pixels, content, or fr weights; for example 1fr,2fr,120. Blank rows create enough content-sized rows.");
    w->columns=Field(controls,"Columns",w->context.keyboardGroup);lv_textarea_set_text(w->columns,"1fr,1fr");w->rows=Field(controls,"Rows (automatic when blank)",w->context.keyboardGroup);
    auto* preview=lv_obj_create(body);Column(preview);lv_obj_set_flex_grow(preview,1);lv_obj_set_height(preview,LV_PCT(100));lv_obj_set_scrollable(preview,false);
    w->host=lv_obj_create(preview);lv_obj_set_size(w->host,LV_PCT(100),0);lv_obj_set_flex_grow(w->host,1);lv_obj_set_scrollable(w->host,false);lv_obj_set_style_pad_all(w->host,0,0);
    w->status=EditorStyle::Help(preview,"");auto* actions=Row(preview);
    Button(actions,"Preview layout",[w]{Preview(w);},160,w->context.keyboardGroup);
    Button(actions,"Apply layout",[w]{try{auto document=Candidate(w);w->context.apply(std::move(document));if(layout==w)lv_label_set_text(w->status,"Layout applied. Use device previews to check its reflow; Undo is available.");}catch(const std::exception& e){if(layout==w)lv_label_set_text(w->status,e.what());}},150,w->context.keyboardGroup);
    Button(actions,"Cancel",[]{CloseLayout();},100,w->context.keyboardGroup);
    Preview(w);
}
}
