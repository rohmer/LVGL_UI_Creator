#include "LVImage.h"

namespace Serialization
{
    json LVImage::ToJSON(lv_obj_t* img)
    {
        json j;
        lv_img_header_t header;
        char* src = (char*)lv_img_get_src(img);
        lv_img_decoder_get_info(src, &header);

        j["base"] = LVObject::ToJSON(img);
        j["img"]["srcType"] = lv_img_src_get_type(img);
        j["img"]["autoSz"] = lv_img_get_auto_size(img);
        j["img"]["offX"] = lv_img_get_offset_x(img);
        j["img"]["offY"] = lv_img_get_offset_y(img);
        lv_img_ext_t* ext = static_cast<lv_img_ext_t*>(lv_obj_get_ext_attr(img));
        j["img"]["cf"] = static_cast<uint8_t>(header.cf);
        j["img"]["h"] = static_cast<uint32_t>(header.h);
        j["img"]["w"] = static_cast<uint32_t>(header.w);
        lv_style_t* st = (lv_style_t*)lv_img_get_style(img, LV_IMG_STYLE_MAIN);
        j["img"]["style"] = Style::ToJSON(*st);
        switch (lv_img_src_get_type(img))
        {
        case LV_IMG_SRC_FILE:
            {
                j["img"]["fileName"] = lv_img_get_file_name(img);
                break;
            }
        case LV_IMG_SRC_VARIABLE:
            {
                lv_img_dsc_t* dsc = (lv_img_dsc_t*)src;

                std::string enc = base64_encode(dsc->data, dsc->data_size);
                j["img"]["data"] = enc;
                break;
            }
        case LV_IMG_SRC_SYMBOL:
            j["img"]["sym"] = src;
            break;
        }
        return j;
    }

    lv_obj_t* LVImage::FromJSON(json j)
    {
        lv_obj_t* img = lv_img_create(lv_scr_act(), nullptr);
        if (j["base"].is_object())
        {
            img = LVObject::FromJSON(j["base"], img);
        }
        auto* ext = static_cast<lv_img_ext_t*>(lv_obj_get_ext_attr(img));
        if (j["img"]["srcType"].is_number())
            ext->src_type = j["img"]["srcType"];
        if (j["img"]["autoSz"].is_number())
            ext->auto_size = j["img"]["autoSz"];
        if (j["img"]["offX"].is_number())
            ext->offset.x = j["img"]["offX"];
        if (j["img"]["offY"].is_number())
            ext->offset.y = j["img"]["offY"];
        if (j["img"]["cf"].is_number())
            ext->cf = j["img"]["cf"];
        if (j["img"]["h"].is_number())
            ext->h = j["img"]["h"];
        if (j["img"]["w"].is_number())
            ext->w = j["img"]["w"];
        switch (ext->src_type)
        {
        case LV_IMG_SRC_FILE:
            {
                if (j["img"]["fileName"].is_string())
                    lv_img_set_src(img, j["img"]["fileName"].get<std::string>().c_str());
                break;
            }
        case LV_IMG_SRC_SYMBOL:
            if (j["img"]["sym"].is_string())
                lv_img_set_src(img, j["img"]["sym"].get<std::string>().c_str());
            break;
        case LV_IMG_SRC_VARIABLE:
            if (j["img"]["data"].is_string())
            {
                lv_img_set_src(img, base64_decode(j["img"]["data"]).c_str());
            }
        }

        return img;
    }
}
