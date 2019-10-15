#include "ObjectSerializer.h"
using namespace Serialization;

json ObjectSerializer::SerializeObject(lv_obj_t* obj)
{
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
}
