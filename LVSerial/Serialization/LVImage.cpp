#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
#include "LVImage.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "ImageRecords.h"
#include "ImageAsset.h"
#include "Style.h"

namespace Serialization
{
    json LVImage::ToJSON(lv_obj_t* image)
    {
#ifdef LV_RUNTIME_PORTABLE
            auto* saved = static_cast<ObjectUserData*>(lv_obj_get_user_data(image));
            return saved ? saved->objectJson : json::object();
#else
        auto record = ReadSection(image, "img");
        const auto* source = lv_image_get_src(image);
        const auto type = lv_image_src_get_type(source);
        switch (type)
        {
        case LV_IMAGE_SRC_VARIABLE:
            record["srcType"] = 0;
            if (!record.contains("asset")) SavePixels(record, *static_cast<const lv_image_dsc_t*>(source), "w", "h");
            else
            {
                const auto* descriptor = static_cast<const lv_image_dsc_t*>(source);
                record["formatVersion"] = 9;
                record["cf"] = static_cast<uint32_t>(descriptor->header.cf);
                record["w"] = static_cast<uint32_t>(descriptor->header.w);
                record["h"] = static_cast<uint32_t>(descriptor->header.h);
                record["stride"] = static_cast<uint32_t>(descriptor->header.stride);
            }
            if (static_cast<const lv_image_dsc_t*>(source)->header.cf == LV_COLOR_FORMAT_RAW ||
                static_cast<const lv_image_dsc_t*>(source)->header.cf == LV_COLOR_FORMAT_RAW_ALPHA)
                record["encodedSource"] = SaveImageSourceRecord(source);
            break;
        case LV_IMAGE_SRC_FILE:
            record["srcType"] = 1;
            record["fileName"] = static_cast<const char*>(source);
            break;
        case LV_IMAGE_SRC_SYMBOL:
            record["srcType"] = 2;
            record["sym"] = static_cast<const char*>(source);
            break;
        default:
            break;
        }
        if (record.contains("asset")) record.erase("data");
        record["offX"] = lv_image_get_offset_x(image);
        record["offY"] = lv_image_get_offset_y(image);
        record["rotation"] = lv_image_get_rotation(image);
        record["scaleX"] = lv_image_get_scale_x(image);
        record["scaleY"] = lv_image_get_scale_y(image);
        lv_point_t pivot;
        lv_image_get_pivot(image, &pivot);
        record["pivot"] = {{"x", pivot.x}, {"y", pivot.y}};
        record["antialias"] = lv_image_get_antialias(image);
        record["blendMode"] = lv_image_get_blend_mode(image);
        record["innerAlign"] = lv_image_get_inner_align(image);
        record["style"] = Style::Read(image, LV_PART_MAIN, record["style"]);
        return {{"base", LVObject::ToJSON(image)}, {"img", record}};
    #endif
        }

    lv_obj_t* LVImage::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* image = LVObject::FromJSON(j["base"], lv_image_create(lv_screen_active()));
        auto record = j["img"];
        if (!record.is_object())
            return image;
        PreserveSection(image, "img", record);
        if (record.contains("asset")) ImageAsset::Restore(image, record["asset"]);
        else switch (record.value("srcType", -1))
        {
        case 0:
            if (record.contains("encodedSource"))
            {
                lv_image_set_src(image, RestoreImageSourceRecord(record["encodedSource"], WidgetData<SerializedImageSource>(image)));
                break;
            }
            if (auto* pixels = RestorePixels(record, "w", "h"))
            {
                WidgetData<SerializedBuffer>(image).buffer = pixels;
                lv_image_set_src(image, pixels);
            }
            break;
        case 1:
            if (record["fileName"].is_string())
                lv_image_set_src(image, record["fileName"].get<std::string>().c_str());
            break;
        case 2:
            if (record["sym"].is_string())
                lv_image_set_src(image, record["sym"].get<std::string>().c_str());
            break;
        }
        SetValue(image, "/img/offX", record.value("offX", 0));
        SetValue(image, "/img/offY", record.value("offY", 0));
        lv_image_set_rotation(image, record.value("rotation", 0));
        lv_image_set_scale_x(image, record.value("scaleX", 256));
        lv_image_set_scale_y(image, record.value("scaleY", 256));
        if (record.contains("pivot") && record["pivot"].is_object())
            lv_image_set_pivot(image, record["pivot"].value("x", 0), record["pivot"].value("y", 0));
        lv_image_set_antialias(image, record.value("antialias", true));
        lv_image_set_blend_mode(image, static_cast<lv_blend_mode_t>(record.value("blendMode", 0)));
        lv_image_set_inner_align(image, static_cast<lv_image_align_t>(record.value("innerAlign", 0)));
        if (!HasNativeStyleSnapshot(j["base"])) Style::Apply(image, record["style"], LV_PART_MAIN);
        return image;
    }

    bool LVImage::SetValue(lv_obj_t* image, std::string key, int value)
    {
        if (key == "/img/offX")
            lv_image_set_offset_x(image, value);
        else if (key == "/img/offY")
            lv_image_set_offset_y(image, value);
        else if (key == "/img/rotation") lv_image_set_rotation(image, value);
        else if (key == "/img/scaleX") lv_image_set_scale_x(image, value);
        else if (key == "/img/scaleY") lv_image_set_scale_y(image, value);
        else if (key == "/img/antialias") lv_image_set_antialias(image, value != 0);
        else if (key == "/img/blendMode") lv_image_set_blend_mode(image, static_cast<lv_blend_mode_t>(value));
        else if (key == "/img/innerAlign") lv_image_set_inner_align(image, static_cast<lv_image_align_t>(value));
        else if (key == "/img/pivot/x" || key == "/img/pivot/y")
        {
            lv_point_t pivot;
            lv_image_get_pivot(image, &pivot);
            lv_image_set_pivot(image, key == "/img/pivot/x" ? value : pivot.x, key == "/img/pivot/y" ? value : pivot.y);
        }
        else
            return false;
        return true;
    }
}
#else

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
#endif

#endif // selected widget
