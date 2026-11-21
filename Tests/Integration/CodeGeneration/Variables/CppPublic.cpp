#include "ui.h"
#include <cstring>
#include <type_traits>

namespace Application
{
    static int32_t level = 73;
    struct State { float fraction = 0.75f; char caption[64] = "C++ member"; };
}

static_assert(std::is_same<decltype(&ui_bind_v_level), bool (*)(int32_t*)>::value, "Exact int32_t pointer binding");
static_assert(std::is_same<decltype(&ui_bind_v_fraction), bool (*)(float*)>::value, "Exact float pointer binding");
static_assert(std::is_same<decltype(&ui_bind_v_caption), bool (*)(char*, size_t)>::value, "Writable bounded string binding");

extern "C" bool check_cpp_variable_bindings(void)
{
    Application::State state;
    if (!ui_bind_v_level(&Application::level) || ui_variables.v_level != 73) return false;
    if (!ui_bind_v_fraction(&state.fraction) || ui_variables.v_fraction != 0.75) return false;
    if (!ui_bind_v_caption(state.caption, sizeof state.caption) || std::strcmp(ui_variables.v_caption, "C++ member")) return false;
    if (!ui_set_v_fraction(0.125) || state.fraction != 0.125f) return false;
    if (!ui_set_v_caption("Member update") || std::strcmp(state.caption, "Member update")) return false;
    if (!ui_set_v_level(74) || Application::level != 74) return false;
    ui_unbind_v_level(); ui_unbind_v_fraction(); ui_unbind_v_caption();
    Application::level = 73;
    return true;
}
