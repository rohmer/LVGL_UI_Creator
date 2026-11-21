#include "espos_keyboard/TypingView.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>

#include "espos_keyboard/Keyboard.h"
#include "Utf8Fold.h"
#include "SuggestionText.h"

extern "C" {
LV_FONT_DECLARE(espos_keyboard_font)
}

namespace espos::keyboard {
namespace {

using prediction::kSuggestionBytes;
using prediction::kSuggestionCount;

struct SuggestionSlot {
    lv_obj_t* button = nullptr;
    lv_obj_t* label = nullptr;
    std::array<char, kSuggestionBytes + 1> word{};
    std::size_t length = 0;
};

struct TypingView {
    std::uint64_t instance = 0;
    lv_obj_t* root = nullptr;
    lv_obj_t* editor = nullptr;
    lv_obj_t* bar = nullptr;
    lv_obj_t* keyboard = nullptr;
    lv_obj_t* close = nullptr;
    lv_timer_t* timer = nullptr;
    SuggestionLookup lookup = nullptr;
    void* context = nullptr;
    WordBoundaryHandler boundary = nullptr;
    void* boundary_context = nullptr;
    DismissHandler dismiss = nullptr;
    void* dismiss_context = nullptr;
    bool processing_insert = false;
    std::uint32_t last_caret = 0;
    bool last_blocked = false;
    bool dirty = true;
    std::array<SuggestionSlot, kSuggestionCount> slots{};
    std::array<prediction::Suggestion, kSuggestionCount> suggestions{};
};

constexpr auto slot_count = static_cast<std::int32_t>(kSuggestionCount);
constexpr std::uint32_t suggestion_delay_ms = 45;
std::uint64_t next_instance = 0;
void on_delete(lv_event_t* event);
TypingView* live_view(lv_obj_t* root, std::uint64_t instance)
{
    if (!lv_obj_is_in_widget_tree(root)) return nullptr;
    for (uint32_t index = 0; index < lv_obj_get_event_count(root); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(root, index);
        if (lv_event_dsc_get_cb(descriptor) == on_delete) {
            auto* view = static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
            return view->instance == instance ? view : nullptr;
        }
    }
    return nullptr;
}

void layout(TypingView& view, std::int32_t width, std::int32_t height)
{
    const auto bar_height = std::clamp<std::int32_t>(46, 1, height - 1);
    const auto keyboard_height = height - bar_height;
    const auto close_width = std::min<std::int32_t>(std::max<std::int32_t>(1, width - slot_count),
        std::max<std::int32_t>(32, std::min<std::int32_t>(46, bar_height)));
    const auto suggestions_width = width - close_width;
    lv_obj_set_pos(view.bar, 0, 0);
    lv_obj_set_size(view.bar, width, bar_height);
    lv_obj_set_pos(view.keyboard, 0, height - keyboard_height);
    lv_obj_set_size(view.keyboard, width, keyboard_height);
    for (std::int32_t index = 0; index < slot_count; ++index) {
        const auto left = index * suggestions_width / slot_count;
        const auto right = (index + 1) * suggestions_width / slot_count;
        const auto gap = index + 1 < slot_count && right - left > 1 ? 1 : 0;
        lv_obj_set_pos(view.slots[index].button, left, 0);
        lv_obj_set_size(view.slots[index].button, right - left - gap, bar_height);
    }
    lv_obj_set_pos(view.close, suggestions_width, 0);
    lv_obj_set_size(view.close, close_width, bar_height);
}

void show_slot(SuggestionSlot& slot, std::size_t length)
{
    slot.length = length;
    if (length != 0) {
        lv_label_set_text(slot.label, slot.word.data());
        lv_obj_remove_state(slot.button, LV_STATE_DISABLED);
        lv_obj_set_clickable(slot.button, true);
    } else {
        lv_label_set_text_static(slot.label, "");
        lv_obj_set_clickable(slot.button, false);
        lv_obj_add_state(slot.button, LV_STATE_DISABLED);
    }
}

void clear(TypingView& view)
{
    for (auto& slot : view.slots) {
        if (slot.length != 0) show_slot(slot, 0);
    }
}

void refresh(lv_timer_t* timer)
{
    auto& view = *static_cast<TypingView*>(lv_timer_get_user_data(timer));
    if (!view.editor) { clear(view); return; }
    const auto caret = lv_textarea_get_cursor_pos(view.editor);
    const auto blocked = lv_textarea_text_is_selected(view.editor) || lv_textarea_get_password_mode(view.editor);
    if (!view.dirty && caret == view.last_caret && blocked == view.last_blocked) return;

    view.dirty = false;
    view.last_caret = caret;
    view.last_blocked = blocked;
    if (view.lookup == nullptr || blocked) {
        clear(view);
        return;
    }

    const auto input = detail::capture_suggestion_text(view.editor);
    std::array<char, kSuggestionBytes> prefix_buffer, previous_buffer, older_buffer;
    std::string_view prefix, previous, older;
    if (!prediction::FoldUtf8(input.prefix, prefix_buffer.data(), prefix_buffer.size(), prefix)) {
        clear(view);
        return;
    }
    if (prediction::FoldUtf8(input.previous, previous_buffer.data(), previous_buffer.size(), previous)) {
        (void)prediction::FoldUtf8(input.older, older_buffer.data(), older_buffer.size(), older);
    }
    const auto count = std::min(view.lookup(view.context, prefix, previous, older, view.suggestions.data()),
                                kSuggestionCount);
    for (std::size_t index = 0; index < kSuggestionCount; ++index) {
        auto& slot = view.slots[index];
        const auto& suggestion = view.suggestions[index];
        if (index < count && suggestion.length <= suggestion.text.size()) {
            show_slot(slot, detail::display_suggestion(input, suggestion.Word(), slot.word.data(), slot.word.size()));
        } else {
            show_slot(slot, 0);
        }
    }
}

void on_edit(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    view.dirty = true;
    clear(view);
    lv_timer_reset(view.timer);
    if (!view.processing_insert) lv_obj_send_event(view.root, LV_EVENT_VALUE_CHANGED, nullptr);
}

bool correct_boundary(TypingView& view)
{
    if (!view.boundary || view.processing_insert || !view.editor) return true;
    auto* root = view.root;
    const auto instance = view.instance;
    view.processing_insert = true;
    view.boundary(view.boundary_context, view.editor);
    if (!live_view(root, instance)) return false;
    view.processing_insert = false;
    return true;
}

void finish(TypingView& view, bool commit)
{
    auto* root = view.root;
    const auto instance = view.instance;
    if (commit) {
        commit_en_us_keyboard_text(view.keyboard);
        if (!live_view(root, instance)) return;
    }
    if (commit && view.editor && !lv_obj_has_state(view.editor, LV_STATE_DISABLED) && view.boundary) {
        if (!correct_boundary(view)) return;
        if (lv_obj_send_event(root, LV_EVENT_VALUE_CHANGED, nullptr) != LV_RESULT_OK) return;
    }
    auto* editor = view.editor;
    auto dismiss = [](TypingView& current) {
        if (current.dismiss) current.dismiss(current.dismiss_context);
        else {
            auto* currentRoot = current.root;
            set_typing_view_editor(currentRoot, nullptr);
            lv_obj_set_hidden(currentRoot, true);
        }
    };
    // Clear pointers before application callbacks. Field READY handlers may
    // legitimately delete this keyboard or its whole screen.
    dismiss(view);
    if (commit && editor && lv_obj_is_in_widget_tree(editor)) lv_obj_send_event(editor, LV_EVENT_READY, nullptr);
    auto* current = live_view(root, instance);
    if (!current) return;
    if (lv_obj_send_event(root, commit ? LV_EVENT_READY : LV_EVENT_CANCEL, nullptr) != LV_RESULT_OK) return;
    // A callback can refocus the field. Keep this close action authoritative,
    // and distinguish a replacement object which reuses the same address.
    if ((current = live_view(root, instance))) dismiss(*current);
}

void on_close(lv_event_t* event)
{
    finish(*static_cast<TypingView*>(lv_event_get_user_data(event)), false);
}

void local_cancel(lv_event_t* event)
{
    lv_event_stop_bubbling(event);
}

void on_editor_key(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    const auto key = lv_event_get_key(event);
    if (key == LV_KEY_ENTER) {
        lv_event_stop_processing(event);
        finish(view, true);
    }
    else if (key && key < 128 && std::strchr(" \t\r\n.,!?;:", static_cast<int>(key))) {
        if (!correct_boundary(view)) lv_event_stop_processing(event);
    }
}

void on_suggestion(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    if (!view.editor || view.dirty || lv_textarea_get_cursor_pos(view.editor) != view.last_caret ||
        lv_textarea_text_is_selected(view.editor) || lv_textarea_get_password_mode(view.editor)) return;

    const auto* target = lv_event_get_target_obj(event);
    const auto slot = std::find_if(view.slots.begin(), view.slots.end(),
        [target](const SuggestionSlot& item) { return item.button == target; });
    if (slot == view.slots.end() || slot->length == 0) return;

    // Accepting the word emits a text edit event and clears the current suggestions.
    const auto word = slot->word;
    auto* keyboard = view.keyboard;
    if (detail::accept_suggestion(view.editor, detail::capture_suggestion_text(view.editor),
                                  {word.data(), slot->length}) && lv_obj_is_in_widget_tree(keyboard)) {
        commit_en_us_keyboard_text(keyboard);
    }
}

void on_key_input(lv_event_t* event)
{
    const auto* input = static_cast<const KeyInput*>(lv_event_get_param(event));
    if (input == nullptr) return;
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    if (input->kind == KeyInputKind::Enter) { finish(view, true); return; }
    if (!view.editor || lv_obj_has_state(view.editor, LV_STATE_DISABLED)) return;
    switch (input->kind) {
    case KeyInputKind::Insert:
        if (input->text && input->text[0] && std::strchr(" \t\r\n.,!?;:", input->text[0]) && !correct_boundary(view)) return;
        if (view.editor) lv_textarea_add_text(view.editor, input->text);
        break;
    case KeyInputKind::Backspace:
        lv_textarea_delete_char(view.editor);
        break;
    case KeyInputKind::Enter:
        break;
    }
}

void on_size(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    auto* root = lv_event_get_target_obj(event);
    const auto width = lv_obj_get_content_width(root);
    const auto height = lv_obj_get_content_height(root);
    const auto minimum_height = 2;
    if (width >= slot_count && height >= minimum_height) layout(view, width, height);
}

void on_editor_delete(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    view.editor = nullptr;
    view.dirty = true;
    clear(view);
    lv_timer_pause(view.timer);
}

void on_delete(lv_event_t* event)
{
    auto* view = static_cast<TypingView*>(lv_event_get_user_data(event));
    if (view->editor) {
        lv_obj_remove_event_cb_with_user_data(view->editor, on_edit, view);
        lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_delete, view);
        lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_key, view);
    }
    if (view->timer != nullptr) lv_timer_delete(view->timer);
    view->~TypingView();
    lv_free(view);
}

lv_obj_t* create_typing_view(lv_obj_t* parent, std::int32_t width, std::int32_t height,
                            SuggestionLookup lookup, void* context, const KeyStyle& style)
{
    const auto minimum_height = 2;
    if (parent == nullptr || width <= slot_count || height < minimum_height ||
        width > LV_COORD_MAX || height > LV_COORD_MAX) {
        return nullptr;
    }
    auto* root = lv_obj_create(parent);
    if (root == nullptr) return nullptr;
    auto* storage = lv_malloc(sizeof(TypingView));
    if (storage == nullptr) {
        lv_obj_delete(root);
        return nullptr;
    }
    auto* view = new (storage) TypingView{};
    view->instance = ++next_instance;
    view->root = root;
    if (lv_obj_add_event_cb(root, on_delete, LV_EVENT_DELETE, view) == nullptr) {
        view->~TypingView();
        lv_free(view);
        lv_obj_delete(root);
        return nullptr;
    }
    view->lookup = lookup;
    view->context = context;

    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, width, height);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_clickable(root, false);
    lv_obj_set_style_bg_color(root, lv_color_hex(0xd7e0ed), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);

    view->bar = lv_obj_create(root);
    view->keyboard = create_en_us_keyboard(root, width, 1, style);
    if (view->bar == nullptr || view->keyboard == nullptr) {
        lv_obj_delete(root);
        return nullptr;
    }
    const auto* font = style.font != nullptr ? style.font : &espos_keyboard_font;

    lv_obj_remove_style_all(view->bar);
    lv_obj_set_scrollable(view->bar, false);
    lv_obj_set_clickable(view->bar, false);
    lv_obj_set_style_bg_color(view->bar, lv_color_hex(0xb4c2d6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->bar, LV_OPA_COVER, LV_PART_MAIN);

    const KeyStyle suggestion_style{lv_color_hex(0xd7e0ed), lv_color_hex(0x1e2c41), 0, font};
    for (auto& slot : view->slots) {
        slot.button = create_key(view->bar, "", 1, 1, suggestion_style);
        if (slot.button == nullptr || lv_obj_add_event_cb(slot.button, on_suggestion, LV_EVENT_CLICKED, view) == nullptr) {
            lv_obj_delete(root);
            return nullptr;
        }
        slot.label = lv_obj_get_child(slot.button, 0);
        lv_obj_set_height(slot.label, lv_font_get_line_height(font));
        lv_label_set_long_mode(slot.label, LV_LABEL_LONG_MODE_DOTS);
        show_slot(slot, 0);
    }
    view->close = create_key(view->bar, "X", 1, 1, suggestion_style);
    if (!view->close || !lv_obj_add_event_cb(view->close, on_close, LV_EVENT_CLICKED, view)) {
        lv_obj_delete(root); return nullptr;
    }
    view->timer = lv_timer_create(refresh, suggestion_delay_ms, view);
    if (view->timer == nullptr ||
        lv_obj_add_event_cb(root, on_size, LV_EVENT_SIZE_CHANGED, view) == nullptr ||
        lv_obj_add_event_cb(root, local_cancel, static_cast<lv_event_code_t>(LV_EVENT_CANCEL | LV_EVENT_PREPROCESS), nullptr) == nullptr ||
        lv_obj_add_event_cb(view->keyboard, on_key_input, LV_EVENT_VALUE_CHANGED, view) == nullptr) {
        lv_obj_delete(root);
        return nullptr;
    }
    layout(*view, width, height);
    refresh(view->timer);
    lv_timer_pause(view->timer);
    return root;
}

}  // namespace

lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, std::int32_t width, std::int32_t height,
                                  SuggestionLookup lookup, void* context, const KeyStyle& style)
{
    return create_typing_view(parent, width, height, lookup, context, style);
}

lv_obj_t* typing_view_editor(lv_obj_t* view)
{
    if (!view) return nullptr;
    for (std::uint32_t index = 0; index < lv_obj_get_event_count(view); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(view, index);
        if (lv_event_dsc_get_cb(descriptor) == on_delete)
            return static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor))->editor;
    }
    return nullptr;
}

lv_obj_t* typing_view_keyboard(lv_obj_t* view)
{
    return view == nullptr ? nullptr : lv_obj_get_child(view, 1);
}

bool set_typing_view_editor(lv_obj_t* root, lv_obj_t* editor)
{
    if (!root || (editor && !lv_obj_has_class(editor, &lv_textarea_class))) return false;
    for (std::uint32_t index = 0; index < lv_obj_get_event_count(root); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(root, index);
        if (lv_event_dsc_get_cb(descriptor) != on_delete) continue;
        auto* view = static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
        if (view->editor == editor) return true;
        if (editor) {
            if (!lv_obj_add_event_cb(editor, on_edit, LV_EVENT_VALUE_CHANGED, view)) return false;
            if (!lv_obj_add_event_cb(editor, on_editor_delete, LV_EVENT_DELETE, view)) {
                lv_obj_remove_event_cb_with_user_data(editor, on_edit, view);
                return false;
            }
            if (!lv_obj_add_event_cb(editor, on_editor_key, static_cast<lv_event_code_t>(LV_EVENT_KEY | LV_EVENT_PREPROCESS), view)) {
                lv_obj_remove_event_cb_with_user_data(editor, on_edit, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_delete, view);
                return false;
            }
        }
        if (view->editor) {
            lv_obj_remove_event_cb_with_user_data(view->editor, on_edit, view);
            lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_delete, view);
            lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_key, view);
        }
        view->editor = editor;
        view->dirty = true;
        clear(*view);
        if (editor && view->lookup) lv_timer_resume(view->timer);
        else lv_timer_pause(view->timer);
        return true;
    }
    return false;
}

void set_typing_view_lookup(lv_obj_t* root, SuggestionLookup lookup, void* context)
{
    if (!root) return;
    for (std::uint32_t index = 0; index < lv_obj_get_event_count(root); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(root, index);
        if (lv_event_dsc_get_cb(descriptor) != on_delete) continue;
        auto* view = static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
        view->lookup = lookup;
        view->context = context;
        view->dirty = true;
        if (lookup && view->editor) lv_timer_resume(view->timer);
        else lv_timer_pause(view->timer);
        refresh(view->timer);
        return;
    }
}

void set_typing_view_word_boundary_handler(lv_obj_t* root, WordBoundaryHandler handler, void* context)
{
    if (!root) return;
    for (std::uint32_t index = 0; index < lv_obj_get_event_count(root); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(root, index);
        if (lv_event_dsc_get_cb(descriptor) != on_delete) continue;
        auto* view = static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
        view->boundary = handler;
        view->boundary_context = context;
        return;
    }
}

void set_typing_view_dismiss_handler(lv_obj_t* root, DismissHandler handler, void* context)
{
    if (!root) return;
    for (std::uint32_t index = 0; index < lv_obj_get_event_count(root); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(root, index);
        if (lv_event_dsc_get_cb(descriptor) != on_delete) continue;
        auto* view = static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
        view->dismiss = handler;
        view->dismiss_context = context;
        return;
    }
}

}  // namespace espos::keyboard
