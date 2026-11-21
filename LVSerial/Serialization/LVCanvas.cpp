#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
#include "LVCanvas.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "ImageRecords.h"
#include "Style.h"
#include <stdexcept>
#include <algorithm>

namespace Serialization
{
    json LVCanvas::ToJSON(lv_obj_t* canvas)
    {
#ifdef LV_RUNTIME_PORTABLE
            auto* saved = static_cast<ObjectUserData*>(lv_obj_get_user_data(canvas));
            return saved ? saved->objectJson : json::object();
#else
        auto record = ReadSection(canvas, "canvas");
        if (auto* buffer = lv_canvas_get_draw_buf(canvas))
        {
            SavePixels(record, *lv_canvas_get_image(canvas), "header.w", "header.h");
            record["img.w"] = static_cast<uint32_t>(buffer->header.w);
            record["img.h"] = static_cast<uint32_t>(buffer->header.h);
            record["width"] = static_cast<uint32_t>(buffer->header.w);
            record["height"] = static_cast<uint32_t>(buffer->header.h);
            record["format"] = static_cast<uint32_t>(buffer->header.cf);
        }
        record["offset"] = {{"x", lv_image_get_offset_x(canvas)}, {"y", lv_image_get_offset_y(canvas)}};
        return {{"base", LVObject::ToJSON(canvas)}, {"canvas", record}};
    #endif
        }

    lv_obj_t* LVCanvas::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* canvas = LVObject::FromJSON(j["base"], lv_canvas_create(lv_screen_active()));
        auto record = j["canvas"];
        if (!record.is_object())
            return canvas;
        if (!record.contains("data") && !record.contains("width") && !record.contains("header.w"))
        {
            const auto offset = record.value("offset", json::object());
            lv_image_set_offset_x(canvas, offset.value("x", 0)); lv_image_set_offset_y(canvas, offset.value("y", 0));
            PreserveSection(canvas, "canvas", record);
        }
        else Apply(canvas, record);
        return canvas;
    }

#if LVGL_VERSION_MAJOR == 8
    void LVCanvas::Apply(lv_obj_t* canvas, const json& record) {
        auto& owned=WidgetData<SerializedBuffer>(canvas);
        if(owned.buffer)throw std::invalid_argument("Replacing a live canvas is unsupported");
        auto pixels=record;
        auto w=record.value("width",record.value("header.w",96)),h=record.value("height",record.value("header.h",64));
        if(w<=0 || h<=0 || w>2047 || h>2047)throw std::invalid_argument("Invalid LVGL 8 canvas dimensions");
        pixels["header.w"]=w;pixels["header.h"]=h;
        if(!pixels.contains("data")){std::string blank(size_t(w)*h*4,0);pixels["data"]=base64_encode((const unsigned char*)blank.data(),blank.size());}
        owned.buffer=RestorePixels(pixels,"header.w","header.h");
        lv_canvas_set_draw_buf(canvas,owned.buffer);
        if(record.contains("fillColor") && !record.contains("data"))lv_canvas_fill_bg(canvas,lv_color_hex(record.value("fillColor",0u)),record.value("fillOpacity",255));
        PreserveSection(canvas,"canvas",record);
    }
#else
    void LVCanvas::Apply(lv_obj_t* canvas, const json& record)
    {
        const auto previous = ReadSection(canvas, "canvas");
        auto* current = lv_canvas_get_draw_buf(canvas);
        auto* replacement = static_cast<lv_draw_buf_t*>(nullptr);
        const bool pixelsChanged = record.contains("data") && (!previous.contains("data") || record["data"] != previous["data"]);
        const auto width = record.value("width", record.value("header.w", current ? int(current->header.w) : 96));
        const auto height = record.value("height", record.value("header.h", current ? int(current->header.h) : 64));
        const auto legacyFormat = ReadImageFormat(record);
        const auto format = static_cast<lv_color_format_t>(record.value("format", current ? int(current->header.cf) :
            int(legacyFormat == LV_COLOR_FORMAT_UNKNOWN ? LV_COLOR_FORMAT_ARGB8888 : legacyFormat)));
        if (width <= 0 || height <= 0) throw std::invalid_argument("Canvas dimensions must be positive");
        if (pixelsChanged) replacement = RestorePixels(record, "header.w", "header.h");
        if (pixelsChanged && !replacement) throw std::invalid_argument("Invalid canvas pixel data");
        if (!replacement && (!current || width != current->header.w || height != current->header.h || format != current->header.cf))
        {
            replacement = lv_draw_buf_create(width, height, format, 0);
            if (!replacement) throw std::runtime_error("Could not allocate the canvas buffer");
            lv_draw_buf_clear(replacement, nullptr);
            if (current && current->header.cf == format)
            {
                const lv_area_t area{0, 0, std::min(width, int(current->header.w)) - 1, std::min(height, int(current->header.h)) - 1};
                lv_draw_buf_copy(replacement, &area, current, &area);
            }
        }
        if (replacement)
        {
            auto& owned = WidgetData<SerializedBuffer>(canvas);
            auto* old = owned.buffer;
            lv_canvas_set_draw_buf(canvas, replacement);
            owned.buffer = replacement;
            if (old) lv_draw_buf_destroy(old);
        }
        if (record.contains("fillColor") && (!record.contains("data") ||
            (!pixelsChanged && (record.value("fillColor", 0u) != previous.value("fillColor", 0u) ||
                record.value("fillOpacity", 255) != previous.value("fillOpacity", 255)))))
            lv_canvas_fill_bg(canvas, lv_color_hex(record.value("fillColor", 0u)), record.value("fillOpacity", 255));
        const auto offset = record.value("offset", json::object());
        SetValue(canvas, "/canvas/offset/x", offset.value("x", 0));
        SetValue(canvas, "/canvas/offset/y", offset.value("y", 0));
        PreserveSection(canvas, "canvas", record);
        lv_obj_invalidate(canvas);
    }

#endif
    bool LVCanvas::SetValue(lv_obj_t* canvas, std::string key, int value)
    {
        if (key == "/canvas/offset/x")
            lv_image_set_offset_x(canvas, value);
        else if (key == "/canvas/offset/y")
            lv_image_set_offset_y(canvas, value);
        else
            return false;
        return true;
    }

    bool LVCanvas::SetValue(lv_obj_t* canvas, std::string key, lv_style_t style)
    {
        if (key != "/canvas/style")
            return false;
        Style::Apply(canvas, Style::ToJSON(style), LV_PART_MAIN);
        return true;
    }

    bool LVCanvas::SetValue(lv_obj_t* canvas, uint32_t x, uint32_t y, uint32_t format,
                            std::vector<unsigned char> bytes, lv_style_t style)
    {
#if LVGL_VERSION_MAJOR == 8
        (void)canvas;(void)x;(void)y;(void)format;(void)bytes;(void)style;return false;
#else

        auto* destination = lv_canvas_get_draw_buf(canvas);
        if (!destination || x >= destination->header.w || y >= destination->header.h || bytes.empty())
            return false;
        auto record = json{{"cf", format}, {"w", destination->header.w - x},
                           {"h", destination->header.h - y}, {"data", base64_encode(bytes.data(), bytes.size())}};
        auto* source = RestorePixels(record, "w", "h");
        if (!source)
            return false;
        lv_layer_t layer;
        lv_canvas_init_layer(canvas, &layer);
        lv_draw_image_dsc_t descriptor;
        lv_draw_image_dsc_init(&descriptor);
        descriptor.src = source;
        const auto selectedStyle = Style::ToJSON(style);
        const auto imageStyle = selectedStyle.value("image", json::object());
        descriptor.opa = imageStyle.value("opa", static_cast<int>(LV_OPA_COVER));
        lv_area_t area{static_cast<int32_t>(x), static_cast<int32_t>(y),
                       destination->header.w - 1, destination->header.h - 1};
        lv_draw_image(&layer, &descriptor, &area);
        lv_canvas_finish_layer(canvas, &layer);
        lv_draw_buf_destroy(source);
        return true;
#endif
    }
}
#else

namespace Serialization
{
    json LVCanvas::ToJSON(lv_obj_t* canvas)
    {
        json j;
        j["base"] = LVObject::ToJSON(canvas);
        lv_canvas_ext_t* ext = static_cast<lv_canvas_ext_t*>(lv_obj_get_ext_attr(canvas));
        j["canvas"]["cf"] = static_cast<uint32_t>(ext->dsc.header.cf);
        j["canvas"]["header.w"] = static_cast<uint16_t>(ext->dsc.header.w);
        j["canvas"]["header.h"] = static_cast<uint16_t>(ext->dsc.header.h);
        j["canvas"]["dataSize"] = static_cast<uint32_t>(ext->dsc.data_size);
        j["canvas"]["data"] = base64_encode(ext->dsc.data, ext->dsc.data_size);
        j["canvas"]["autoSize"] = static_cast<uint8_t>(ext->img.auto_size);
        j["canvas"]["offset"]["x"] = ext->img.offset.x;
        j["canvas"]["offset"]["y"] = ext->img.offset.y;
        j["canvas"]["img.h"] = ext->img.h;
        j["canvas"]["img.w"] = ext->img.w;
        j["canvas"]["srcType"] = static_cast<uint8_t>(ext->img.src_type);
        if (lv_img_src_get_type(ext->img.src) == LV_IMG_SRC_VARIABLE)
        {
            auto buf = new unsigned char(4);
            std::stringstream ss;
            for (int y = 0; y < ext->img.h; y++)
            {
                for (int x = 0; x < ext->img.w; x++)
                {
                    lv_color_t col = lv_img_buf_get_px_color(&ext->dsc, x, y, nullptr);
                    buf[0] = col.ch.red;
                    buf[1] = col.ch.green;
                    buf[2] = col.ch.blue;
                    buf[3] = col.ch.alpha;
                    ss << base64_encode(const_cast<unsigned char*>(buf), 4);
                }
            }
            j["canvas"]["img"] = ss.str();
        }
        else
        {
            std::stringstream ss;
            char* p = (char*)ext->img.src;
            while (p != nullptr)
            {
                ss << p;
            }
            j["canvas"]["img"] = ss.str();
        }
        static const lv_style_t* style = lv_canvas_get_style(canvas, LV_CANVAS_STYLE_MAIN);
        j["canvas"]["img.color"]["r"] = style->image.color.ch.red;
        j["canvas"]["img.color"]["g"] = style->image.color.ch.green;
        j["canvas"]["img.color"]["b"] = style->image.color.ch.blue;
        j["canvas"]["img.color"]["a"] = style->image.color.ch.alpha;
        return j;
    }

    lv_obj_t* LVCanvas::FromJSON(json j)
    {
        lv_obj_t* canvas = lv_canvas_create(lv_scr_act(), nullptr);
        if (j["base"].is_object())
        {
            canvas = LVObject::FromJSON(j["base"], canvas);
        }
        if (j["canvas"].is_object())
            return canvas;
        int cf = -1;
        if (j["canvas"]["cf"].is_number())
        {
            cf = j["canvas"]["cf"];
        }
        int w = -1, h = -1;
        if (j["canvas"]["img.h"].is_number())
        {
            h = j["canvas"]["img.h"];
        }
        if (j["canvas"]["img.w"].is_number())
        {
            w = j["canvas"]["img.w"];
        }
        int srcType = -1;
        if (j["canvas"]["srcType"].is_number())
        {
            srcType = j["canvas"]["srcType"];
        }

        if (srcType == LV_IMG_SRC_VARIABLE && cf != -1 && w != -1 && h != -1 && j["canvas"]["img"].is_string())
        {
            std::string val = base64_decode(j["canvas"]["img"]);
            lv_canvas_draw_img(canvas, 0, 0, (void*)val.c_str(), nullptr);
        }
        else
        {
            lv_canvas_draw_img(canvas, 0, 0, j["canvas"]["img"].get<std::string>().c_str(), nullptr);
        }
        uint16_t offX = -1, offY = -1;
        if (j["canvas"]["offset"].is_object())
        {
            if (j["canvas"]["offset"]["x"].is_number())
                offX = j["canvas"]["offset"]["x"];
            if (j["canvas"]["offset"]["y"].is_number())
                offY = j["canvas"]["offset"]["y"];
        }
        lv_canvas_ext_t* ext = static_cast<lv_canvas_ext_t*>(lv_obj_get_ext_attr(canvas));
        if (offX != -1)
        {
            ext->img.offset.x = offX;
        }
        if (offY != -1)
        {
            ext->img.offset.y = offY;
        }
        if (j["canvas"]["img.color"].is_object())
        {
            static lv_style_t style;
            lv_style_copy(&style, lv_canvas_get_style(canvas, LV_CANVAS_STYLE_MAIN));
            lv_color_t color;
            int r, g, b, a;
            r = g = b = 0;
            a = 255;
            if (j["canvas"]["img.color"]["r"].is_number())
                r = j["canvas"]["img.color"]["r"];
            if (j["canvas"]["img.color"]["r"].is_number())
                g = j["canvas"]["img.color"]["g"];
            if (j["canvas"]["img.color"]["r"].is_number())
                b = j["canvas"]["img.color"]["b"];
            if (j["canvas"]["img.color"]["r"].is_number())
                a = j["canvas"]["img.color"]["a"];
            color = lv_color_make(r, g, b);
            style.image.color = color;
            lv_canvas_set_style(canvas, LV_CANVAS_STYLE_MAIN, &style);
        }
        return (canvas);
    }

    bool LVCanvas::SetValue(lv_obj_t* obj, uint32_t x, uint32_t y, uint32_t cf, std::vector<unsigned char> img,
                            lv_style_t style)
    {
        lv_canvas_draw_img(obj, x, y, img.data(), &style);
        return true;
    }

    bool LVCanvas::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        return false;
    }
}
#endif

#endif // selected widget
