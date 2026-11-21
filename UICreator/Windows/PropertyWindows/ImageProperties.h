#pragma once
#include "../../Utils/EditorStyle.h"
#ifndef _WIN32
#include "../../Utils/NativeFileDialogUI.h"
#endif
#include "BasicProperties.h"
#include <Serialization/ImageAsset.h>
#include "../../Images/ImageConverterWindow.h"
#include "../../Images/ImageConverter.h"
#include "../../Images/ImageManagerWindow.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#if LVGL_VERSION_MAJOR >= 9
class ImageProperties
{
    struct Picker { PropertyWindow* window; lv_obj_t* status; };
public:
    static void LoadFromPath(PropertyWindow* window, const std::filesystem::path& path)
    {
        EditorEdit::Scope edit;
        auto* image = window->GetSelectedObject();
        if (!image || lv_obj_get_class(image) != &lv_image_class) throw std::invalid_argument("Select an image widget first");
        auto extension=path.extension().u8string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(extension==".svg"||extension==".bmp") {auto converted=ImageConversion::Convert({path},ImageConversion::Defaults());if(!converted.success)throw std::runtime_error(converted.message);ImageConversion::Apply(image,converted.assets.at(0));}
        else Serialization::ImageAsset::LoadFromFile(image, path);
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(image));
        data->objectJson = Serialization::ObjectSerializer::SerializeObject(image);
        auto asset=data->objectJson.at("img").at("asset");auto id=window->Images().Add(asset.value("name",std::string("Image")),asset);window->Images().Apply(image,id);
        window->SetSelectedObject(image);
    }
    static void Update(PropertyWindow* window, const json& record)
    {
        BasicProperties::Update(window, record);
        auto* content = lv_obj_get_child(window->ObjectPropWin()->GetWindow(), 0);
        auto* selector=lv_dropdown_create(content);lv_obj_set_width(selector,LV_PCT(100));
        std::string options="Custom / current image";uint32_t index=1,selected=0;
        for(const auto& entry:window->Images().ToJSON()){options+='\n';options+=entry.at("name").get<std::string>();if(entry.at("id")==ImageLibrary::Selection(window->GetSelectedObject()))selected=index;++index;}
        lv_dropdown_set_options(selector,options.c_str());lv_dropdown_set_selected(selector,selected);
        lv_obj_add_event_cb(selector,[](lv_event_t* e){auto* window=static_cast<PropertyWindow*>(lv_event_get_user_data(e));FocusEditorControl(window->GetKBGroup(),lv_event_get_target_obj(e));},LV_EVENT_CLICKED,window);
        lv_obj_add_event_cb(selector,[](lv_event_t* e){EditorEdit::Scope edit;auto* window=static_cast<PropertyWindow*>(lv_event_get_user_data(e));auto choice=lv_dropdown_get_selected(lv_event_get_target_obj(e));if(!choice){auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(window->GetSelectedObject()));if(data)data->objectJson["base"].erase("imageId");return;}const auto& entries=window->Images().ToJSON();if(choice<=entries.size())window->Images().Apply(window->GetSelectedObject(),entries[choice-1].at("id"));},LV_EVENT_VALUE_CHANGED,window);
        auto* manager=lv_button_create(content);lv_obj_set_size(manager,LV_PCT(100),38);auto* managerLabel=lv_label_create(manager);lv_label_set_text(managerLabel,"Project images...");lv_obj_center(managerLabel);
        lv_obj_add_event_cb(manager,[](lv_event_t* e){ImageManagerWindow::Open(static_cast<PropertyWindow*>(lv_event_get_user_data(e)));},LV_EVENT_CLICKED,window);
        auto* button = lv_button_create(content);
        lv_obj_set_size(button, LV_PCT(100), 38);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
        auto* title = lv_label_create(button);
        lv_label_set_text(title, "Load image from disk...");
        lv_obj_center(title);
        auto* status = lv_label_create(content);
        lv_obj_set_width(status, LV_PCT(100));
        lv_label_set_long_mode(status, LV_LABEL_LONG_MODE_WRAP);
        const auto asset = record.at("img").value("asset", json::object());
        const auto caption = asset.empty() ? std::string("GIF, PNG, JPG, BMP, SVG") : asset.value("name", std::string("Image"));
        lv_label_set_text(status, caption.c_str());
        lv_obj_move_to_index(button, 0); lv_obj_move_to_index(status, 1);
        auto* convert=lv_button_create(content);lv_obj_set_size(convert,LV_PCT(100),38);auto* conversionTitle=lv_label_create(convert);lv_label_set_text(conversionTitle,"Image conversion options...");lv_obj_center(conversionTitle);lv_obj_move_to_index(convert,2);lv_obj_move_to_index(selector,0);lv_obj_move_to_index(manager,1);
        lv_obj_add_event_cb(convert,[](lv_event_t* event){ImageConverterWindow::Open(static_cast<PropertyWindow*>(lv_event_get_user_data(event)));},LV_EVENT_CLICKED,window);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* picker = static_cast<Picker*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete picker; return; }
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
#ifdef _WIN32
            wchar_t path[32768] = {};
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof dialog;
            dialog.hwndOwner = GetActiveWindow();
            dialog.lpstrFilter = L"Images (*.gif;*.png;*.jpg;*.jpeg;*.bmp;*.svg)\0*.gif;*.png;*.jpg;*.jpeg;*.bmp;*.svg\0\0";
            dialog.lpstrFile = path; dialog.nMaxFile = _countof(path);
            dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
            dialog.lpstrTitle = L"Load image";
            if (!GetOpenFileNameW(&dialog)) return;
            auto* window = picker->window;
            auto* status = picker->status;
            try { LoadFromPath(window, std::filesystem::path(path)); }
            catch (const std::exception& error) { lv_label_set_text(status, error.what()); }
#else
            const auto path=NativeFileDialogUI::PickOne(false,false,L"Load image",L"Images\0*.gif;*.png;*.jpg;*.jpeg;*.bmp;*.svg\0All files\0*.*\0\0");
            if(path.empty())return;
            try{LoadFromPath(picker->window,path);}catch(const std::exception& error){lv_label_set_text(picker->status,error.what());}
#endif
        }, LV_EVENT_ALL, new Picker{window, status});
    }
};
#endif
