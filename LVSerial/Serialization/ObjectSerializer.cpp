#include "ObjectSerializer.h"
#include "BasicWidgets.h"
#include "ExtendedWidgets.h"
#include "CustomWidgets.h"
#include "CodeWidgets.h"
#include "AdvancedControls.h"
#include "AdvancedKeyboard.h"
#include <algorithm>
using namespace Serialization;

json ObjectSerializer::SerializeObject(lv_obj_t* obj)
{
    if (!obj)
        return nullptr;
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
    const auto* type = lv_obj_get_class(obj);
    if (AdvancedKeyboard::Type(obj)) return AdvancedKeyboard::ToJSON(obj);
    if (CodeWidgets::Type(obj)) return CodeWidgets::ToJSON(obj);
    if (AdvancedControls::Type(obj)) return AdvancedControls::ToJSON(obj);
    if (CustomWidgets::Type(obj)) return CustomWidgets::ToJSON(obj);
    if (BasicWidgets::Type(obj)) return BasicWidgets::ToJSON(obj);
    if (ExtendedWidgets::Type(obj)) return ExtendedWidgets::ToJSON(obj);
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
    if (type == &lv_arc_class) return LVArc::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
    if (type == &lv_bar_class) return LVBar::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
    if (type == &lv_button_class) return LVButton::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
    if (type == &lv_buttonmatrix_class) return LVButtonMatrix::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
    if (type == &lv_calendar_class) return LVCalendar::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
    if (type == &lv_canvas_class) return LVCanvas::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
    if (type == &lv_checkbox_class) return LVCheckBox::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
    if (type == &lv_image_class) return LVImage::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
    if (type == &lv_label_class) return LVLabel::ToJSON(obj);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
    if (lv_obj_has_class(obj, &lv_button_class)) return LVButton::ToJSON(obj);
#endif
#else
    lv_obj_type_t type;
    lv_obj_get_type(obj, &type);

    if (strcmp(type.type[0], "lv_arc") == 0)
    {
        return LVArc::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_bar") == 0)
    {
        return LVBar::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_btn") == 0)
    {
        return LVButton::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_btnm") == 0)
    {
        return LVButtonMatrix::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_calendar") == 0)
    {
        return LVCalendar::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_canvas") == 0)
    {
        return LVCanvas::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_cb") == 0)
    {
        return LVCheckBox::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_img") == 0)
    {
        return LVImage::ToJSON(obj);
    }
    if (strcmp(type.type[0], "lv_label") == 0)
    {
        return LVLabel::ToJSON(obj);
    }
#endif
    return {{"base", LVObject::ToJSON(obj)}};
}

lv_obj_t* ObjectSerializer::DeserializeObject(json record)
{
    if (!record.is_object() || !record.contains("base"))
        return nullptr;
#if LVSERIAL_MINIMAL_RUNTIME
    const auto enabled = SupportedTypes();
    for(const auto& item : std::vector<std::pair<const char*,const char*>>{
        {"arc","arc"},{"bar","bar"},{"button","button"},{"btnm","buttonMatrix"},{"cal","calendar"},
        {"canvas","canvas"},{"cb","checkbox"},{"img","image"},{"text","label"},
        {"dropdown","dropdown"},{"roller","roller"},{"slider","slider"},{"spinbox","spinbox"},
        {"switch","switch"},{"textarea","textarea"},{"led","led"},{"line","line"},{"spinner","spinner"},
        {"chart","chart"},{"table","table"},{"scale","scale"},{"keyboard","keyboard"},
        {"tabview","tabview"},{"tileview","tileview"},{"window","window"},{"messagebox","messagebox"},
        {"imagebutton","imagebutton"},{"textBox","textBox"},{"layerView","layerView"},
        {"barcode","barcode"},{"qrcode","qrcode"},{"advancedKeyboard","advancedKeyboard"},
        {"radioGroup","radioGroup"},{"checkboxButton","checkboxButton"},{"labeledCheckbox","labeledCheckbox"},{"datePicker","datePicker"},{"menu","menu"}})
        if(record.contains(item.first) && std::find(enabled.begin(),enabled.end(),item.second)==enabled.end())
            throw std::invalid_argument(std::string("Widget disabled in this runtime: ")+item.second);
#endif
    // Advanced keyboard must fail explicitly on older or feature-trimmed builds.
    if (record.contains("advancedKeyboard")) return AdvancedKeyboard::FromJSON(record);
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
    if (auto* object = CodeWidgets::FromJSON(record)) return object;
    if (auto* object = AdvancedControls::FromJSON(record)) return object;
    if (auto* object = CustomWidgets::FromJSON(record)) return object;
    if (auto* object = BasicWidgets::FromJSON(record)) return object;
    if (auto* object = ExtendedWidgets::FromJSON(record)) return object;
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
    if (record.contains("arc")) return LVArc::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
    if (record.contains("bar")) return LVBar::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
    if (record.contains("button")) return LVButton::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
    if (record.contains("btnm")) return LVButtonMatrix::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
    if (record.contains("cal")) return LVCalendar::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
    if (record.contains("canvas")) return LVCanvas::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
    if (record.contains("cb")) return LVCheckBox::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
    if (record.contains("img")) return LVImage::FromJSON(std::move(record));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
    if (record.contains("text")) return LVLabel::FromJSON(std::move(record));
#endif
    return LVObject::FromJSON(std::move(record["base"]));
}


const char* ObjectSerializer::TypeName(lv_obj_t* object)
{
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
    if (const auto* type = AdvancedKeyboard::Type(object)) return type;
    if (const auto* type = CodeWidgets::Type(object)) return type;
    if (const auto* type = AdvancedControls::Type(object)) return type;
    if (const auto* type = CustomWidgets::Type(object)) return type;
    if (const auto* type = BasicWidgets::Type(object)) return type;
    if (const auto* type = ExtendedWidgets::Type(object)) return type;
    const auto* type = lv_obj_get_class(object);
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
    if (type == &lv_arc_class) return "arc";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
    if (type == &lv_bar_class) return "bar";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
    if (type == &lv_button_class) return "button";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
    if (type == &lv_buttonmatrix_class) return "buttonMatrix";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
    if (type == &lv_calendar_class) return "calendar";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
    if (type == &lv_canvas_class) return "canvas";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
    if (type == &lv_checkbox_class) return "checkbox";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
    if (type == &lv_image_class) return "image";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
    if (type == &lv_label_class) return "label";
#endif
#endif
    return "object";
}
Binary ObjectSerializer::ToProtobuf(lv_obj_t* object)
{
    if (!object) throw std::invalid_argument("Widget is null");
    return Protobuf::Encode(EntityKind::Widget, TypeName(object), SerializeObject(object));
}
lv_obj_t* ObjectSerializer::FromProtobuf(const Binary& input)
{
    const auto record = Protobuf::Decode(input);
    if (record.kind != EntityKind::Widget) throw std::invalid_argument("Expected a widget record");
    auto* object = DeserializeObject(record.data);
    if (object && record.type != TypeName(object)) { lv_obj_delete(object); throw std::invalid_argument("Widget type does not match its properties"); }
    return object;
}


std::vector<std::string> ObjectSerializer::SupportedTypes()
{
    std::vector<std::string> result{"page", "object"};
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
    result.push_back("arc");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
    result.push_back("bar");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
    result.push_back("button");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
    result.push_back("buttonMatrix");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
    result.push_back("calendar");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
    result.push_back("canvas");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
    result.push_back("checkbox");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
    result.push_back("image");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
    result.push_back("label");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
    result.push_back("dropdown");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
    result.push_back("roller");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
    result.push_back("slider");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
    result.push_back("spinbox");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SWITCH
    result.push_back("switch");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
    result.push_back("textarea");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LED
    result.push_back("led");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LINE
    result.push_back("line");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINNER
    result.push_back("spinner");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHART
    result.push_back("chart");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABLE
    result.push_back("table");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SCALE
    result.push_back("scale");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_KEYBOARD
    result.push_back("keyboard");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
    result.push_back("tabview");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
    result.push_back("tileview");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_WINDOW
    result.push_back("window");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
    result.push_back("messagebox");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
    result.push_back("imagebutton");
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6 && (!LVSERIAL_MINIMAL_RUNTIME || LV_USE_BARCODE)
    result.push_back("barcode");
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6 && (!LVSERIAL_MINIMAL_RUNTIME || LV_USE_QRCODE)
    result.push_back("qrcode");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_TEXTBOX
    result.push_back("textBox");
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_LAYERVIEW
    result.push_back("layerView");
#endif
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    result.push_back("advancedKeyboard");
#endif
#if LVSERIAL_USE_RADIO_GROUP
    result.push_back("radioGroup");
#endif
#if LVSERIAL_USE_CHECKBOX_BUTTON
    result.push_back("checkboxButton");
#endif
#if LVSERIAL_USE_LABELED_CHECKBOX
    result.push_back("labeledCheckbox");
#endif
#if LVSERIAL_USE_DATE_PICKER
    result.push_back("datePicker");
#endif
#if LVSERIAL_USE_MENU
    result.push_back("menu");
#endif
    return result;
}
lv_obj_t* ObjectSerializer::Create(const std::string& type, lv_obj_t* parent)
{
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
    if (!parent) parent = lv_screen_active();
    lv_obj_t* object = type == "advancedKeyboard" ? AdvancedKeyboard::Create(parent) : nullptr;
    if (!object) {
        object = CodeWidgets::Create(type, parent);
        if (object) CodeWidgets::Apply(object, json::object());
    }
    if (!object) object = AdvancedControls::Create(type, parent);
    if (!object) object = CustomWidgets::Create(type, parent);
    if (!object) object = BasicWidgets::Create(type, parent);
    if (!object) { object = ExtendedWidgets::Create(type); if (object) lv_obj_set_parent(object, parent); }
    if (!object)
    {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
        if (type == "arc") object = lv_arc_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
        if (type == "bar") object = lv_bar_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
        if (type == "button") object = lv_button_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
        if (type == "buttonMatrix") object = lv_buttonmatrix_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
        if (type == "calendar") { object = lv_calendar_create(parent); lv_calendar_add_header_arrow(object); }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
        if (type == "canvas") object = lv_canvas_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
        if (type == "checkbox") object = lv_checkbox_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
        if (type == "image") object = lv_image_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
        if (type == "label") object = lv_label_create(parent);
#endif
        if (type == "object") object = lv_obj_create(parent);
    }
    if (object && !lv_obj_get_user_data(object)) LVObject::FromJSON(json::object(), object);
    return object;
#else
    return nullptr;
#endif
}
