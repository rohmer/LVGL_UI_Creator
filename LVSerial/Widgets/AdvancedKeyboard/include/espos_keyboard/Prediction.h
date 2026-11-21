#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace espos::keyboard::prediction {
inline constexpr std::size_t kSuggestionCount = 3;
inline constexpr std::size_t kSuggestionBytes = 256;
struct Suggestion {
    std::array<char, kSuggestionBytes> text{};
    std::uint32_t mass = 0;
    std::uint16_t length = 0;
    std::string_view Word() const noexcept { return {text.data(), length}; }
};
}
