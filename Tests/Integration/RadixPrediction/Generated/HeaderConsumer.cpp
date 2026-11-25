// Consume only generated_ui's public headers, as an application would. The
// Arduino src/ forwarders cannot be used after the export flattens src/.
#include <espos_keyboard/TypingView.h>
#include <lv_advanced_keyboard.h>
#include <lv_advanced_keyboard_prediction.h>
#include <lv_textarea_input_mode.h>
#include <cstring>
#include <type_traits>

static_assert(std::is_same<decltype(&espos::keyboard::typing_view_editor),
              lv_obj_t* (*)(lv_obj_t*)>::value, "The exported public C++ view API must be usable");

extern "C" int radix_cpp_header_consumer(void)
{
    char normalized[16]{};
    return lv_advanced_keyboard_normalize_input(LV_ADVANCED_KEYBOARD_INPUT_NUMERIC,
        "1a2..3", normalized, sizeof normalized) == 4 && !std::strcmp(normalized, "12.3");
}
