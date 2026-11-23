#include "espos_keyboard/AdvancedTypingView.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <new>

#include "espos_keyboard/Keyboard.h"
#include "Utf8Fold.h"
#include "SuggestionText.h"
#include "Pinyin.h"

extern "C" {
LV_FONT_DECLARE(espos_keyboard_font)
}

namespace espos::keyboard::view {
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
    TypingView* next_live = nullptr;
    std::uint64_t instance = 0;
    lv_obj_t* root = nullptr;
    lv_obj_t* editor = nullptr;
    lv_obj_t* bar = nullptr;
    lv_obj_t* keyboard = nullptr;
    lv_obj_t* close = nullptr;
    lv_obj_t* preedit = nullptr;
    lv_obj_t* previous_page = nullptr;
    lv_obj_t* next_page = nullptr;
    lv_timer_t* timer = nullptr;
    SuggestionLookup lookup = nullptr;
    void* context = nullptr;
    WordBoundaryHandler boundary = nullptr;
    void* boundary_context = nullptr;
    DismissHandler dismiss = nullptr;
    void* dismiss_context = nullptr;
    bool processing_insert = false;
    bool right_to_left = false;
    bool direction_owned = false, direction_existed = false, applying_direction = false;
    std::uint64_t direction_order = 0;
    lv_style_value_t original_direction{}, applied_direction{};
    const lv_font_t* authored_font = nullptr;
    detail::PinyinComposition composition;
    std::size_t page = 0;
    std::uint32_t composition_caret = 0;
    std::uint32_t last_caret = 0;
    bool last_blocked = false;
    bool dirty = true;
    std::array<SuggestionSlot, kSuggestionCount> slots{};
    std::array<prediction::Suggestion, kSuggestionCount> suggestions{};
};

constexpr auto slot_count = static_cast<std::int32_t>(kSuggestionCount);
constexpr std::uint32_t suggestion_delay_ms = 45;
std::uint64_t next_instance = 0;
TypingView* live_views = nullptr;
std::uint64_t next_direction_order = 0;
void on_delete(lv_event_t* event);
bool han_tokens(const TypingView& view) { return !std::strcmp(keyboard_language(view.keyboard), "zh-CN"); }
bool pinyin(const TypingView& view) { return LV_ADVANCED_KEYBOARD_USE_PINYIN && han_tokens(view); }
TypingView* data(lv_obj_t* root)
{
    if (!root) return nullptr;
    for (std::uint32_t i = 0; i < lv_obj_get_event_count(root); ++i) {
        auto* descriptor = lv_obj_get_event_dsc(root, i);
        if (lv_event_dsc_get_cb(descriptor) == on_delete) return static_cast<TypingView*>(lv_event_dsc_get_user_data(descriptor));
    }
    return nullptr;
}
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

TypingView* direction_owner(lv_obj_t* editor)
{
    TypingView* owner = nullptr;
    for (auto* view = live_views; view; view = view->next_live) if (view->editor == editor && view->direction_owned &&
        (!owner || view->direction_order > owner->direction_order)) owner = view;
    return owner;
}
void restore_direction(TypingView& view)
{
    if (!view.direction_owned || !view.editor) return;
    const auto existed = view.direction_existed; const auto value = view.original_direction; auto* editor = view.editor;
    view.direction_owned = false;
    if (auto* other = direction_owner(editor)) lv_obj_set_local_style_prop(editor, LV_STYLE_BASE_DIR, other->applied_direction, LV_PART_MAIN);
    else if (existed) lv_obj_set_local_style_prop(editor, LV_STYLE_BASE_DIR, value, LV_PART_MAIN);
    else lv_obj_remove_local_style_prop(editor, LV_STYLE_BASE_DIR, LV_PART_MAIN);
}
bool apply_direction(TypingView& view)
{
    if (!view.editor) return true;
    auto* root = view.root; const auto instance = view.instance; auto* editor = view.editor;
    if (!view.direction_owned) {
        if (const auto* other = direction_owner(editor)) { view.direction_existed = other->direction_existed; view.original_direction = other->original_direction; }
        else view.direction_existed = lv_obj_get_local_style_prop(editor, LV_STYLE_BASE_DIR, &view.original_direction, LV_PART_MAIN) == LV_STYLE_RES_FOUND;
        view.direction_owned = true;
    }
    view.direction_order = ++next_direction_order;
    view.applying_direction = true; view.applied_direction.num = view.right_to_left ? LV_BASE_DIR_RTL : LV_BASE_DIR_LTR;
    lv_obj_set_local_style_prop(editor, LV_STYLE_BASE_DIR, view.applied_direction, LV_PART_MAIN);
    auto* current = live_view(root, instance); if (!current || current->editor != editor) return false;
    current->applying_direction = false; return true;
}
void on_direction_style(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    if (!view.direction_owned || view.applying_direction || !view.editor || direction_owner(view.editor) != &view) return;
    lv_style_value_t value{}; const bool existed = lv_obj_get_local_style_prop(view.editor, LV_STYLE_BASE_DIR, &value, LV_PART_MAIN) == LV_STYLE_RES_FOUND;
    if (existed && value.num == view.applied_direction.num) return;
    // Inspector edits remain authored even while editing direction is derived.
    for (auto* current = live_views; current; current = current->next_live) if (current->editor == view.editor && current->direction_owned) {
        current->direction_existed = existed; current->original_direction = value;
    }
    apply_direction(view);
}

void layout(TypingView& view, std::int32_t width, std::int32_t height)
{
    const bool ime = pinyin(view);
    const auto bar_height = std::clamp<std::int32_t>(ime ? 64 : 46, 1, height - 1);
    const auto preedit_height = ime ? std::min<std::int32_t>(18, bar_height / 2) : 0;
    const auto keyboard_height = height - bar_height;
    const auto close_width = std::min<std::int32_t>(std::max<std::int32_t>(1, width - slot_count),
        std::max<std::int32_t>(32, std::min<std::int32_t>(46, bar_height)));
    const auto page_width = ime ? std::min<std::int32_t>(28, (width - close_width) / 5) : 0;
    const auto suggestions_width = width - close_width - page_width * 2;
    lv_obj_set_pos(view.bar, 0, 0);
    lv_obj_set_size(view.bar, width, bar_height);
    lv_obj_set_pos(view.keyboard, 0, height - keyboard_height);
    lv_obj_set_size(view.keyboard, width, keyboard_height);
    for (std::int32_t index = 0; index < slot_count; ++index) {
        const auto left = page_width + index * suggestions_width / slot_count;
        const auto right = page_width + (index + 1) * suggestions_width / slot_count;
        const auto gap = index + 1 < slot_count && right - left > 1 ? 1 : 0;
        lv_obj_set_pos(view.slots[index].button, left, preedit_height);
        lv_obj_set_size(view.slots[index].button, right - left - gap, bar_height - preedit_height);
    }
    if (view.preedit) {
        lv_obj_set_hidden(view.preedit, !ime); lv_obj_set_hidden(view.previous_page, !ime); lv_obj_set_hidden(view.next_page, !ime);
        lv_obj_set_pos(view.preedit, 5, 0); lv_obj_set_size(view.preedit, std::max<std::int32_t>(1, width - close_width - 5), preedit_height);
        lv_obj_set_pos(view.previous_page, 0, preedit_height); lv_obj_set_size(view.previous_page, page_width, bar_height - preedit_height);
        lv_obj_set_pos(view.next_page, page_width + suggestions_width, preedit_height); lv_obj_set_size(view.next_page, page_width, bar_height - preedit_height);
    }
    lv_obj_set_pos(view.close, width - close_width, 0);
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

void clear_composition(TypingView& view)
{
    view.composition.Clear(); view.page = 0;
    if (view.preedit) lv_label_set_text_static(view.preedit, "Pinyin");
    if (view.previous_page) lv_obj_add_state(view.previous_page, LV_STATE_DISABLED);
    if (view.next_page) lv_obj_add_state(view.next_page, LV_STATE_DISABLED);
}
void present_composition(TypingView& view)
{
    if (view.editor && view.composition.Spelling()[0] && lv_textarea_get_cursor_pos(view.editor) != view.composition_caret) clear_composition(view);
    const auto count = view.composition.CandidateCount();
    view.page = count ? (std::min)(view.page, (count - 1) / kSuggestionCount) : 0;
    char caption[24]; std::snprintf(caption, sizeof caption, "Pinyin: %s", view.composition.Spelling());
    lv_label_set_text(view.preedit, view.composition.Spelling()[0] ? caption : "Pinyin");
    for (std::size_t i = 0; i < kSuggestionCount; ++i) {
        char word[5]; const bool found = view.composition.Candidate(view.page * kSuggestionCount + i, word);
        if (found) std::memcpy(view.slots[i].word.data(), word, std::strlen(word) + 1);
        show_slot(view.slots[i], found ? std::strlen(word) : 0);
    }
    lv_obj_set_state(view.previous_page, LV_STATE_DISABLED, view.page == 0);
    lv_obj_set_state(view.next_page, LV_STATE_DISABLED, (view.page + 1) * kSuggestionCount >= count);
    if (view.editor) view.last_caret = lv_textarea_get_cursor_pos(view.editor);
    view.last_blocked = false; view.dirty = !view.composition.Spelling()[0];
}
bool commit_composition(TypingView& view)
{
    if (!view.composition.Spelling()[0]) return true;
    if (view.editor && lv_textarea_get_cursor_pos(view.editor) != view.composition_caret) { clear_composition(view); clear(view); return true; }
    char text[8];
    if (!view.composition.Candidate(view.page * kSuggestionCount, text)) std::memcpy(text, view.composition.Spelling(), std::strlen(view.composition.Spelling()) + 1);
    clear_composition(view); clear(view);
    if (!view.editor || lv_textarea_get_password_mode(view.editor)) return true;
    auto* root = view.root; const auto instance = view.instance;
    view.processing_insert = true; lv_textarea_add_text(view.editor, text);
    auto* current = live_view(root, instance); if (!current) return false;
    current->processing_insert = false; current->dirty = true; return true;
}

void refresh(lv_timer_t* timer)
{
    auto& view = *static_cast<TypingView*>(lv_timer_get_user_data(timer));
    if (!view.editor) { clear_composition(view); clear(view); return; }
    const auto caret = lv_textarea_get_cursor_pos(view.editor);
    const auto blocked = lv_textarea_text_is_selected(view.editor) || lv_textarea_get_password_mode(view.editor);
    if (!view.dirty && caret == view.last_caret && blocked == view.last_blocked) return;

    view.dirty = false;
    view.last_caret = caret;
    view.last_blocked = blocked;
    if (pinyin(view)) {
        if (blocked || (view.composition.Spelling()[0] && caret != view.composition_caret)) clear_composition(view);
        if (blocked) { clear(view); return; }
        if (view.composition.Spelling()[0]) { present_composition(view); return; }
        clear_composition(view);
    }
    if (view.lookup == nullptr || blocked) {
        clear(view);
        return;
    }

    const auto input = detail::capture_suggestion_text(view.editor, han_tokens(view));
    std::array<char, kSuggestionBytes> prefix_buffer, previous_buffer, older_buffer;
    std::string_view prefix, previous, older;
    const auto* language = keyboard_language(view.keyboard);
    const bool listed = !std::strcmp(language,"fr-FR") || !std::strcmp(language,"de-DE") || !std::strcmp(language,"el-GR");
    if (!prediction::PrepareKeyboardQuery(input.prefix, prefix_buffer.data(), prefix_buffer.size(), prefix, language, false)) {
        clear(view);
        return;
    }
    if (prediction::PrepareKeyboardQuery(input.previous, previous_buffer.data(), previous_buffer.size(), previous, language, true)) {
        (void)prediction::PrepareKeyboardQuery(input.older, older_buffer.data(), older_buffer.size(), older, language, true);
    }
    const auto count = std::min(view.lookup(view.context, prefix, previous, older, view.suggestions.data()),
                                kSuggestionCount);
    for (std::size_t index = 0; index < kSuggestionCount; ++index) {
        auto& slot = view.slots[index];
        const auto& suggestion = view.suggestions[index];
        if (index < count && suggestion.length <= suggestion.text.size()) {
            show_slot(slot, listed ? detail::display_suggestion(input, suggestion.Word(), slot.word.data(), slot.word.size(), prefix)
                                   : detail::display_suggestion(input, suggestion.Word(), slot.word.data(), slot.word.size()));
        } else {
            show_slot(slot, 0);
        }
    }
}

void on_edit(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    view.dirty = true;
    if (!view.processing_insert) clear_composition(view);
    clear(view);
    lv_timer_reset(view.timer);
    if (!view.processing_insert) lv_obj_send_event(view.root, LV_EVENT_VALUE_CHANGED, nullptr);
}

constexpr std::size_t replacement_bytes = kSuggestionBytes + 16;
void delete_replacement(lv_event_t* event) { lv_free(lv_event_get_user_data(event)); }
char* replacement_for(lv_obj_t* editor, bool create)
{
    // INSERT replacement text is consumed after callbacks return. The external
    // editor owns this buffer so deleting its keyboard cannot invalidate it.
    for (std::uint32_t i = 0; i < lv_obj_get_event_count(editor); ++i) {
        auto* descriptor = lv_obj_get_event_dsc(editor, i);
        if (lv_event_dsc_get_cb(descriptor) == delete_replacement) return static_cast<char*>(lv_event_dsc_get_user_data(descriptor));
    }
    if (!create) return nullptr;
    auto* bytes = static_cast<char*>(lv_malloc(replacement_bytes));
    if (!bytes) return nullptr;
    if (!lv_obj_add_event_cb(editor, delete_replacement, LV_EVENT_DELETE, bytes)) { lv_free(bytes); return nullptr; }
    bytes[0] = '\0'; return bytes;
}

void on_editor_insert(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    const auto* text = static_cast<const char*>(lv_event_get_param(event));
    if (!text || !text[0] || !pinyin(view) || view.processing_insert || text == replacement_for(view.editor, false)) return;
    if (lv_textarea_get_password_mode(view.editor) || lv_textarea_text_is_selected(view.editor)) { clear_composition(view); clear(view); return; }
    if (view.composition.Spelling()[0] && lv_textarea_get_cursor_pos(view.editor) != view.composition_caret) clear_composition(view);
    std::size_t length = 0; bool letters = true;
    while (text[length] && length + 9 < replacement_bytes) {
        const auto value = text[length];
        if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z'))) letters = false;
        ++length;
    }
    if (text[length]) return; // Large pastes remain ordinary external edits.
    if (letters) {
        if (!view.composition.Spelling()[0]) view.composition_caret = lv_textarea_get_cursor_pos(view.editor);
        const auto pending = std::strlen(view.composition.Spelling());
        if (pending + length >= 8) {
            // Preserve unrecognized/overlong romanization as literal text.
            auto* replacement = replacement_for(view.editor, true); if (!replacement) return;
            std::memcpy(replacement, view.composition.Spelling(), pending);
            std::memcpy(replacement + pending, text, length + 1);
            clear_composition(view); clear(view);
            lv_textarea_set_insert_replace(view.editor, replacement); return;
        }
        for (std::size_t i = 0; i < length; ++i) view.composition.Append(text[i]);
        view.page = 0; lv_textarea_set_insert_replace(view.editor, ""); present_composition(view); return;
    }
    if (length == 1 && view.composition.Spelling()[0] && static_cast<unsigned char>(text[0]) < 128) {
        if (static_cast<unsigned char>(text[0]) == LV_KEY_DEL) {
            view.composition.Backspace(); view.page = 0; lv_textarea_set_insert_replace(view.editor, ""); present_composition(view); return;
        }
        char chosen[8];
        if (!view.composition.Candidate(view.page * kSuggestionCount, chosen)) std::memcpy(chosen, view.composition.Spelling(), std::strlen(view.composition.Spelling()) + 1);
        auto* replacement = replacement_for(view.editor, true); if (!replacement) return;
        const auto size = std::strlen(chosen); std::memcpy(replacement, chosen, size);
        // Space selects a candidate. Other punctuation follows the committed text.
        replacement[size] = text[0] == ' ' ? '\0' : text[0]; replacement[size + 1] = '\0';
        clear_composition(view); clear(view); lv_textarea_set_insert_replace(view.editor, replacement);
    }
}

void on_page(lv_event_t* event)
{
    auto& view = *static_cast<TypingView*>(lv_event_get_user_data(event));
    if (!view.editor || lv_textarea_get_password_mode(view.editor) || !pinyin(view)) return;
    if (lv_event_get_target_obj(event) == view.previous_page) { if (view.page) --view.page; }
    else if ((view.page + 1) * kSuggestionCount < view.composition.CandidateCount()) ++view.page;
    present_composition(view);
}

bool correct_boundary(TypingView& view)
{
    if (pinyin(view)) return true;
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
        if (!commit_composition(view)) return;
    }
    else clear_composition(view);
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
    if (pinyin(view) && !lv_textarea_get_password_mode(view.editor) &&
        (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) && view.composition.Backspace()) {
        lv_event_stop_processing(event); view.page = 0; present_composition(view); return;
    }
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
    if (pinyin(view) && view.composition.Spelling()[0]) {
        const auto word = slot->word; auto* root = view.root; const auto instance = view.instance;
        clear_composition(view); clear(view); view.processing_insert = true;
        lv_textarea_add_text(view.editor, word.data());
        auto* current = live_view(root, instance); if (!current) return;
        current->processing_insert = false; current->dirty = true;
        lv_obj_send_event(root, LV_EVENT_VALUE_CHANGED, nullptr); return;
    }

    // Accepting the word emits a text edit event and clears the current suggestions.
    const auto word = slot->word;
    auto* keyboard = view.keyboard;
    if (detail::accept_suggestion(view.editor, detail::capture_suggestion_text(view.editor, han_tokens(view)),
                                  {word.data(), slot->length}, true, han_tokens(view)) && lv_obj_is_in_widget_tree(keyboard)) {
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
        if (pinyin(view) && !lv_textarea_get_password_mode(view.editor) && view.composition.Backspace()) { view.page = 0; present_composition(view); break; }
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
    view.direction_owned = false;
    clear_composition(view);
    view.dirty = true;
    clear(view);
    lv_timer_pause(view.timer);
}

void on_delete(lv_event_t* event)
{
    auto* view = static_cast<TypingView*>(lv_event_get_user_data(event));
    auto** link = &live_views; while (*link && *link != view) link = &(*link)->next_live; if (*link) *link = view->next_live;
    if (view->editor) {
        lv_obj_remove_event_cb_with_user_data(view->editor, on_edit, view);
        lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_delete, view);
        lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_key, view);
        lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_insert, view);
        lv_obj_remove_event_cb_with_user_data(view->editor, on_direction_style, view);
        restore_direction(*view);
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
    view->next_live = live_views; live_views = view;
    view->lookup = lookup;
    view->context = context;
    view->authored_font = style.font;

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
#if LV_ADVANCED_KEYBOARD_USE_PINYIN
    view->preedit = lv_label_create(view->bar);
    view->previous_page = create_key(view->bar, "<", 1, 1, suggestion_style);
    view->next_page = create_key(view->bar, ">", 1, 1, suggestion_style);
    if (!view->preedit || !view->previous_page || !view->next_page ||
        !lv_obj_add_event_cb(view->previous_page, on_page, LV_EVENT_CLICKED, view) ||
        !lv_obj_add_event_cb(view->next_page, on_page, LV_EVENT_CLICKED, view)) { lv_obj_delete(root); return nullptr; }
    lv_label_set_text_static(view->preedit, "Pinyin"); lv_obj_set_clickable(view->preedit, false);
    lv_obj_set_style_text_font(view->preedit, &espos_keyboard_font, LV_PART_MAIN);
#endif
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
        const auto instance = view->instance; auto* old_editor = view->editor;
        restore_direction(*view);
        if (!live_view(root, instance) || view->editor != old_editor) return false;
        if (editor) {
            if (!lv_obj_add_event_cb(editor, on_edit, LV_EVENT_VALUE_CHANGED, view)) return false;
            // Drop the target before ordinary DELETE handlers detach bindings.
            // Restoring its derived direction during deletion would reenter
            // application style callbacks after their data has been released.
            if (!lv_obj_add_event_cb(editor, on_editor_delete, static_cast<lv_event_code_t>(LV_EVENT_DELETE | LV_EVENT_PREPROCESS), view)) {
                lv_obj_remove_event_cb_with_user_data(editor, on_edit, view);
                return false;
            }
            if (!lv_obj_add_event_cb(editor, on_editor_key, static_cast<lv_event_code_t>(LV_EVENT_KEY | LV_EVENT_PREPROCESS), view)) {
                lv_obj_remove_event_cb_with_user_data(editor, on_edit, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_delete, view);
                return false;
            }
            if (!lv_obj_add_event_cb(editor, on_editor_insert, static_cast<lv_event_code_t>(LV_EVENT_INSERT | LV_EVENT_PREPROCESS), view)) {
                lv_obj_remove_event_cb_with_user_data(editor, on_edit, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_delete, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_key, view); return false;
            }
            if (!lv_obj_add_event_cb(editor, on_direction_style, LV_EVENT_STYLE_CHANGED, view)) {
                lv_obj_remove_event_cb_with_user_data(editor, on_edit, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_delete, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_key, view);
                lv_obj_remove_event_cb_with_user_data(editor, on_editor_insert, view); return false;
            }
        }
        if (view->editor) {
            lv_obj_remove_event_cb_with_user_data(view->editor, on_edit, view);
            lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_delete, view);
            lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_key, view);
            lv_obj_remove_event_cb_with_user_data(view->editor, on_editor_insert, view);
            lv_obj_remove_event_cb_with_user_data(view->editor, on_direction_style, view);
        }
        view->editor = editor;
        clear_composition(*view);
        if (!apply_direction(*view)) return false;
        view->dirty = true;
        clear(*view);
        if (editor && (view->lookup || pinyin(*view))) lv_timer_resume(view->timer);
        else lv_timer_pause(view->timer);
        return true;
    }
    return false;
}

bool set_typing_view_language(lv_obj_t* root, const char* id)
{
    auto* view = data(root); if (!view || !id) return false;
    if (!std::strcmp(keyboard_language(view->keyboard), id)) return true;
    if (!set_keyboard_language(view->keyboard, id)) return false;
    clear_composition(*view); clear(*view); view->dirty = true;
    view->right_to_left = keyboard_language_right_to_left(id);
    const auto* font = view->authored_font ? view->authored_font : keyboard_language_font(id);
    for (auto& slot : view->slots) { lv_obj_set_style_text_font(slot.button, font, LV_PART_MAIN); lv_obj_set_height(slot.label, lv_font_get_line_height(font)); }
    const auto width = lv_obj_get_content_width(root), height = lv_obj_get_content_height(root);
    if (width >= slot_count && height >= 2) layout(*view, width, height);
    if (view->editor) {
        if (!apply_direction(*view)) return false;
        if (view->lookup || pinyin(*view)) lv_timer_resume(view->timer); else lv_timer_pause(view->timer);
        refresh(view->timer);
    }
    return true;
}

void set_typing_view_right_to_left(lv_obj_t* root, bool enabled)
{
    auto* view = data(root); if (!view) return; view->right_to_left = enabled;
    apply_direction(*view);
}
bool typing_view_right_to_left(lv_obj_t* root) { const auto* view = data(root); return view && view->right_to_left; }
void reset_typing_view_composition(lv_obj_t* root)
{
    auto* view = data(root); if (!view) return; clear_composition(*view); clear(*view); view->dirty = true;
}
bool authored_typing_view_style(lv_obj_t* editor, lv_style_prop_t property, bool& exists, lv_style_value_t& value)
{
    if (property != LV_STYLE_BASE_DIR) return false;
    for (auto* view = live_views; view; view = view->next_live) if (view->editor == editor && view->direction_owned) {
        exists = view->direction_existed; value = view->original_direction; return true;
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
        if ((lookup || pinyin(*view)) && view->editor) lv_timer_resume(view->timer);
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

}  // namespace espos::keyboard::view
