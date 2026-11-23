#include "espos_keyboard/LegacyTypingView.h"
#include "ngram_trie/esp_runtime.h"
#include "lv_advanced_keyboard_prediction.h"
namespace espos::keyboard {
namespace {
lv_obj_t* attach_partition(lv_obj_t* root)
{
    if (!root) return nullptr;
    auto& flash = ngram_trie::DefaultPredictor();
    if (!flash.Trie().IsOpen()) return root; // Compiled bundled model remains available.
    auto lease = flash.ModelOwner();
    auto* predictor = lv_advanced_keyboard_predictor_create_borrowed(
        flash.ModelData(), flash.ModelBytes(), flash.IndexData(), flash.IndexBytes());
    if (predictor && !set_typing_view_owned_predictor(root, predictor, std::move(lease)))
        lv_advanced_keyboard_predictor_destroy(predictor);
    return root;
}
}
lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, std::int32_t width, std::int32_t height, const KeyStyle& style)
{
    return attach_partition(create_en_us_typing_view(parent, width, height, nullptr, nullptr, style));
}
lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, const KeyStyle& style)
{
    return attach_partition(create_en_us_typing_view(parent, nullptr, nullptr, style));
}
}
