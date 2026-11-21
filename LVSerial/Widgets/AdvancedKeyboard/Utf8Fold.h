#pragma once
#include <cstddef>
#include <string_view>
namespace espos::keyboard::prediction {
// The original ESPOS UTF-8 fold, isolated from any application predictor.
bool FoldUtf8(std::string_view input, char* destination, std::size_t capacity, std::string_view& folded) noexcept;
}
