#pragma once

#include <lv_textarea_input_mode.h>
#include <stdexcept>
#include <string>

namespace Serialization::TextInputMode
{
    inline lv_advanced_keyboard_input_mode_t Parse(const std::string& value)
    {
        if (value == "alphanumeric") return LV_ADVANCED_KEYBOARD_INPUT_ALPHANUMERIC;
        if (value == "uppercase") return LV_ADVANCED_KEYBOARD_INPUT_UPPERCASE;
        if (value == "lowercase") return LV_ADVANCED_KEYBOARD_INPUT_LOWERCASE;
        if (value == "numeric") return LV_ADVANCED_KEYBOARD_INPUT_NUMERIC;
        throw std::invalid_argument("Input mode must be alphanumeric, uppercase, lowercase or numeric");
    }

    inline const char* Name(lv_advanced_keyboard_input_mode_t mode)
    {
        switch (mode)
        {
            case LV_ADVANCED_KEYBOARD_INPUT_UPPERCASE: return "uppercase";
            case LV_ADVANCED_KEYBOARD_INPUT_LOWERCASE: return "lowercase";
            case LV_ADVANCED_KEYBOARD_INPUT_NUMERIC: return "numeric";
            default: return "alphanumeric";
        }
    }
}
