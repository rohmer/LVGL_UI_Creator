#include "espos_keyboard/LegacyTypingView.h"
#include "espos_keyboard/Keyboard.h"
#include "espos_keyboard/BundledModels.h"
#include "lv_advanced_keyboard_prediction.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace espos::keyboard {
namespace {
constexpr std::array<lv_display_rotation_t, 4> rotations{
    LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90, LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270};
constexpr std::uint32_t custom_model = UINT32_MAX;
struct TypingView {
    lv_obj_t* root = nullptr;
    lv_obj_t* editor = nullptr;
    lv_obj_t* keyboard = nullptr;
    lv_obj_t* language = nullptr;
    lv_obj_t* size = nullptr;
    lv_obj_t* rtl = nullptr;
    lv_obj_t* orientation = nullptr;
    bool authored_font = false;
    bool fill_parent = false;
    SuggestionLookup lookup = nullptr;
    void* context = nullptr;
    lv_advanced_keyboard_predictor_t* owned_predictor = nullptr;
    std::shared_ptr<const void> model_owner;
    std::vector<std::uint32_t> sizes;
};
void on_delete(lv_event_t* event);
TypingView* data(lv_obj_t* root)
{
    if (!root) return nullptr;
    for (std::uint32_t i = 0; i < lv_obj_get_event_count(root); ++i) {
        auto* descriptor = lv_obj_get_event_dsc(root, i);
        if (lv_event_dsc_get_cb(descriptor) == on_delete)
            return static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
    }
    return nullptr;
}
void release_owned(TypingView& view)
{
    lv_advanced_keyboard_predictor_destroy(view.owned_predictor);
    view.owned_predictor = nullptr;
    view.model_owner.reset();
}
void on_keyboard_delete(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    view.keyboard = nullptr;
    release_owned(view);
}
void on_editor_delete(lv_event_t* event)
{
    static_cast<TypingView*>(lv_event_get_user_data(event))->editor = nullptr;
}
void on_delete(lv_event_t* event)
{
    auto* view = static_cast<TypingView*>(lv_event_get_user_data(event));
    if (view->keyboard) {
        lv_obj_remove_event_cb_with_user_data(view->keyboard, on_keyboard_delete, view);
        lv_advanced_keyboard_set_predictor(view->keyboard, nullptr);
    }
    if (view->editor) lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_delete, view);
    release_owned(*view);
    delete view;
}
void resize(TypingView& view)
{
    if (!view.keyboard) return;
    const auto height = lv_obj_get_content_height(view.root);
    lv_obj_set_height(view.keyboard, std::max<std::int32_t>(120, height * 58 / 100));
}
void on_size(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    resize(view);
}
std::size_t lookup_bridge(void* context, const char* prefix, std::size_t prefix_size,
    const char* previous, std::size_t previous_size, const char* older, std::size_t older_size,
    lv_advanced_keyboard_suggestion_t* output, std::size_t capacity)
{
    auto& view = *static_cast<TypingView*>(context);
    if (!view.lookup) return 0;
    std::array<ngram_trie::Suggestion, ngram_trie::kSuggestionCount> suggestions{};
    const auto count = std::min({capacity, suggestions.size(), view.lookup(view.context,
        {prefix, prefix_size}, {previous, previous_size}, {older, older_size}, suggestions.data())});
    for (std::size_t i = 0; i < count; ++i) {
        output[i].length = suggestions[i].length;
        output[i].mass = suggestions[i].mass;
        std::memcpy(output[i].text, suggestions[i].text.data(), sizeof(output[i].text));
    }
    return count;
}
void refresh_controls(TypingView& view)
{
    if (!view.keyboard) return;
    const auto* language = lv_advanced_keyboard_get_language(view.keyboard);
    for (std::size_t i = 0; i < lv_advanced_keyboard_language_count(); ++i)
        if (!std::strcmp(language, lv_advanced_keyboard_language_id(i)))
            lv_dropdown_set_selected(view.language, static_cast<std::uint32_t>(i));
    if (view.editor && !view.authored_font)
        lv_obj_set_style_text_font(view.editor, keyboard_language_font(language), LV_PART_MAIN);
    if (lv_advanced_keyboard_get_right_to_left(view.keyboard)) lv_obj_add_state(view.rtl, LV_STATE_CHECKED);
    else lv_obj_remove_state(view.rtl, LV_STATE_CHECKED);
    view.sizes.clear(); view.sizes.push_back(0);
    std::string options = "None";
    for (std::size_t i = 0; i < bundled::Count(); ++i) {
        const auto* model = bundled::At(i);
        if (model && !std::strcmp(model->language, language)) view.sizes.push_back(model->sizeKiB);
    }
    std::sort(view.sizes.begin() + 1, view.sizes.end());
    view.sizes.erase(std::unique(view.sizes.begin(), view.sizes.end()), view.sizes.end());
    for (std::size_t i = 1; i < view.sizes.size(); ++i) options += "\n" + std::to_string(view.sizes[i]) + " KiB";
    const auto* selected = bundled::Selected(view.keyboard);
    std::uint32_t current = selected ? selected->sizeKiB : 0;
    if (!selected && (view.lookup || lv_advanced_keyboard_get_predictor(view.keyboard))) {
        view.sizes.push_back(custom_model); options += "\nPartition / custom"; current = custom_model;
    }
    lv_dropdown_set_options(view.size, options.c_str());
    for (std::size_t i = 0; i < view.sizes.size(); ++i)
        if (view.sizes[i] == current) lv_dropdown_set_selected(view.size, static_cast<std::uint32_t>(i));
}
void on_language(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    const auto index = lv_dropdown_get_selected(view.language);
    set_typing_view_language(view.root, lv_advanced_keyboard_language_id(index));
}
void on_model(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    const auto index = lv_dropdown_get_selected(view.size);
    if (index < view.sizes.size() && view.sizes[index] != custom_model)
        set_typing_view_trie(view.root, lv_advanced_keyboard_get_language(view.keyboard), view.sizes[index]);
}
void on_rtl(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    set_typing_view_right_to_left(view.root, lv_obj_has_state(view.rtl, LV_STATE_CHECKED));
}
void on_orientation(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    const auto index = lv_dropdown_get_selected(view.orientation);
    if (index < rotations.size()) set_typing_view_orientation(view.root, rotations[index]);
}
lv_obj_t* create_view(lv_obj_t* parent, std::int32_t width, std::int32_t height,
    SuggestionLookup lookup, void* context, const KeyStyle& style, bool fill_parent)
{
    if (!parent || width <= 0 || height <= 0) return nullptr;
    auto* state = new (std::nothrow) TypingView;
    if (!state) return nullptr;
    auto* root = lv_obj_create(parent);
    if (!root) { delete state; return nullptr; }
    state->root = root; state->lookup = lookup; state->context = context;
    state->authored_font = style.font != nullptr; state->fill_parent = fill_parent;
    if (!lv_obj_add_event_cb(root, on_delete, LV_EVENT_DELETE, state)) { lv_obj_delete(root); delete state; return nullptr; }
    lv_obj_set_size(root, fill_parent ? LV_PCT(100) : width, fill_parent ? LV_PCT(100) : height);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_pad_row(root, 6, 0);
    state->editor = lv_textarea_create(root);
    auto* toolbar = lv_obj_create(root);
    state->keyboard = lv_advanced_keyboard_create(root);
    if (!state->editor || !toolbar || !state->keyboard) { lv_obj_delete(root); return nullptr; }
    if (!lv_obj_add_event_cb(state->keyboard, on_keyboard_delete, LV_EVENT_DELETE, state) ||
        !lv_obj_add_event_cb(state->editor, on_editor_delete, LV_EVENT_DELETE, state)) { lv_obj_delete(root); return nullptr; }
    lv_obj_set_width(state->editor, LV_PCT(100));
    lv_obj_set_flex_grow(state->editor, 1);
    lv_obj_set_style_text_font(state->editor, style.font ? style.font : keyboard_language_font("en-US"), LV_PART_MAIN);
    lv_textarea_set_placeholder_text(state->editor, "Select a language and trie, then type here");
    lv_obj_set_size(toolbar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(toolbar, 4, 0);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    state->language = lv_dropdown_create(toolbar);
    state->size = lv_dropdown_create(toolbar);
    state->rtl = lv_checkbox_create(toolbar);
    if (!state->language || !state->size || !state->rtl) { lv_obj_delete(root); return nullptr; }
    lv_obj_set_width(state->language, 210); lv_obj_set_width(state->size, 155);
    std::string languages;
    for (std::size_t i = 0; i < lv_advanced_keyboard_language_count(); ++i) {
        if (i) languages += '\n';
        languages += lv_advanced_keyboard_language_name(i);
    }
    lv_dropdown_set_options(state->language, languages.c_str());
    lv_checkbox_set_text(state->rtl, "RTL");
    if (fill_parent) {
        state->orientation = lv_dropdown_create(toolbar);
        if (!state->orientation) { lv_obj_delete(root); return nullptr; }
        lv_dropdown_set_options(state->orientation, "0 degrees\n90 degrees\n180 degrees\n270 degrees");
        lv_obj_set_width(state->orientation, 130);
        const auto rotation = lv_display_get_rotation(lv_obj_get_display(root));
        for (std::size_t i = 0; i < rotations.size(); ++i)
            if (rotations[i] == rotation) lv_dropdown_set_selected(state->orientation, static_cast<std::uint32_t>(i));
        if (!lv_obj_add_event_cb(state->orientation, on_orientation, LV_EVENT_VALUE_CHANGED, state)) { lv_obj_delete(root); return nullptr; }
    }
    if (!lv_obj_add_event_cb(state->language, on_language, LV_EVENT_VALUE_CHANGED, state) ||
        !lv_obj_add_event_cb(state->size, on_model, LV_EVENT_VALUE_CHANGED, state) ||
        !lv_obj_add_event_cb(state->rtl, on_rtl, LV_EVENT_VALUE_CHANGED, state) ||
        !lv_obj_add_event_cb(root, on_size, LV_EVENT_SIZE_CHANGED, state)) { lv_obj_delete(root); return nullptr; }
    lv_obj_set_width(state->keyboard, LV_PCT(100));
    lv_advanced_keyboard_set_key_background(state->keyboard, style.background);
    lv_advanced_keyboard_set_key_foreground(state->keyboard, style.foreground);
    lv_advanced_keyboard_set_key_radius(state->keyboard, style.radius);
    if (style.font) lv_obj_set_style_text_font(state->keyboard, style.font, LV_PART_MAIN);
    if (lookup) lv_advanced_keyboard_set_suggestion_lookup(state->keyboard, lookup_bridge, state);
    else if (const auto* model = bundled::Find("en-US", 16)) bundled::Apply(state->keyboard, model->language, model->sizeKiB);
    lv_advanced_keyboard_set_autocorrect(state->keyboard, true);
    if (!lv_advanced_keyboard_bind_textarea(state->keyboard, state->editor) ||
        !lv_advanced_keyboard_set_textarea(state->keyboard, state->editor)) { lv_obj_delete(root); return nullptr; }
    lv_obj_update_layout(root); resize(*state); refresh_controls(*state);
    return root;
}
}
lv_display_t* create_default_typing_display() { return lv_display_create(1024, 600); }
lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, std::int32_t width, std::int32_t height,
    SuggestionLookup lookup, void* context, const KeyStyle& style)
{
    return create_view(parent, width, height, lookup, context, style, false);
}
lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, SuggestionLookup lookup, void* context, const KeyStyle& style)
{
    if (!parent) return nullptr;
    lv_obj_update_layout(parent);
    return create_view(parent, lv_obj_get_content_width(parent), lv_obj_get_content_height(parent), lookup, context, style, true);
}
bool set_typing_view_orientation(lv_obj_t* root, lv_display_rotation_t rotation)
{
    auto* view = data(root);
    if (!view || std::find(rotations.begin(), rotations.end(), rotation) == rotations.end()) return false;
    auto* display = lv_obj_get_display(root);
    if (!display) return false;
    if (lv_display_get_rotation(display) != rotation) lv_display_set_rotation(display, rotation);
    if (view->orientation)
        lv_dropdown_set_selected(view->orientation, static_cast<std::uint32_t>(std::find(rotations.begin(), rotations.end(), rotation) - rotations.begin()));
    return true;
}
lv_obj_t* typing_view_editor(lv_obj_t* root)
{
    auto* view = data(root);
    if (!view) return nullptr;
    auto* active = view->keyboard ? lv_advanced_keyboard_get_textarea(view->keyboard) : nullptr;
    return active ? active : view->editor;
}
lv_obj_t* typing_view_advanced_keyboard(lv_obj_t* root) { auto* view = data(root); return view ? view->keyboard : nullptr; }
lv_obj_t* typing_view_keyboard(lv_obj_t* root) { return lv_advanced_keyboard_get_keyboard(typing_view_advanced_keyboard(root)); }
bool set_typing_view_editor(lv_obj_t* root, lv_obj_t* editor)
{
    auto* keyboard = typing_view_advanced_keyboard(root);
    return keyboard && lv_advanced_keyboard_set_textarea(keyboard, editor);
}
bool set_typing_view_language(lv_obj_t* root, const char* language)
{
    auto* view = data(root);
    if (!view || !view->keyboard || !language) return false;
    const auto* selected = bundled::Selected(view->keyboard);
    const auto size = selected ? selected->sizeKiB : 16;
    const bool changed = std::strcmp(lv_advanced_keyboard_get_language(view->keyboard), language) != 0;
    if (!lv_advanced_keyboard_set_language(view->keyboard, language)) return false;
    if (changed && !view->lookup) {
        const auto* model = bundled::Find(language, size);
        if (!model) for (std::size_t i = 0; i < bundled::Count(); ++i) {
            const auto* candidate = bundled::At(i);
            if (candidate && !std::strcmp(candidate->language, language) &&
                (!model || candidate->sizeKiB < model->sizeKiB)) model = candidate;
        }
        if (!model || !bundled::Apply(view->keyboard, language, model->sizeKiB)) bundled::Clear(view->keyboard);
        release_owned(*view);
    }
    refresh_controls(*view); return true;
}
const char* typing_view_language(lv_obj_t* root) { return lv_advanced_keyboard_get_language(typing_view_advanced_keyboard(root)); }
bool set_typing_view_right_to_left(lv_obj_t* root, bool enabled)
{
    auto* view = data(root); if (!view || !view->keyboard) return false;
    lv_advanced_keyboard_set_right_to_left(view->keyboard, enabled); refresh_controls(*view); return true;
}
bool typing_view_right_to_left(lv_obj_t* root) { return lv_advanced_keyboard_get_right_to_left(typing_view_advanced_keyboard(root)); }
bool set_typing_view_trie(lv_obj_t* root, const char* language, std::uint32_t size_kib)
{
    auto* view = data(root); if (!view || !view->keyboard || !language) return false;
    if (size_kib ? !bundled::Apply(view->keyboard, language, size_kib) : !bundled::Clear(view->keyboard)) return false;
    view->lookup = nullptr; view->context = nullptr; release_owned(*view);
    refresh_controls(*view); return true;
}
bool set_typing_view_owned_predictor(lv_obj_t* root, lv_advanced_keyboard_predictor_t* predictor, std::shared_ptr<const void> owner)
{
    auto* view = data(root); if (!view || !view->keyboard) return false;
    if (predictor == view->owned_predictor && predictor) return true;
    bundled::Clear(view->keyboard);
    view->lookup = nullptr; view->context = nullptr;
    lv_advanced_keyboard_set_predictor(view->keyboard, predictor);
    release_owned(*view); view->owned_predictor = predictor; view->model_owner = std::move(owner);
    refresh_controls(*view); return true;
}
}
