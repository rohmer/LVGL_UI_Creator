#pragma once
#include "PreviewSurface.h"
#include "../ProjectThemes/DesignContext.h"
namespace PreviewProfiles {
// Child dialog; the owner must release this object before its editor window.
class PreviewDialog {
public:
    PreviewDialog(lv_obj_t* parent,const ProjectAuthoring::DesignContext& context,nlohmann::json candidate,
        const char* title,const std::string& summary,const char* applyCaption,std::function<void()> apply,std::function<void()> cancel);
    ~PreviewDialog();
    lv_obj_t* Window()const{return root;}
private:
    lv_obj_t* root=nullptr;
    std::unique_ptr<PreviewSurface> surface;
};
}
