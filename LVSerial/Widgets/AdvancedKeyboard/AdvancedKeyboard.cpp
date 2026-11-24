#include "lv_advanced_keyboard.h"
#include "lv_advanced_keyboard_prediction.h"
#include "espos_keyboard/AdvancedTypingView.h"
#include "espos_keyboard/Keyboard.h"
#include "SuggestionText.h"
#include "Utf8Fold.h"
#include "KeyboardVisibility.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <new>
#include <vector>

extern "C" { LV_FONT_DECLARE(espos_keyboard_font) }
namespace {
struct Visibility;
struct Viewport {
    Visibility* owner = nullptr;
    Viewport* next = nullptr;
    lv_obj_t* object = nullptr;
    lv_point_t scroll{};
    int32_t height = 0;
};
struct Visibility {
    unsigned references = 1;
    Visibility* next = nullptr;
    lv_obj_t* root = nullptr;
    lv_obj_t* anchor = nullptr;
    lv_obj_t* common = nullptr;
    Viewport* viewports = nullptr;
    lv_point_t anchor_scroll{};
    bool floating = false, local_x = false, local_y = false;
    lv_style_value_t translate_x{}, translate_y{};
};
Visibility* visible_views = nullptr;
bool InTree(lv_obj_t* object) { return object && lv_obj_is_in_widget_tree(object); }
void ViewportEvent(lv_event_t* event)
{
    auto* viewport = static_cast<Viewport*>(lv_event_get_user_data(event));
    if (lv_event_get_code(event) == LV_EVENT_DELETE) viewport->object = nullptr;
    else if (lv_event_get_code(event) == LV_EVENT_GET_SELF_SIZE) {
        auto* size = static_cast<lv_point_t*>(lv_event_get_param(event));
        size->y = (std::max)(size->y, viewport->height);
    }
}
void AnchorEvent(lv_event_t* event)
{
    static_cast<Visibility*>(lv_event_get_user_data(event))->anchor = nullptr;
}
void ReleaseView(Visibility* view)
{
    if (!view || --view->references) return;
    for (auto* viewport = view->viewports; viewport; viewport = viewport->next)
        if (InTree(viewport->object)) lv_obj_remove_event_cb_with_user_data(viewport->object, ViewportEvent, viewport);
    if (InTree(view->anchor)) {
        auto* anchor = view->anchor;
        lv_obj_remove_event_cb_with_user_data(anchor, AnchorEvent, view);
        if (view->local_x) lv_obj_set_local_style_prop(anchor, LV_STYLE_TRANSLATE_X, view->translate_x, LV_PART_MAIN);
        else lv_obj_remove_local_style_prop(anchor, LV_STYLE_TRANSLATE_X, LV_PART_MAIN);
        if (InTree(anchor)) {
            if (view->local_y) lv_obj_set_local_style_prop(anchor, LV_STYLE_TRANSLATE_Y, view->translate_y, LV_PART_MAIN);
            else lv_obj_remove_local_style_prop(anchor, LV_STYLE_TRANSLATE_Y, LV_PART_MAIN);
        }
        if (InTree(anchor)) lv_obj_set_floating(anchor, view->floating);
    }
    for (auto* viewport = view->viewports; viewport; viewport = viewport->next) {
        auto* object = viewport->object;
        if (InTree(object)) lv_obj_scroll_by(object, lv_obj_get_scroll_x(object) - viewport->scroll.x,
            lv_obj_get_scroll_y(object) - viewport->scroll.y, LV_ANIM_OFF);
    }
    auto** link = &visible_views;
    while (*link && *link != view) link = &(*link)->next;
    if (*link) *link = view->next;
    while (view->viewports) { auto* item = view->viewports; view->viewports = item->next; delete item; }
    delete view;
}
struct ViewGuard {
    Visibility* view;
    explicit ViewGuard(Visibility* value) : view(value) { ++view->references; }
    ~ViewGuard() { ReleaseView(view); }
};
bool Within(lv_obj_t* object, lv_obj_t* ancestor)
{
    for (; object; object = lv_obj_get_parent(object)) if (object == ancestor) return true;
    return false;
}
void Reveal(Visibility& view, lv_obj_t* field)
{
    ViewGuard guard(&view);
    if (!InTree(view.root) || !InTree(field)) return;
    lv_obj_update_layout(view.root);
    if (!view.root || !InTree(field)) return;
    // Focus scrolling uses the full page; cancel it before reserving the
    // keyboard's smaller viewport. It must not animate over this correction.
    for (auto* parent = lv_obj_get_parent(field); parent;) {
        auto* next = lv_obj_get_parent(parent);
        lv_obj_stop_scroll_anim(parent);
        if (!view.root || !InTree(field) || (next && !InTree(next))) return;
        parent = next;
    }
    if (!view.common) {
        auto* common = lv_obj_get_parent(field);
        while (common && !Within(view.root, common)) common = lv_obj_get_parent(common);
        if (!common || common == view.root) return;
        view.common = common;
        auto* anchor = view.root;
        while (lv_obj_get_parent(anchor) != common) anchor = lv_obj_get_parent(anchor);
        view.anchor = anchor;
        view.floating = lv_obj_is_floating(anchor);
        view.anchor_scroll = {lv_obj_get_scroll_x(common), lv_obj_get_scroll_y(common)};
        view.local_x = lv_obj_get_local_style_prop(anchor, LV_STYLE_TRANSLATE_X, &view.translate_x, LV_PART_MAIN) == LV_STYLE_RES_FOUND;
        view.local_y = lv_obj_get_local_style_prop(anchor, LV_STYLE_TRANSLATE_Y, &view.translate_y, LV_PART_MAIN) == LV_STYLE_RES_FOUND;
        if (!lv_obj_add_event_cb(anchor, AnchorEvent, LV_EVENT_DELETE, &view)) { view.anchor = nullptr; return; }
        lv_area_t before{}; lv_obj_get_coords(anchor, &before);
        lv_obj_set_floating(anchor, true);
        lv_obj_update_layout(anchor);
        if (!view.root || !InTree(view.anchor) || !InTree(field)) return;
        lv_area_t after{}; lv_obj_get_coords(anchor, &after);
        auto tx = lv_obj_get_style_translate_x(anchor, LV_PART_MAIN);
        auto ty = lv_obj_get_style_translate_y(anchor, LV_PART_MAIN);
        if (LV_COORD_IS_PCT(tx)) tx = lv_obj_get_width(anchor) * LV_COORD_GET_PCT(tx) / 100;
        if (LV_COORD_IS_PCT(ty)) ty = lv_obj_get_height(anchor) * LV_COORD_GET_PCT(ty) / 100;
        lv_obj_set_style_translate_x(anchor, tx + before.x1 - after.x1, LV_PART_MAIN);
        lv_obj_set_style_translate_y(anchor, ty + before.y1 - after.y1, LV_PART_MAIN);
        lv_obj_update_layout(anchor);
        if (!view.root || !InTree(field)) return;
        Viewport** tail = &view.viewports;
        for (auto* parent = lv_obj_get_parent(field); parent; parent = lv_obj_get_parent(parent)) {
            if (parent == common || (lv_obj_is_scrollable(parent) && (lv_obj_get_scroll_dir(parent) & LV_DIR_VER))) {
                const auto bottom = lv_obj_get_scroll_bottom(parent);
                if (!view.root || !InTree(field) || !InTree(parent)) return;
                const auto content = lv_obj_get_content_height(parent);
                const auto scroll = lv_obj_get_scroll_y(parent);
                if (!view.root || !InTree(field) || !InTree(parent)) return;
                auto* viewport = new(std::nothrow) Viewport;
                if (!viewport) return;
                viewport->owner = &view; viewport->object = parent;
                viewport->scroll = {lv_obj_get_scroll_x(parent), scroll};
                viewport->height = content + (std::max)(int32_t{0}, scroll + bottom) + lv_obj_get_height(view.root) + 8;
                if (!lv_obj_add_event_cb(parent, ViewportEvent, LV_EVENT_ALL, viewport)) { delete viewport; return; }
                *tail = viewport; tail = &viewport->next;
            }
            if (parent == common) break;
        }
    }
    // A second pass handles a nested form whose viewport itself started below
    // the keyboard and was moved into view by its outer container.
    for (unsigned pass = 0; pass < 2; ++pass) for (auto* viewport = view.viewports; viewport; viewport = viewport->next) {
        auto* parent = viewport->object;
        if (!view.root || !InTree(field) || !InTree(parent)) return;
        lv_obj_update_layout(parent);
        if (!view.root || !InTree(field) || !InTree(parent)) return;
        lv_area_t visible{}, bounds{}, keyboard{};
        lv_obj_get_content_coords(parent, &visible);
        lv_obj_get_coords(field, &bounds); lv_obj_get_coords(view.root, &keyboard);
        for (auto* clip = lv_obj_get_parent(parent); clip; clip = lv_obj_get_parent(clip)) {
            if (!lv_obj_is_overflow_visible(clip)) {
                lv_area_t area{}; lv_obj_get_content_coords(clip, &area);
                visible.y1 = (std::max)(visible.y1, area.y1); visible.y2 = (std::min)(visible.y2, area.y2);
            }
        }
        visible.y1 = (std::max)(visible.y1, int32_t{0});
        visible.y2 = (std::min)(visible.y2, lv_display_get_vertical_resolution(lv_obj_get_display(parent)) - 1);
        if (bounds.x1 <= keyboard.x2 && bounds.x2 >= keyboard.x1) visible.y2 = (std::min)(visible.y2, keyboard.y1 - 9);
        if (visible.y2 < visible.y1) continue;
        const auto height = bounds.y2 - bounds.y1 + 1;
        int32_t dy = 0;
        if (height > visible.y2 - visible.y1 + 1 || bounds.y1 < visible.y1) dy = visible.y1 - bounds.y1;
        else if (bounds.y2 > visible.y2) dy = visible.y2 - bounds.y2;
        if (dy) lv_obj_scroll_by_bounded(parent, 0, dy, LV_ANIM_OFF);
        if (!view.root) return;
    }
}
struct State {
    uint64_t instance = 0;
    lv_obj_t* root = nullptr;
    lv_obj_t* target = nullptr;
    lv_timer_t* focus_timer = nullptr;
    Visibility* visibility = nullptr;
    std::vector<lv_obj_t*> fields;
    bool hide_on_defocus = false;
    bool dismissed = false;
    bool target_focused = false;
    bool keyboard_focused = false;
    lv_advanced_keyboard_predictor_t* predictor = nullptr;
    lv_advanced_keyboard_predictor_t* owned_predictor = nullptr;
    bool autocorrect = true;
    double autocorrect_threshold = 0.3;
    lv_color_t background = lv_color_white();
    lv_color_t foreground = lv_color_black();
    int32_t radius = 6;
    lv_advanced_keyboard_lookup_t lookup = nullptr;
    void* context = nullptr;
    const lv_font_t* styled_font = nullptr;
    lv_color_t styled_background{};
    lv_color_t styled_foreground{};
    int32_t styled_radius = -1;
};
uint64_t next_state = 0;
void FieldEvent(lv_event_t* event);
void RestoreView(State& state)
{
    auto* view = state.visibility;
    state.visibility = nullptr;
    if (view) { view->root = nullptr; ReleaseView(view); }
}
void Destroy(lv_event_t* event)
{
    auto* state = static_cast<State*>(lv_event_get_user_data(event));
    if (state->visibility && state->visibility->anchor == state->root) state->visibility->anchor = nullptr;
    RestoreView(*state);
    if (state->focus_timer) lv_timer_delete(state->focus_timer);
    for (auto* field : state->fields)
        lv_obj_remove_event_cb_with_user_data(field, FieldEvent, state);
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    lv_advanced_keyboard_predictor_destroy(state->owned_predictor);
#endif
    delete state;
}
State* Data(lv_obj_t* object)
{
    if (!object) return nullptr;
    for (uint32_t i=0;i<lv_obj_get_event_count(object);++i) {
        auto* descriptor=lv_obj_get_event_dsc(object,i);
        if (lv_event_dsc_get_cb(descriptor)==Destroy) return static_cast<State*>(lv_event_dsc_get_user_data(descriptor));
    }
    return nullptr;
}
bool Alive(lv_obj_t* root, uint64_t instance)
{
    auto* current = InTree(root) ? Data(root) : nullptr;
    return current && current->instance == instance;
}
const lv_font_t* Font(State& state)
{
    const auto* font=lv_obj_get_style_text_font(state.root,LV_PART_MAIN);
    return !font || font==LV_FONT_DEFAULT ? espos::keyboard::keyboard_language_font(espos::keyboard::keyboard_language(espos::keyboard::view::typing_view_keyboard(state.root))) : font;
}
void Style(State& state)
{
    auto* keyboard=espos::keyboard::view::typing_view_keyboard(state.root);
    const auto* font=Font(state);
    if (state.styled_font==font && state.styled_radius==state.radius &&
        lv_color_eq(state.background,state.styled_background) && lv_color_eq(state.foreground,state.styled_foreground)) return;
    state.styled_font=font;state.styled_radius=state.radius;
    state.styled_background=state.background;state.styled_foreground=state.foreground;
    constexpr lv_style_selector_t normal=LV_PART_MAIN;
    constexpr lv_style_selector_t pressed=normal | LV_STATE_PRESSED;
    for (uint32_t i=0;keyboard && i<lv_obj_get_child_count(keyboard);++i) {
        auto* button=lv_obj_get_child(keyboard,i);
        lv_obj_set_style_bg_color(button,state.background,normal);
        lv_obj_set_style_bg_color(button,state.foreground,pressed);
        lv_obj_set_style_text_color(button,state.foreground,normal);
        lv_obj_set_style_text_color(button,state.background,pressed);
        lv_obj_set_style_radius(button,state.radius,normal);
        lv_obj_set_style_text_font(button,font,normal);
    }
    auto* bar=lv_obj_get_child(state.root,0);
    for (uint32_t i=0;bar && i<lv_obj_get_child_count(bar);++i) {
        auto* button=lv_obj_get_child(bar,i);
        lv_obj_set_style_text_font(button,font,normal);
        if (auto* label=lv_obj_get_child(button,0)) lv_obj_set_height(label,lv_font_get_line_height(font));
    }
}
void CheckFocus(lv_timer_t* timer)
{
    auto* state = static_cast<State*>(lv_timer_get_user_data(timer));
    lv_timer_pause(timer);
    if (state->hide_on_defocus && !state->target_focused && !state->keyboard_focused)
        lv_obj_set_hidden(state->root, true);
}
void DeferHide(State& state)
{
    if (state.hide_on_defocus) {
        lv_timer_reset(state.focus_timer);
        lv_timer_resume(state.focus_timer);
    }
}
bool Activate(State& state, lv_obj_t* field, bool focused)
{
    auto* root = state.root;
    const auto instance = state.instance;
    if (field && lv_obj_has_state(field, LV_STATE_DISABLED)) return false;
    if (!espos::keyboard::view::set_typing_view_editor(state.root, field)) return false;
    if (!Alive(root, instance)) return false;
    if (state.target != field || !field) RestoreView(state);
    if (!Alive(root, instance)) return false;
    state.target = field;
    if (field) state.dismissed = false;
    state.target_focused = focused;
    state.keyboard_focused = false;
    lv_timer_pause(state.focus_timer);
    lv_obj_set_hidden(state.root, field == nullptr);
    if (!Alive(root, instance)) return false;
    if (field) {
        if (!state.visibility) {
            state.visibility = new(std::nothrow) Visibility;
            if (state.visibility) {
                state.visibility->root = state.root;
                state.visibility->next = visible_views; visible_views = state.visibility;
            }
        }
        if (state.visibility) Reveal(*state.visibility, field);
    }
    return true;
}
void Dismiss(void* context)
{
    auto& state = *static_cast<State*>(context);
    state.dismissed = true;
    Activate(state, nullptr, false);
}
void FieldEvent(lv_event_t* event)
{
    auto& state = *static_cast<State*>(lv_event_get_user_data(event));
    auto* field = lv_event_get_target_obj(event);
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        state.fields.erase(std::remove(state.fields.begin(), state.fields.end(), field), state.fields.end());
        if (state.target == field) Activate(state, nullptr, false);
    }
    else if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) Activate(state, field, true);
    else if (code == LV_EVENT_DEFOCUSED && state.target == field) {
        state.target_focused = false;
        DeferHide(state);
    }
}
void KeyFocus(lv_event_t* event)
{
    auto& state = *static_cast<State*>(lv_event_get_user_data(event));
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED) {
        state.keyboard_focused = true;
        if (state.target) lv_obj_set_hidden(state.root, false);
    }
    else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED) {
        state.keyboard_focused = false;
        DeferHide(state);
    }
}
void StyleChanged(lv_event_t* event)
{
    auto* state=static_cast<State*>(lv_event_get_user_data(event));
    if (lv_event_get_target_obj(event)==state->root) Style(*state);
}
bool ValidSuggestion(const lv_advanced_keyboard_suggestion_t& suggestion)
{
    const auto length=suggestion.length;
    if (length>LV_ADVANCED_KEYBOARD_SUGGESTION_BYTES) return false;
    // Callback output is a length-delimited UTF-8 view. Reject embedded NUL,
    // overlong encodings, surrogates and partial code points before LVGL reads it.
    const auto* bytes=reinterpret_cast<const unsigned char*>(suggestion.text);
    for (size_t i=0;i<length;) {
        const auto lead=bytes[i++];
        if (!lead) return false;
        if (lead<0x80) continue;
        unsigned count=0;
        uint32_t code=0;
        uint32_t minimum=0;
        if (lead>=0xc2 && lead<=0xdf) {count=1;code=lead&0x1f;minimum=0x80;}
        else if (lead>=0xe0 && lead<=0xef) {count=2;code=lead&0x0f;minimum=0x800;}
        else if (lead>=0xf0 && lead<=0xf4) {count=3;code=lead&0x07;minimum=0x10000;}
        else return false;
        if (count>length-i) return false;
        while (count--) {
            const auto next=bytes[i++];
            if ((next&0xc0)!=0x80) return false;
            code=(code<<6)|(next&0x3f);
        }
        if (code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) return false;
    }
    return true;
}
size_t PrefixBytes(const char* text,uint32_t characters)
{
    size_t bytes=0;
    for (uint32_t count=0;count<characters && text[bytes];++count) {
        ++bytes;
        while ((static_cast<unsigned char>(text[bytes])&0xc0)==0x80) ++bytes;
    }
    return bytes;
}
char* CopyPrefix(const char* text,size_t bytes)
{
    auto* copy=static_cast<char*>(lv_malloc(bytes+1));
    LV_ASSERT_MALLOC(copy);
    if (copy) { std::memcpy(copy,text,bytes);copy[bytes]='\0'; }
    return copy;
}
std::size_t Lookup(void* context,std::string_view prefix,std::string_view previous,std::string_view older,
                   espos::keyboard::prediction::Suggestion* output) noexcept
{
    auto* state=static_cast<State*>(context);
    std::array<lv_advanced_keyboard_suggestion_t,LV_ADVANCED_KEYBOARD_SUGGESTION_COUNT> candidates{};
    size_t count = 0;
    if (state->lookup)
        count=std::min(state->lookup(state->context,prefix.data(),prefix.size(),previous.data(),previous.size(),
            older.data(),older.size(),candidates.data(),candidates.size()),candidates.size());
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    else if (state->predictor) {
        count = lv_advanced_keyboard_predictor_suggest_folded(state->predictor, prefix.data(), prefix.size(),
            previous.data(), previous.size(), older.data(), older.size(), candidates.data(), candidates.size());
        if (!count && !prefix.empty()) {
            double probability = 0;
            if (lv_advanced_keyboard_predictor_autocorrect_folded(state->predictor, prefix.data(), prefix.size(),
                previous.data(), previous.size(), older.data(), older.size(), 0, &candidates[0], &probability)) count = 1;
        }
    }
#endif
    for (std::size_t i=0;i<count;++i) {
        if (!ValidSuggestion(candidates[i])) { output[i]={};continue; }
        output[i].length=candidates[i].length;
        output[i].mass=candidates[i].mass;
        std::memcpy(output[i].text.data(),candidates[i].text,candidates[i].length);
    }
    return count;
}
void UpdateLookup(State& state)
{
    espos::keyboard::view::set_typing_view_lookup(state.root, (state.lookup || state.predictor) ? Lookup : nullptr, &state);
}
void CorrectBoundary(void* context, lv_obj_t* editor)
{
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    auto& state = *static_cast<State*>(context);
    if (!state.autocorrect || !state.predictor || !editor) return;
    const auto* language = espos::keyboard::keyboard_language(espos::keyboard::view::typing_view_keyboard(state.root));
    const bool han = !std::strcmp(language, "zh-CN");
    const auto input = espos::keyboard::detail::capture_suggestion_text(editor, han);
    if (input.selected || input.prefix.empty() || input.prefix.size() != input.end - input.start) return;
    auto prefix = input.prefix, previous = input.previous, older = input.older;
    std::array<char, LV_ADVANCED_KEYBOARD_SUGGESTION_BYTES + 1> prefix_buffer{}, previous_buffer{}, older_buffer{};
    if (!espos::keyboard::prediction::PrepareKeyboardQuery(input.prefix, prefix_buffer.data(), prefix_buffer.size(), prefix, language, true)) return;
    if (!espos::keyboard::prediction::PrepareKeyboardQuery(input.previous, previous_buffer.data(), previous_buffer.size(), previous, language, true)) previous = {};
    if (!espos::keyboard::prediction::PrepareKeyboardQuery(input.older, older_buffer.data(), older_buffer.size(), older, language, true)) older = {};
    lv_advanced_keyboard_suggestion_t correction{};
    double probability = 0;
    if (!lv_advanced_keyboard_predictor_autocorrect_folded(state.predictor, prefix.data(), prefix.size(),
        previous.data(), previous.size(), older.data(), older.size(),
        state.autocorrect_threshold, &correction, &probability)) return;
    std::array<char, LV_ADVANCED_KEYBOARD_SUGGESTION_BYTES + 1> text{};
    const bool listed = !std::strcmp(language,"fr-FR") || !std::strcmp(language,"de-DE") || !std::strcmp(language,"el-GR");
    const auto length = listed ? espos::keyboard::detail::display_suggestion(input,
        {correction.text, correction.length}, text.data(), text.size(), prefix)
        : espos::keyboard::detail::display_suggestion(input,{correction.text,correction.length},text.data(),text.size());
    if (length) espos::keyboard::detail::accept_suggestion(editor, input, {text.data(), length}, false, han);
#else
    (void)context; (void)editor;
#endif
}
}
namespace espos::keyboard {
void ReadAuthoredKeyboardView(lv_obj_t* object, lv_point_t& position, lv_point_t& scroll, bool& floating)
{
    for (auto* view = visible_views; view; view = view->next) {
        if (view->anchor == object) {
            auto* parent = lv_obj_get_parent(object);
            if (parent) {
                position.x -= lv_obj_get_scroll_x(parent) - view->anchor_scroll.x;
                position.y -= lv_obj_get_scroll_y(parent) - view->anchor_scroll.y;
            }
            floating = view->floating;
        }
        for (auto* viewport = view->viewports; viewport; viewport = viewport->next)
            if (viewport->object == object) scroll = viewport->scroll;
    }
}
bool ReadAuthoredKeyboardStyle(lv_obj_t* object, lv_style_prop_t property, bool& exists, lv_style_value_t& value)
{
    if (espos::keyboard::view::authored_typing_view_style(object, property, exists, value)) return true;
    if (property != LV_STYLE_TRANSLATE_X && property != LV_STYLE_TRANSLATE_Y) return false;
    for (auto* view = visible_views; view; view = view->next) if (view->anchor == object) {
        exists = property == LV_STYLE_TRANSLATE_X ? view->local_x : view->local_y;
        value = property == LV_STYLE_TRANSLATE_X ? view->translate_x : view->translate_y;
        return true;
    }
    return false;
}
}
extern "C" {
lv_obj_t* lv_advanced_keyboard_create(lv_obj_t* parent)
{
    espos::keyboard::keyboard_view_query = espos::keyboard::ReadAuthoredKeyboardView;
    espos::keyboard::keyboard_style_query = espos::keyboard::ReadAuthoredKeyboardStyle;
    auto* root=espos::keyboard::view::create_en_us_typing_view(parent,600,240,nullptr,nullptr);
    if (!root) return nullptr;
    auto* state=new(std::nothrow) State;
    if (!state) { lv_obj_delete(root);return nullptr; }
    state->root=root;
    state->instance=++next_state;
    if (!lv_obj_add_event_cb(root,Destroy,LV_EVENT_DELETE,state)) { delete state;lv_obj_delete(root);return nullptr; }
    state->focus_timer = lv_timer_create(CheckFocus, 1, state);
    if (!state->focus_timer || !lv_obj_add_event_cb(root,StyleChanged,LV_EVENT_STYLE_CHANGED,state)) {
        lv_obj_delete(root);return nullptr;
    }
    lv_timer_pause(state->focus_timer);
    for (auto* container : {lv_obj_get_child(root, 0), lv_obj_get_child(root, 1)})
        for (uint32_t i = 0; i < lv_obj_get_child_count(container); ++i)
            if (!lv_obj_add_event_cb(lv_obj_get_child(container, i), KeyFocus, LV_EVENT_ALL, state)) {
                lv_obj_delete(root); return nullptr;
            }
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION && LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
    state->owned_predictor = lv_advanced_keyboard_predictor_create_builtin();
    if (!state->owned_predictor) { lv_obj_delete(root); return nullptr; }
    state->predictor = state->owned_predictor;
#endif
    espos::keyboard::view::set_typing_view_word_boundary_handler(root, CorrectBoundary, state);
    espos::keyboard::view::set_typing_view_dismiss_handler(root, Dismiss, state);
    UpdateLookup(*state);
    Style(*state);
    lv_obj_set_hidden(root, true);
    return root;
}
bool lv_advanced_keyboard_is_instance(lv_obj_t* object) { return Data(object)!=nullptr; }
lv_obj_t* lv_advanced_keyboard_get_textarea(lv_obj_t* object) { return Data(object) ? espos::keyboard::view::typing_view_editor(object) : nullptr; }
lv_obj_t* lv_advanced_keyboard_get_keyboard(lv_obj_t* object) { return Data(object) ? espos::keyboard::view::typing_view_keyboard(object) : nullptr; }
bool lv_advanced_keyboard_set_language(lv_obj_t* object, const char* language) {
    if (!Data(object) || !espos::keyboard::view::set_typing_view_language(object, language)) return false;
    Style(*Data(object)); return true;
}
const char* lv_advanced_keyboard_get_language(lv_obj_t* object) { return Data(object) ? espos::keyboard::keyboard_language(espos::keyboard::view::typing_view_keyboard(object)) : ""; }
size_t lv_advanced_keyboard_language_count(void) { return espos::keyboard::keyboard_language_count(); }
const char* lv_advanced_keyboard_language_id(size_t index) { return espos::keyboard::keyboard_language_id(index); }
const char* lv_advanced_keyboard_language_name(size_t index) { return espos::keyboard::keyboard_language_name(index); }
void lv_advanced_keyboard_set_right_to_left(lv_obj_t* object, bool enabled) { if (Data(object)) espos::keyboard::view::set_typing_view_right_to_left(object, enabled); }
bool lv_advanced_keyboard_get_right_to_left(lv_obj_t* object) { return Data(object) && espos::keyboard::view::typing_view_right_to_left(object); }
bool lv_advanced_keyboard_bind_textarea(lv_obj_t* object, lv_obj_t* field)
{
    auto* state = Data(object);
    if (!state || !field || !lv_obj_has_class(field, &lv_textarea_class)) return false;
    if (std::find(state->fields.begin(), state->fields.end(), field) != state->fields.end()) return true;
    try { state->fields.push_back(field); } catch (...) { return false; }
    if (!lv_obj_add_event_cb(field, FieldEvent, LV_EVENT_ALL, state)) {
        state->fields.pop_back(); return false;
    }
    return true;
}
bool lv_advanced_keyboard_set_textarea(lv_obj_t* object, lv_obj_t* field)
{
    auto* state = Data(object);
    if (!state || (field && !lv_advanced_keyboard_bind_textarea(object, field))) return false;
    return Activate(*state, field, field && lv_obj_has_state(field, LV_STATE_FOCUSED));
}
void lv_advanced_keyboard_close(lv_obj_t* object) { if (auto* state = Data(object)) Dismiss(state); }
bool lv_advanced_keyboard_is_dismissed(lv_obj_t* object) { auto* state = Data(object); return state && state->dismissed; }
void lv_advanced_keyboard_unbind_textarea(lv_obj_t* object, lv_obj_t* field)
{
    auto* state = Data(object);
    if (!state || !field) return;
    if (state->target == field && !Activate(*state, nullptr, false)) return;
    lv_obj_remove_event_cb_with_user_data(field, FieldEvent, state);
    state->fields.erase(std::remove(state->fields.begin(), state->fields.end(), field), state->fields.end());
}
void lv_advanced_keyboard_unbind_all(lv_obj_t* object)
{
    auto* state = Data(object);
    if (!state) return;
    if (!Activate(*state, nullptr, false)) return;
    for (auto* field : state->fields) lv_obj_remove_event_cb_with_user_data(field, FieldEvent, state);
    state->fields.clear();
}
size_t lv_advanced_keyboard_bound_textarea_count(lv_obj_t* object) { auto* state = Data(object); return state ? state->fields.size() : 0; }
lv_obj_t* lv_advanced_keyboard_bound_textarea(lv_obj_t* object, size_t index) { auto* state = Data(object); return state && index < state->fields.size() ? state->fields[index] : nullptr; }
void lv_advanced_keyboard_set_hide_on_defocus(lv_obj_t* object, bool enabled)
{
    if (auto* state = Data(object)) {
        state->hide_on_defocus = enabled;
        if (!enabled) lv_timer_pause(state->focus_timer);
    }
}
bool lv_advanced_keyboard_get_hide_on_defocus(lv_obj_t* object) { auto* state = Data(object); return state && state->hide_on_defocus; }
void lv_advanced_keyboard_set_predictor(lv_obj_t* object, lv_advanced_keyboard_predictor_t* predictor)
{
    if (auto* state = Data(object)) {
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
        if (state->owned_predictor && state->owned_predictor != predictor) {
            lv_advanced_keyboard_predictor_destroy(state->owned_predictor);
            state->owned_predictor = nullptr;
        }
#endif
        state->predictor = predictor;
        UpdateLookup(*state);
    }
}
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_get_predictor(lv_obj_t* object) { auto* state = Data(object); return state ? state->predictor : nullptr; }
void lv_advanced_keyboard_set_autocorrect(lv_obj_t* object, bool enabled) { if (auto* state = Data(object)) state->autocorrect = enabled; }
bool lv_advanced_keyboard_get_autocorrect(lv_obj_t* object) { auto* state = Data(object); return state && state->autocorrect; }
void lv_advanced_keyboard_set_autocorrect_threshold(lv_obj_t* object, double value) { if (auto* state = Data(object)) if (std::isfinite(value) && value >= 0 && value <= 1) state->autocorrect_threshold = value; }
double lv_advanced_keyboard_get_autocorrect_threshold(lv_obj_t* object) { auto* state = Data(object); return state ? state->autocorrect_threshold : 0.3; }
void lv_advanced_keyboard_set_text(lv_obj_t* object,const char* text)
{
    auto* entry=lv_advanced_keyboard_get_textarea(object);
    if (!entry) return;
    if (!text) text="";
    const auto limit=lv_textarea_get_max_length(entry);
    const auto bytes=limit?PrefixBytes(text,limit):0;
    if (limit && text[bytes]) {
        // Pre-limit the source as well as the label. Native textarea password
        // storage must receive the same bounded string that is displayed.
        if (auto* prefix=CopyPrefix(text,bytes)) { lv_textarea_set_text(entry,prefix);lv_free(prefix); }
    }
    else lv_textarea_set_text(entry,text);
}
const char* lv_advanced_keyboard_get_text(lv_obj_t* object) { auto* entry=lv_advanced_keyboard_get_textarea(object);return entry?lv_textarea_get_text(entry):""; }
void lv_advanced_keyboard_set_placeholder(lv_obj_t* object,const char* text) { if(auto* entry=lv_advanced_keyboard_get_textarea(object))lv_textarea_set_placeholder_text(entry,text?text:""); }
const char* lv_advanced_keyboard_get_placeholder(lv_obj_t* object) { auto* entry=lv_advanced_keyboard_get_textarea(object);const auto* text=entry?lv_textarea_get_placeholder_text(entry):nullptr;return text?text:""; }
void lv_advanced_keyboard_set_password(lv_obj_t* object,bool enabled) { if(auto* entry=lv_advanced_keyboard_get_textarea(object)){espos::keyboard::view::reset_typing_view_composition(object);lv_textarea_set_password_mode(entry,enabled);} }
bool lv_advanced_keyboard_get_password(lv_obj_t* object) { auto* entry=lv_advanced_keyboard_get_textarea(object);return entry && lv_textarea_get_password_mode(entry); }
void lv_advanced_keyboard_set_max_length(lv_obj_t* object,uint32_t length)
{
    auto* entry=lv_advanced_keyboard_get_textarea(object);
    if (!entry) return;
    const auto* text=lv_textarea_get_text(entry);
    const auto bytes=length?PrefixBytes(text,length):0;
    char* prefix=nullptr;
    if (length && text[bytes]) {
        // Allocate only the retained UTF-8 prefix, once; shrinking a large
        // document never needs a full-text copy or repeated character deletes.
        prefix=CopyPrefix(text,bytes);
        if (!prefix) return;
    }
    lv_textarea_set_max_length(entry,length);
    if (prefix) { lv_textarea_set_text(entry,prefix);lv_free(prefix); }
}
uint32_t lv_advanced_keyboard_get_max_length(lv_obj_t* object) { auto* entry=lv_advanced_keyboard_get_textarea(object);return entry?lv_textarea_get_max_length(entry):0; }
void lv_advanced_keyboard_set_key_background(lv_obj_t* object,lv_color_t color) { if(auto* state=Data(object)){state->background=color;Style(*state);} }
lv_color_t lv_advanced_keyboard_get_key_background(lv_obj_t* object) { auto* state=Data(object);return state?state->background:lv_color_white(); }
void lv_advanced_keyboard_set_key_foreground(lv_obj_t* object,lv_color_t color) { if(auto* state=Data(object)){state->foreground=color;Style(*state);} }
lv_color_t lv_advanced_keyboard_get_key_foreground(lv_obj_t* object) { auto* state=Data(object);return state?state->foreground:lv_color_black(); }
void lv_advanced_keyboard_set_key_radius(lv_obj_t* object,int32_t radius) { if(auto* state=Data(object)){state->radius=std::clamp<int32_t>(radius,0,32767);Style(*state);} }
int32_t lv_advanced_keyboard_get_key_radius(lv_obj_t* object) { auto* state=Data(object);return state?state->radius:6; }
void lv_advanced_keyboard_set_suggestion_lookup(lv_obj_t* object,lv_advanced_keyboard_lookup_t lookup,void* context)
{
    if(auto* state=Data(object)) {
        state->lookup=lookup;state->context=context;
        UpdateLookup(*state);
    }
}
}
