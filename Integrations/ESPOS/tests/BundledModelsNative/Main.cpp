#include "espos_keyboard/BundledModels.h"
#include "lv_advanced_keyboard_prediction.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace bundle = espos::keyboard::bundled;
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Bundled models line %d: %s\n", __LINE__, #value); return false; } } while (false)
namespace {
void Flush(lv_display_t* display, const lv_area_t*, std::uint8_t*) { lv_display_flush_ready(display); }
void Deleting(lv_event_t* event) {
    auto* result = static_cast<bool*>(lv_event_get_user_data(event));
    *result = bundle::Selected(static_cast<lv_obj_t*>(lv_event_get_target(event))) == nullptr;
}
bool Exercise() {
    CHECK(bundle::Count() == 5);
    CHECK(!bundle::At(5));
    CHECK(!bundle::Find(nullptr, 16));
    CHECK(!bundle::Find("fr-FR", 16));
    CHECK(!bundle::Apply(nullptr, "en-US", 16));
    CHECK(!bundle::Selected(nullptr));
    CHECK(!bundle::Clear(nullptr));
    auto* root = lv_advanced_keyboard_create(lv_screen_active());
    CHECK(root);
    auto* plain = lv_obj_create(lv_screen_active());
    CHECK(!bundle::Apply(plain, "en-US", 16));
    lv_obj_delete(plain);
    auto* first = lv_textarea_create(lv_screen_active());
    auto* second = lv_textarea_create(lv_screen_active());
    CHECK(lv_advanced_keyboard_bind_textarea(root, first));
    CHECK(lv_advanced_keyboard_bind_textarea(root, second));
    CHECK(lv_advanced_keyboard_set_textarea(root, first));
    CHECK(bundle::Apply(root, "en-US", 16));
    const auto* english = bundle::Find("en-US", 16);
    CHECK(bundle::Selected(root) == english && english->modelBytes == 16384);
    auto* predictor = lv_advanced_keyboard_get_predictor(root);
    CHECK(predictor && bundle::Apply(root, "en-US", 16));
    CHECK(lv_advanced_keyboard_get_predictor(root) == predictor);
    CHECK(!bundle::Apply(root, "en-US", 32));
    CHECK(bundle::Selected(root) == english);
    CHECK(!std::strcmp(lv_advanced_keyboard_get_language(root), "en-US"));

    CHECK(bundle::Apply(root, "ar-SA", 16));
    CHECK(lv_advanced_keyboard_get_right_to_left(root));
    lv_advanced_keyboard_set_right_to_left(root, false);
    CHECK(bundle::Apply(root, "ar-SA", 256));
    CHECK(!lv_advanced_keyboard_get_right_to_left(root));
    CHECK(!std::strcmp(lv_advanced_keyboard_get_language(root), "ar-SA"));
    CHECK(bundle::Apply(root, "en-US", 16));
    CHECK(bundle::Apply(root, "ar-SA", 16));
    CHECK(lv_advanced_keyboard_get_right_to_left(root));

    CHECK(bundle::Apply(root, "zh-CN", 16));
    CHECK(!lv_advanced_keyboard_get_right_to_left(root));
    for (const auto size : {16u, 256u}) {
        CHECK(bundle::Apply(root, "zh-CN", size));
        predictor = lv_advanced_keyboard_get_predictor(root);
        lv_advanced_keyboard_suggestion_t suggestions[3]{};
        const auto count = lv_advanced_keyboard_predictor_suggest_folded(predictor,
            "", 0, "世", 3, "全", 3, suggestions, 3);
        CHECK(count && suggestions[0].length == 3 && !std::memcmp(suggestions[0].text, "界", 3));
    }
    lv_textarea_set_text(first, "全世");
    CHECK(lv_advanced_keyboard_set_textarea(root, second));
    lv_advanced_keyboard_set_text(root, "Second field");
    CHECK(!std::strcmp(lv_textarea_get_text(first), "全世"));
    CHECK(!std::strcmp(lv_textarea_get_text(second), "Second field"));
    lv_obj_delete(first);
    CHECK(lv_advanced_keyboard_bound_textarea_count(root) == 1);

    auto* other = lv_advanced_keyboard_create(lv_screen_active());
    CHECK(other && bundle::Apply(other, "zh-CN", 256));
    CHECK(bundle::Selected(root) == bundle::Selected(other));
    CHECK(lv_advanced_keyboard_get_predictor(root) != lv_advanced_keyboard_get_predictor(other));
    CHECK(bundle::Clear(root));
    CHECK(!bundle::Selected(root) && !lv_advanced_keyboard_get_predictor(root));
    CHECK(bundle::Selected(other));
    auto* borrowed = lv_advanced_keyboard_predictor_create_borrowed(
        english->trie, english->modelBytes, english->index, english->indexBytes);
    CHECK(borrowed);
    lv_advanced_keyboard_set_predictor(root, borrowed);
    CHECK(bundle::Clear(root));
    CHECK(lv_advanced_keyboard_predictor_node_count(borrowed) > 0);
    lv_advanced_keyboard_predictor_destroy(borrowed);

    bool deleted = false;
    lv_obj_add_event_cb(other, Deleting, LV_EVENT_DELETE, &deleted);
    lv_obj_delete(other);
    CHECK(deleted && !bundle::Selected(other));
    lv_obj_delete(root);
    lv_obj_delete(second);
    for (unsigned repeat = 0; repeat < 20; ++repeat) {
        auto* item = lv_advanced_keyboard_create(lv_screen_active());
        CHECK(item && bundle::Apply(item, "en-US", 16));
        lv_obj_delete(item);
        CHECK(!bundle::Selected(item));
    }
    return true;
}
}
int main() {
    lv_init();
    auto* display = lv_display_create(640, 480);
    if (!display) return 1;
    auto* pixels = std::malloc(640 * 20 * 4);
    if (!pixels) return 1;
    lv_display_set_buffers(display, pixels, nullptr, 640 * 20 * 4, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, Flush);
    const bool result = Exercise();
    lv_display_delete(display);
    lv_deinit();
    std::free(pixels);
    if (result) std::puts("Bundled profiles: model switching, Mandarin context, RTL override, external fields, ownership and deletion passed");
    return result ? 0 : 1;
}
