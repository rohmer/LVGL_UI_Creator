#include "espos_keyboard/TypingView.h"
#include "espos_keyboard/AdvancedTypingView.h"
#include "espos_keyboard/Keyboard.h"
#include "espos_keyboard/BundledModels.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace keyboard = espos::keyboard;
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Native adapter line %d: %s\n", __LINE__, #value); return false; } } while (false)
namespace {
void Flush(lv_display_t* display, const lv_area_t*, std::uint8_t*) { lv_display_flush_ready(display); }
lv_obj_t* Button(lv_obj_t* root, const char* caption)
{
    for (std::uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        auto* item = lv_obj_get_child(root, i);
        if (lv_obj_has_class(item, &lv_button_class))
            for (std::uint32_t j = 0; j < lv_obj_get_child_count(item); ++j) {
                auto* label = lv_obj_get_child(item, j);
                if (lv_obj_has_class(label, &lv_label_class) && !std::strcmp(lv_label_get_text(label), caption)) return item;
            }
        if (auto* found = Button(item, caption)) return found;
    }
    return nullptr;
}
void Key(lv_obj_t* core, keyboard::KeyInputKind kind, const char* text = nullptr)
{
    keyboard::KeyInput input{kind, text};
    lv_obj_send_event(lv_advanced_keyboard_get_keyboard(core), LV_EVENT_VALUE_CHANGED, &input);
}
void EventCount(lv_event_t* event) { ++*static_cast<unsigned*>(lv_event_get_user_data(event)); }
std::size_t CustomLookup(void*, std::string_view, std::string_view, std::string_view,
    ngram_trie::Suggestion*) noexcept { return 0; }
bool Exercise()
{
    // Include both headers and link both namespaces in the same translation unit.
    auto* standalone = keyboard::view::create_en_us_typing_view(lv_screen_active(), 400, 200, nullptr, nullptr);
    CHECK(standalone); lv_obj_delete(standalone);
    auto* outer = keyboard::create_en_us_typing_view(lv_screen_active(), nullptr, nullptr);
    CHECK(outer && lv_advanced_keyboard_language_count() == 14);
    auto* core = keyboard::typing_view_advanced_keyboard(outer);
    auto* owned = keyboard::typing_view_editor(outer);
    CHECK(core && owned && keyboard::typing_view_keyboard(outer));
    CHECK(lv_advanced_keyboard_get_autocorrect(core));
    CHECK(keyboard::bundled::Selected(core) == keyboard::bundled::Find("en-US", 16));
    CHECK(keyboard::set_typing_view_orientation(outer, LV_DISPLAY_ROTATION_90));
    CHECK(lv_display_get_rotation(lv_obj_get_display(outer)) == LV_DISPLAY_ROTATION_90);
    CHECK(!keyboard::set_typing_view_orientation(outer, static_cast<lv_display_rotation_t>(99)));
    CHECK(keyboard::set_typing_view_orientation(outer, LV_DISPLAY_ROTATION_0));
    // A locale with only a larger compiled profile must remain usable directly
    // from the language dropdown, including fallback back to the smaller model.
    CHECK(!keyboard::bundled::Find("ar-SA", 16));
    CHECK(keyboard::set_typing_view_language(outer, "ar-SA"));
    CHECK(keyboard::bundled::Selected(core) == keyboard::bundled::Find("ar-SA", 256));
    CHECK(keyboard::typing_view_right_to_left(outer));
    CHECK(keyboard::set_typing_view_right_to_left(outer, false));
    auto* arabic_predictor = lv_advanced_keyboard_get_predictor(core);
    CHECK(keyboard::set_typing_view_language(outer, "ar-SA"));
    CHECK(!keyboard::typing_view_right_to_left(outer) && lv_advanced_keyboard_get_predictor(core) == arabic_predictor);
    CHECK(keyboard::set_typing_view_language(outer, "en-US"));
    CHECK(keyboard::bundled::Selected(core) == keyboard::bundled::Find("en-US", 16));
    CHECK(keyboard::set_typing_view_language(outer, "el-GR"));
    CHECK(!std::strcmp(keyboard::typing_view_language(outer), "el-GR"));
    CHECK(!lv_advanced_keyboard_get_predictor(core)); // Missing compiled locale is explicit None.
    CHECK(!keyboard::set_typing_view_language(outer, "missing"));
    CHECK(keyboard::set_typing_view_language(outer, "ar-SA") && keyboard::typing_view_right_to_left(outer));
    CHECK(keyboard::set_typing_view_right_to_left(outer, false));
    CHECK(keyboard::set_typing_view_trie(outer, "ar-SA", 256) && !keyboard::typing_view_right_to_left(outer));
    CHECK(keyboard::set_typing_view_trie(outer, "zh-CN", 256));
    CHECK(!std::strcmp(keyboard::typing_view_language(outer), "zh-CN"));
    auto* toolbar = lv_obj_get_child(outer, 1);
    auto* sizes = lv_obj_get_child(toolbar, 1);
    const auto* options = lv_dropdown_get_options(sizes);
    CHECK(std::strstr(options, "16 KiB") && std::strstr(options, "256 KiB"));
    lv_dropdown_set_selected(sizes, 1); lv_obj_send_event(sizes, LV_EVENT_VALUE_CHANGED, nullptr);
    CHECK(keyboard::bundled::Selected(core)->sizeKiB == 16);
    lv_dropdown_set_selected(sizes, 0); lv_obj_send_event(sizes, LV_EVENT_VALUE_CHANGED, nullptr);
    CHECK(!lv_advanced_keyboard_get_predictor(core));
    CHECK(keyboard::set_typing_view_language(outer, "en-US"));
    auto* first = lv_textarea_create(lv_screen_active());
    auto* second = lv_textarea_create(lv_screen_active());
    CHECK(lv_advanced_keyboard_bind_textarea(core, first) && lv_advanced_keyboard_bind_textarea(core, second));
    CHECK(keyboard::set_typing_view_editor(outer, first));
    Key(core, keyboard::KeyInputKind::Insert, "a");
    CHECK(!std::strcmp(lv_textarea_get_text(first), "a"));
    CHECK(keyboard::set_typing_view_editor(outer, second));
    Key(core, keyboard::KeyInputKind::Insert, "b");
    CHECK(!std::strcmp(lv_textarea_get_text(first), "a") && !std::strcmp(lv_textarea_get_text(second), "b"));
    unsigned ready = 0, cancel = 0;
    lv_obj_add_event_cb(second, EventCount, LV_EVENT_READY, &ready);
    lv_obj_add_event_cb(core, EventCount, LV_EVENT_CANCEL, &cancel);
    Key(core, keyboard::KeyInputKind::Enter);
    CHECK(ready == 1 && lv_advanced_keyboard_is_dismissed(core));
    CHECK(!std::strchr(lv_textarea_get_text(second), '\n'));
    lv_obj_send_event(first, LV_EVENT_CLICKED, nullptr);
    CHECK(lv_advanced_keyboard_get_textarea(core) == first && !lv_advanced_keyboard_is_dismissed(core));
    auto* close = Button(core, "X"); CHECK(close);
    lv_obj_send_event(close, LV_EVENT_CLICKED, nullptr);
    CHECK(cancel == 1 && lv_advanced_keyboard_is_dismissed(core));
    CHECK(keyboard::typing_view_editor(outer) == owned);
    lv_obj_delete(first); lv_obj_delete(second);
    lv_obj_delete(outer);
    // Both object deletion orders must remain safe.
    outer = keyboard::create_en_us_typing_view(lv_screen_active(), 600, 400, nullptr, nullptr);
    CHECK(outer); lv_obj_delete(keyboard::typing_view_advanced_keyboard(outer)); lv_obj_delete(outer);
    outer = keyboard::create_en_us_typing_view(lv_screen_active(), 600, 400, nullptr, nullptr);
    CHECK(outer); lv_obj_delete(keyboard::typing_view_editor(outer)); lv_obj_delete(outer);
    outer = keyboard::create_en_us_typing_view(lv_screen_active(), 600, 400, CustomLookup, nullptr);
    CHECK(outer && keyboard::set_typing_view_language(outer, "ar-SA"));
    core = keyboard::typing_view_advanced_keyboard(outer);
    CHECK(!lv_advanced_keyboard_get_predictor(core));
    sizes = lv_obj_get_child(lv_obj_get_child(outer, 1), 1);
    CHECK(std::strstr(lv_dropdown_get_options(sizes), "Partition / custom"));
    lv_obj_delete(outer);
    return true;
}
}
int main()
{
    lv_init(); auto* display = lv_display_create(1024, 600);
    auto* pixels = std::malloc(1024 * 20 * 4); if (!display || !pixels) return 1;
    lv_display_set_buffers(display, pixels, nullptr, 1024 * 20 * 4, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, Flush);
    const bool result = Exercise();
    lv_display_delete(display); lv_deinit(); std::free(pixels);
    if (result) std::puts("Legacy adapter: dual namespaces, orientation, 14 languages, RTL, trie size, external targets, Enter/X, deletion passed");
    return result ? 0 : 1;
}
