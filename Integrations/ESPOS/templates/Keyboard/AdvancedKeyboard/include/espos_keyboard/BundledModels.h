#pragma once
#include <cstddef>
#include <cstdint>
#include "lv_advanced_keyboard.h"

namespace espos::keyboard::bundled {
// Only profiles selected by ADVANCED_KEYBOARD_MODELS are linked into firmware.
// Byte arrays and metadata are immutable flash storage with process lifetime.
struct Model {
    const char* language;
    const char* name;
    std::uint32_t sizeKiB;
    std::size_t modelBytes;
    std::size_t indexBytes;
    const void* trie;
    const void* index;
};

std::size_t Count() noexcept;
const Model* At(std::size_t index) noexcept;
const Model* Find(const char* language, std::uint32_t sizeKiB) noexcept;

// LVGL-thread calls. Apply owns a separate inference workspace on this root;
// equal static model bytes share the normal ESP32 PSRAM copy when available.
// Unknown profiles, invalid roots and allocation failures return false. A
// same-language size change preserves an explicit RTL override. Selecting a
// bundle clears a custom lookup so the chosen predictor becomes active.
bool Apply(lv_obj_t* root, const char* language, std::uint32_t sizeKiB);
// Returns the owned model while its predictor is still attached. An explicit
// custom lookup can override it; legacy adapters track that mode separately.
const Model* Selected(lv_obj_t* root) noexcept;
// Disables both a custom lookup and a predictor, releasing owned workspace.
// A predictor supplied by the application remains application-owned.
bool Clear(lv_obj_t* root);
}
