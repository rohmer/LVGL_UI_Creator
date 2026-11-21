#pragma once

#include <cstddef>
#include <cstdint>

namespace espos::keyboard::detail {

enum class ActionType : std::uint8_t { None, Insert, SwitchLayout, Shift, ShiftLock, Backspace, Enter };

inline constexpr std::size_t no_layout = static_cast<std::size_t>(-1);

struct Action {
    ActionType type;
    const char* text;
    std::size_t layout;
};

struct Key {
    const char* id;
    const char* label;
    float width;
    Action tap;
    Action double_tap;
};

struct Row {
    float offset;
    const Key* keys;
    std::size_t key_count;
};

struct Layout {
    const Row* rows;
    std::size_t row_count;
    std::size_t shifted;
};

struct LayoutDefinition {
    float width;
    const Layout* layouts;
    std::size_t layout_count;
    std::size_t initial_layout;
    std::size_t max_keys;
};

}  // namespace espos::keyboard::detail
