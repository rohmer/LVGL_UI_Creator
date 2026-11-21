#include "Style.h"
#include <deque>

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
namespace Serialization
{
    namespace
    {
        struct Property
        {
            const char* path;
            lv_style_prop_t property;
            bool color;
        };

        constexpr Property properties[] = {
            {"/body/main_color", LV_STYLE_BG_COLOR, true},
            {"/body/gradColor", LV_STYLE_BG_GRAD_COLOR, true},
            {"/body/opa", LV_STYLE_BG_OPA, false},
            {"/body/radius", LV_STYLE_RADIUS, false},
            {"/body/border/color", LV_STYLE_BORDER_COLOR, true},
            {"/body/border/opa", LV_STYLE_BORDER_OPA, false},
            {"/body/border/width", LV_STYLE_BORDER_WIDTH, false},
            {"/body/border/part", LV_STYLE_BORDER_SIDE, false},
            {"/body/padding/left", LV_STYLE_PAD_LEFT, false},
            {"/body/padding/right", LV_STYLE_PAD_RIGHT, false},
            {"/body/padding/top", LV_STYLE_PAD_TOP, false},
            {"/body/padding/bottom", LV_STYLE_PAD_BOTTOM, false},
            {"/body/padding/inner", LV_STYLE_PAD_ROW, false},
            {"/body/shadow/color", LV_STYLE_SHADOW_COLOR, true},
            {"/body/shadow/width", LV_STYLE_SHADOW_WIDTH, false},
            {"/image/color", LV_STYLE_IMAGE_RECOLOR, true},
            {"/image/intense", LV_STYLE_IMAGE_RECOLOR_OPA, false},
            {"/image/opa", LV_STYLE_IMAGE_OPA, false},
            {"/line/color", LV_STYLE_LINE_COLOR, true},
            {"/line/opa", LV_STYLE_LINE_OPA, false},
            {"/line/rounded", LV_STYLE_LINE_ROUNDED, false},
            {"/line/width", LV_STYLE_LINE_WIDTH, false},
            {"/text/color", LV_STYLE_TEXT_COLOR, true},
            {"/text/opa", LV_STYLE_TEXT_OPA, false},
            {"/text/letter_space", LV_STYLE_TEXT_LETTER_SPACE, false},
            {"/text/line_space", LV_STYLE_TEXT_LINE_SPACE, false}
        };

        template <typename Setter>
        void ReadProperties(const json& record, Setter set)
        {
            for (const auto& property : properties)
            {
                const json::json_pointer path(property.path);
                if (!record.contains(path))
                    continue;
                const auto& entry = record.at(path);
                if (!entry.is_number_integer())
                    continue;
                lv_style_value_t value{};
                if (property.color)
                    value.color = lv_color_hex(entry.get<uint32_t>());
                else
                    value.num = entry.get<int32_t>();
                set(property.property, value);
                if (property.property == LV_STYLE_PAD_ROW)
                    set(LV_STYLE_PAD_COLUMN, value);
                if (property.property == LV_STYLE_BG_GRAD_COLOR)
                {
                    value.num = LV_GRAD_DIR_VER;
                    set(LV_STYLE_BG_GRAD_DIR, value);
                }
            }
        }

        void WriteProperty(json& record, const Property& property, lv_style_value_t value)
        {
            auto path = json::json_pointer(property.path);
            if (property.color)
                record[path] = lv_color_to_u32(value.color);
            else
                record[path] = value.num;
        }
    }

    json Style::ToJSON(lv_style_t& style) { return ToJSON(style, ""); }

    json Style::ToJSON(lv_style_t& style, std::string name)
    {
        json result = {{"name", std::move(name)}};
        for (const auto& property : properties)
        {
            lv_style_value_t value{};
            if (lv_style_get_prop(&style, property.property, &value) == LV_STYLE_RES_FOUND)
                WriteProperty(result, property, value);
        }
        return result;
    }

    const lv_style_t& Style::FromJSON(json record)
    {
        // Callers keep references to styles selected in the editor for the lifetime of the document.
        static std::deque<lv_style_t> styles;
        auto& style = styles.emplace_back();
        lv_style_init(&style);
        ReadProperties(record, [&style](lv_style_prop_t property, lv_style_value_t value) {
            lv_style_set_prop(&style, property, value);
        });
        return style;
    }

    bool Style::StyleComp(lv_style_t first, lv_style_t second)
    {
        return ToJSON(first) == ToJSON(second);
    }

    void Style::Apply(lv_obj_t* object, const json& record, lv_style_selector_t selector)
    {
        ReadProperties(record, [object, selector](lv_style_prop_t property, lv_style_value_t value) {
            lv_obj_set_local_style_prop(object, property, value, selector);
        });
    }

    json Style::Read(lv_obj_t* object, lv_part_t part, json record)
    {
        if (!record.is_object())
            record = json::object();
        if (!record.contains("name"))
            record["name"] = "";
        for (const auto& property : properties)
            WriteProperty(record, property, lv_obj_get_style_prop(object, part, property.property));
        return record;
    }
}
#else

namespace Serialization
{
    json Style::ToJSON(lv_style_t& style)
    {
        std::string name = "";
        if (StyleComp(style, lv_style_btn_pr))
        {
            name = "lv_style_btn_pr";
        }
        if (StyleComp(style, lv_style_scr))
        {
            name = "lv_style_scr";
        }
        if (StyleComp(style, lv_style_transp))
        {
            name = "lv_style_transp";
        }
        if (StyleComp(style, lv_style_transp_fit))
        {
            name = "lv_style_transp_fit";
        }
        if (StyleComp(style, lv_style_transp_tight))
        {
            name = "lv_style_transp_tight";
        }
        if (StyleComp(style, lv_style_plain))
        {
            name = "lv_style_plain";
        }
        if (StyleComp(style, lv_style_plain_color))
        {
            name = "lv_style_plain_color";
        }
        if (StyleComp(style, lv_style_pretty_color))
        {
            name = "lv_style_pretty_color";
        }
        if (StyleComp(style, lv_style_pretty))
        {
            name = "lv_style_pretty";
        }
        if (StyleComp(style, lv_style_btn_ina))
        {
            name = "lv_style_btn_ina";
        }
        if (StyleComp(style, lv_style_btn_pr))
        {
            name = "lv_style_btn_pr";
        }
        if (StyleComp(style, lv_style_btn_rel))
        {
            name = "lv_style_btn_rel";
        }
        if (StyleComp(style, lv_style_btn_tgl_pr))
        {
            name = "lv_style_btn_tgl_pr";
        }
        if (StyleComp(style, lv_style_btn_tgl_rel))
        {
            name = "lv_style_btn_tgl_rel";
        }
        // if name is still blank, we dont know what it is.  Set a random name
        if (name == "")
        {
            std::stringstream ss;
            srand(time(nullptr));
            ss << "Style_" << (rand() % 65536);
            name = ss.str();
        }
        return ToJSON(style, name);
    }

    json Style::ToJSON(lv_style_t& style, std::string name)
    {
        json j;

        j["name"] = name;
        j["glass"] = static_cast<uint8_t>(style.glass);
        j["body"]["border"]["color"] = style.body.border.color.full;
        j["body"]["border"]["opa"] = style.body.border.opa;
        j["body"]["border"]["part"] = style.body.border.part;
        j["body"]["border"]["width"] = style.body.border.width;
        j["body"]["gradColor"] = style.body.grad_color.full;
        j["body"]["main_color"] = style.body.main_color.full;
        j["body"]["opa"] = style.body.opa;
        j["body"]["padding"]["bottom"] = style.body.padding.bottom;
        j["body"]["padding"]["inner"] = style.body.padding.inner;
        j["body"]["padding"]["left"] = style.body.padding.left;
        j["body"]["padding"]["right"] = style.body.padding.right;
        j["body"]["padding"]["top"] = style.body.padding.top;
        j["body"]["radius"] = style.body.radius;
        j["body"]["shadow"]["color"] = style.body.shadow.color.full;
        j["body"]["shadow"]["type"] = style.body.shadow.type;
        j["body"]["shadow"]["width"] = style.body.shadow.width;
        j["image"]["color"] = style.image.color.full;
        j["image"]["intense"] = style.image.intense;
        j["image"]["opa"] = style.image.opa;
        j["line"]["color"] = style.line.color.full;
        j["line"]["opa"] = style.line.opa;
        j["line"]["rounded"] = static_cast<uint8_t>(style.line.rounded);
        j["line"]["width"] = style.line.width;
        j["text"]["color"] = style.text.color.full;
        //TODO: Deal with fonts correctly
        const lv_font_t font = *style.text.font;
        j["text"]["font"] = LVFont::ToJSON(font);
        j["text"]["letter_space"] = style.text.letter_space;
        j["text"]["line_space"] = style.text.line_space;
        j["text"]["opa"] = style.text.opa;
        j["text"]["sel_color"] = style.text.sel_color.full;

        return j;
    }

    const lv_style_t &Style::FromJSON(json j)
    {
        static std::deque<lv_style_t> restoredStyles;
        auto& style = restoredStyles.emplace_back(lv_style_plain);
        if (j["glass"].is_number())
            style.glass = j["glass"];
        if (j["body"]["border"]["color"].is_number())
            style.body.border.color.full = j["body"]["border"]["color"];
        if (j["body"]["border"]["opa"].is_number())
            style.body.border.opa = j["body"]["border"]["opa"];
        if (j["body"]["border"]["part"].is_number())
            style.body.border.part = j["body"]["border"]["part"];
        if (j["body"]["border"]["width"].is_number())
            style.body.border.width = j["body"]["border"]["width"];
        if (j["body"]["gradColor"].is_number())
            style.body.grad_color.full = j["body"]["gradColor"];
        if (j["body"]["main_color"].is_number())
            style.body.main_color.full = j["body"]["main_color"];
        if (j["body"]["opa"].is_number())
            style.body.opa = j["body"]["opa"];
        if (j["body"]["padding"]["bottom"].is_number())
            style.body.padding.bottom = j["body"]["padding"]["bottom"];
        if (j["body"]["padding"]["inner"].is_number())
            style.body.padding.inner = j["body"]["padding"]["inner"];
        if (j["body"]["padding"]["left"].is_number())
            style.body.padding.left = j["body"]["padding"]["left"];
        if (j["body"]["padding"]["right"].is_number())
            style.body.padding.right = j["body"]["padding"]["right"];
        if (j["body"]["padding"]["top"].is_number())
            style.body.padding.top = j["body"]["padding"]["top"];
        if (j["body"]["radius"].is_number())
            style.body.radius = j["body"]["radius"];
        if (j["body"]["shadow"]["color"].is_number())
            style.body.shadow.color.full = j["body"]["shadow"]["color"];
        if (j["body"]["shadow"]["type"].is_number())
            style.body.shadow.type = j["body"]["shadow"]["type"];
        if (j["body"]["shadow"]["width"].is_number())
            style.body.shadow.width = j["body"]["shadow"]["width"];
        if (j["image"]["color"].is_number())
            style.image.color.full = j["image"]["color"];
        if (j["image"]["intense"].is_number())
            style.image.intense = j["image"]["intense"];
        if (j["image"]["opa"].is_number())
            style.image.opa = j["image"]["opa"];
        if (j["line"]["color"].is_number())
            style.line.color.full = j["line"]["color"];
        if (j["line"]["opa"].is_number())
            style.line.opa = j["line"]["opa"];
        if (j["line"]["rounded"].is_number())
            style.line.rounded = j["line"]["rounded"];
        if (j["line"]["width"].is_number())
            style.line.width = j["line"]["width"];
        if (j["text"]["color"].is_number())
            style.text.color.full = j["text"]["color"];
        //TODO: Deal with fonts correctly
        //style.text.font=const_cast<lv_font_t*>(&LVFont::FromJSON(j["text"]["font"]));
        if (j["text"]["letter_space"].is_number())
            style.text.letter_space = j["text"]["letter_space"];
        if (j["text"]["line_space"].is_number())
            style.text.line_space = j["text"]["line_space"];
        if (j["text"]["opa"].is_number())
            style.text.opa = j["text"]["opa"];
        if (j["text"]["sel_color"].is_number())
            style.text.sel_color.full = j["text"]["sel_color"];

        return style;
    }

    bool Style::StyleComp(lv_style_t st1, lv_style_t st2)
    {
        if ((st1.body.border.color.full == st2.body.border.color.full)
            && (st1.body.border.opa == st2.body.border.opa)
            && (st1.body.border.part == st2.body.border.part)
            && (st1.body.border.width == st2.body.border.width)
            && (st1.body.grad_color.full == st2.body.grad_color.full)
            && (st1.body.main_color.full == st2.body.main_color.full)
            && (st1.body.opa == st2.body.opa)
            && (st1.body.padding.bottom == st2.body.padding.bottom)
            && (st1.body.padding.left == st2.body.padding.left)
            && (st1.body.padding.right == st2.body.padding.right)
            && (st1.body.padding.top == st2.body.padding.top)
            && (st1.body.radius == st2.body.radius)
            && (st1.body.shadow.color.full && st2.body.shadow.color.full)
            && (st1.body.shadow.type == st2.body.shadow.type)
            && (st1.body.shadow.width == st2.body.shadow.width)
            && (st1.image.color.full == st2.image.color.full)
            && (st1.image.intense == st2.image.intense)
            && (st1.image.opa == st2.image.opa)
            && (st1.line.color.full == st2.line.color.full)
            && (st1.line.opa == st2.line.opa)
            && (st1.line.rounded == st2.line.rounded)
            && (st1.line.width == st2.line.width)
            && (st1.text.color.full == st2.text.color.full)
            && (st1.text.letter_space == st2.text.letter_space)
            && (st1.text.line_space == st2.text.line_space)
            && (st1.text.opa == st2.text.opa)
            && (st1.text.sel_color.full == st2.text.sel_color.full))
            return true;

        return false;
    }
}
#endif
