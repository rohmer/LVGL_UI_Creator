#include "EditorTextInput.h"
#include "EditorEdit.h"
#include "EditorFocus.h"
#include "EditorObjectRef.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <string>

#if LVGL_VERSION_MAJOR >= 9
namespace {
lv_obj_t* TextField(lv_group_t* group) {
    auto* field = group ? lv_group_get_focused(group) : nullptr;
    if (!field || !lv_obj_has_class(field, &lv_textarea_class) || lv_obj_has_class(field, &lv_spinbox_class)) return nullptr;
    if (!IsEditorControlAvailable(field)) return nullptr;
    return field;
}
const char* CharacterAt(const char* text, uint32_t index) {
    while (*text && index--) {
        ++text;
        while ((static_cast<unsigned char>(*text) & 0xc0) == 0x80) ++text;
    }
    return text;
}
uint32_t Characters(const char* text) {
    uint32_t count = 0;
    for (; *text; ++text) if ((static_cast<unsigned char>(*text) & 0xc0) != 0x80) ++count;
    return count;
}
bool Selection(lv_obj_t* field, uint32_t& start, uint32_t& end) {
    auto* label = lv_textarea_get_label(field);
    start = lv_label_get_text_selection_start(label);
    end = lv_label_get_text_selection_end(label);
    if (start == LV_LABEL_TEXT_SELECTION_OFF || end == LV_LABEL_TEXT_SELECTION_OFF || start == end) return false;
    if (start > end) std::swap(start, end);
    const auto count = Characters(lv_textarea_get_text(field));
    start = (std::min)(start, count); end = (std::min)(end, count);
    return start < end;
}
bool Replace(lv_obj_t* field, const char* replacement) {
    uint32_t start = 0, end = 0;
    if (!Selection(field, start, end)) return false;
    const char* text = lv_textarea_get_text(field);
    std::string updated(text, CharacterAt(text, start));
    const auto prefixBytes = updated.size();
    updated += replacement;
    const auto replacementBytes = updated.size() - prefixBytes;
    updated += CharacterAt(text, end);
    EditorObjectRef alive(field);
    EditorEdit::Scope edit(field, true);
    lv_textarea_set_text(field, updated.c_str());
    if (alive.Get()) {
        // Accepted-character and length constraints remain enforced by LVGL.
        const auto position = start + Characters(std::string(replacement, replacementBytes).c_str());
        lv_textarea_set_cursor_pos(field, (std::min)(position, Characters(lv_textarea_get_text(field))));
        lv_textarea_clear_selection(field);
    }
    return true;
}
}

bool HandleEditorTextShortcut(lv_group_t* group, int key) {
    auto* field = TextField(group);
    if (!field || (key != 'a' && key != 'c' && key != 'x' && key != 'v')) return false;
    if (key == 'a') {
        lv_textarea_set_text_selection(field, true);
        lv_textarea_set_cursor_pos(field, LV_TEXTAREA_CURSOR_LAST);
        auto* label = lv_textarea_get_label(field);
        lv_label_set_text_selection_start(label, 0);
        lv_label_set_text_selection_end(label, Characters(lv_textarea_get_text(field)));
    } else if (key == 'v') {
        char* clipboard = SDL_GetClipboardText();
        if (clipboard) {
            const std::string text(clipboard); SDL_free(clipboard);
            if (!text.empty() && !Replace(field, text.c_str())) {
                EditorEdit::Scope edit(field, true);
                lv_textarea_add_text(field, text.c_str());
            }
        }
    } else if (!lv_textarea_get_password_mode(field)) {
        uint32_t start = 0, end = 0;
        if (Selection(field, start, end)) {
            const char* text = lv_textarea_get_text(field);
            const std::string selected(CharacterAt(text, start), CharacterAt(text, end));
            if (SDL_SetClipboardText(selected.c_str()) && key == 'x') Replace(field, "");
        }
    }
    return true;
}

bool ReplaceEditorTextSelection(lv_group_t* group, const char* text) {
    auto* field = TextField(group);
    return field && text && Replace(field, text);
}
bool DeleteEditorTextSelection(lv_group_t* group) {
    auto* field = TextField(group);
    return field && Replace(field, "");
}
void ClearEditorTextSelection(lv_group_t* group) {
    if(auto* field=TextField(group))lv_textarea_clear_selection(field);
}
#endif
