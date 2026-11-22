#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace espos::keyboard::prediction {
// The original ESPOS UTF-8 fold, isolated from any application predictor.
// canonicalIndic expands the eleven NFC-excluded Devanagari/Bengali Nukta
// letters used by those models. It is opt-in and is not a general NFC API.
bool FoldUtf8(std::string_view input, char* destination, std::size_t capacity, std::string_view& folded) noexcept;
bool FoldUtf8(std::string_view input, char* destination, std::size_t capacity,
              std::string_view& folded, bool canonicalIndic) noexcept;
// Unicode 14 categories match the corpus generator. These classifiers allocate
// no memory; marks and joiners are handled by the word-boundary caller.
bool IsWordScalar(std::uint32_t scalar) noexcept;
bool IsMarkScalar(std::uint32_t scalar) noexcept;
bool IsHanScalar(std::uint32_t scalar) noexcept;
bool IsCyrillicUpper(std::uint32_t scalar) noexcept;
bool IsCyrillicLower(std::uint32_t scalar) noexcept;
std::uint32_t UpperCyrillic(std::uint32_t scalar) noexcept;
}
