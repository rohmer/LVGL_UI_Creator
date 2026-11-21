#include "PreviewDialog.h"
#include "../ProjectThemes/AuthoringControls.h"
namespace PreviewProfiles {
PreviewDialog::PreviewDialog(lv_obj_t* parent,const ProjectAuthoring::DesignContext& context,nlohmann::json candidate,
    const char* title,const std::string& summary,const char* applyCaption,std::function<void()> apply,std::function<void()> cancel)
{
    using namespace ProjectAuthoring;
    root=EditorWindow::Create(parent);lv_obj_set_floating(root,true);lv_obj_set_size(root,LV_PCT(96),LV_PCT(94));lv_obj_center(root);EditorWindow::SetTitle(root,title);EditorWindow::SetCloseHandler(root,cancel);
    auto* body=EditorWindow::Content(root);Column(body);lv_obj_set_scroll_dir(body,LV_DIR_VER);
    auto* reportPanel=lv_obj_create(body);lv_obj_set_size(reportPanel,LV_PCT(100),150);lv_obj_set_scroll_dir(reportPanel,LV_DIR_VER);lv_obj_set_style_pad_all(reportPanel,8,0);
    auto* report=EditorStyle::Help(reportPanel,summary.c_str());
    auto* host=lv_obj_create(body);lv_obj_set_size(host,LV_PCT(100),0);lv_obj_set_style_min_height(host,180,0);lv_obj_set_flex_grow(host,1);lv_obj_set_style_pad_all(host,0,0);lv_obj_set_scrollable(host,false);lv_obj_set_style_bg_color(host,lv_color_hex(0x111820),0);
    auto* actions=Row(body);Button(actions,applyCaption,std::move(apply),200,context.keyboardGroup);Button(actions,"Cancel preview",std::move(cancel),160,context.keyboardGroup);
    lv_obj_update_layout(root);const auto canvas=candidate.value("canvas",nlohmann::json::object());
    surface=std::make_unique<PreviewSurface>(host);
    try{surface->Load(std::move(candidate),canvas.value("width",320),canvas.value("height",240),context.sourceDirectory);surface->Freeze();}
    catch(const std::exception& e){lv_label_set_text(report,e.what());}
}
PreviewDialog::~PreviewDialog(){surface.reset();if(root)lv_obj_delete(root);}
}
