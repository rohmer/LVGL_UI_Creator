#pragma once
#include <string_view>

namespace CIdentifier {
// Exported symbols must also be usable from C++ firmware and LVGL headers.
constexpr bool Valid(std::string_view name) {
    auto letter = [](unsigned char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    if (name.empty() || !letter(static_cast<unsigned char>(name.front())) || name.substr(0, 2) == "__") return false;
    for (unsigned char c : name) if (!letter(c) && !(c >= '0' && c <= '9')) return false;
    constexpr std::string_view words =
        "alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t "
        "class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield "
        "decltype default delete do double dynamic_cast else enum explicit export extern false float for friend "
        "goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private "
        "protected public register reinterpret_cast requires restrict return short signed sizeof static static_assert "
        "static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned "
        "using virtual void volatile wchar_t while xor xor_eq _Alignas _Alignof _Atomic _Bool _Complex _Generic "
        "_Imaginary _Noreturn _Static_assert _Thread_local NULL";
    std::size_t start = 0;
    while (start < words.size()) {
        auto end = words.find(' ', start);
        if (end == std::string_view::npos) end = words.size();
        if (words.substr(start, end - start) == name) return false;
        start = end + 1;
    }
    return true;
}
}
