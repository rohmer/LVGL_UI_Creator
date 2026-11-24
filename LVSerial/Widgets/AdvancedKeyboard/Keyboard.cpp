#include "espos_keyboard/Keyboard.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>

#include "LayoutData.h"
#include "en_us_qwerty.generated.h"
#include "es_es_qwerty.generated.h"
#include "fr_fr_azerty.generated.h"
#include "de_de_qwertz.generated.h"
#include "it_it_qwerty.generated.h"
#include "pt_pt_qwerty.generated.h"
#include "zh_cn_pinyin.generated.h"
#include "ar_sa_arabic.generated.h"
#include "hi_in_inscript.generated.h"
#include "bn_bd_inscript.generated.h"
#include "id_id_qwerty.generated.h"
#include "ur_pk_phonetic.generated.h"
#include "ru_ru_jcuken.generated.h"
#include "el_gr_qwerty.generated.h"

#ifndef LV_ADVANCED_KEYBOARD_USE_HINDI_FONT
#define LV_ADVANCED_KEYBOARD_USE_HINDI_FONT 0
#endif
#ifndef LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT
#define LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT 0
#endif
#ifndef LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT
#define LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT 0
#endif
#ifndef LV_ADVANCED_KEYBOARD_USE_URDU_FONT
#define LV_ADVANCED_KEYBOARD_USE_URDU_FONT 0
#endif
#ifndef LV_ADVANCED_KEYBOARD_USE_GREEK_FONT
#define LV_ADVANCED_KEYBOARD_USE_GREEK_FONT 0
#endif

extern "C" {
LV_FONT_DECLARE(espos_keyboard_font)
#if LV_ADVANCED_KEYBOARD_USE_HINDI_FONT
LV_FONT_DECLARE(espos_keyboard_hindi_font)
#endif
#if LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT
LV_FONT_DECLARE(espos_keyboard_bengali_font)
#endif
#if LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT
LV_FONT_DECLARE(espos_keyboard_cyrillic_font)
#endif
#if LV_ADVANCED_KEYBOARD_USE_URDU_FONT
LV_FONT_DECLARE(espos_keyboard_urdu_font)
#endif
#if LV_ADVANCED_KEYBOARD_USE_GREEK_FONT
LV_FONT_DECLARE(espos_keyboard_greek_font)
#endif
}

namespace espos::keyboard {
namespace {

using detail::Action;
using detail::ActionType;
using detail::Key;
using detail::Layout;
using detail::en_us_qwerty;
using detail::no_layout;

struct Language {
    const char* id;
    const char* name;
    const detail::LayoutDefinition* definition;
};
constexpr Language languages[] = {
    {"en-US", "English (US)", &detail::en_us_qwerty},
    {"es-ES", "Spanish", &detail::es_es_qwerty},
    {"fr-FR", "French", &detail::fr_fr_azerty},
    {"de-DE", "German", &detail::de_de_qwertz},
    {"it-IT", "Italian", &detail::it_it_qwerty},
    {"pt-PT", "Portuguese", &detail::pt_pt_qwerty},
    {"zh-CN", "Mandarin (Pinyin)", &detail::zh_cn_pinyin},
    {"ar-SA", "Arabic", &detail::ar_sa_arabic},
    {"hi-IN", "Hindi", &detail::hi_in_inscript},
    {"bn-BD", "Bengali", &detail::bn_bd_inscript},
    {"id-ID", "Indonesian", &detail::id_id_qwerty},
    {"ur-PK", "Urdu", &detail::ur_pk_phonetic},
    {"ru-RU", "Russian", &detail::ru_ru_jcuken},
    {"el-GR", "Greek", &detail::el_gr_qwerty},
};
constexpr std::size_t max_keys = (std::max)({detail::en_us_qwerty.max_keys, detail::es_es_qwerty.max_keys,
    detail::fr_fr_azerty.max_keys, detail::de_de_qwertz.max_keys,
    detail::it_it_qwerty.max_keys, detail::pt_pt_qwerty.max_keys,
    detail::zh_cn_pinyin.max_keys, detail::ar_sa_arabic.max_keys, detail::hi_in_inscript.max_keys,
    detail::bn_bd_inscript.max_keys, detail::id_id_qwerty.max_keys, detail::ur_pk_phonetic.max_keys, detail::ru_ru_jcuken.max_keys,
    detail::el_gr_qwerty.max_keys});

constexpr std::uint32_t double_tap_ms = 300;
constexpr std::uint32_t variant_hold_ms = 1000;
constexpr std::int32_t key_gap = 4;

enum class ShiftMode { Off, OneShot, Locked };

struct Slot {
    lv_obj_t* button = nullptr;
    lv_obj_t* label = nullptr;
    const Key* key = nullptr;
};

struct KeyboardView {
    lv_obj_t* container = nullptr;
    lv_timer_t* tap_timer = nullptr;
    lv_timer_t* hold_timer = nullptr;
    lv_obj_t* held_button = nullptr;
    lv_obj_t* suppress_click = nullptr;
    lv_obj_t* variant_overlay = nullptr;
    lv_obj_t* variant_row = nullptr;
    lv_obj_t* drag_source = nullptr;
    lv_obj_t* highlighted_variant = nullptr;
    bool drag_scroll_chain_hor = false;
    bool drag_scroll_chain_ver = false;
    const Key* variant_key = nullptr;
    KeyStyle key_style{};
    const Key* pending_key = nullptr;
    std::uint32_t pending_since = 0;
    std::size_t base_layout = en_us_qwerty.initial_layout;
    const Language* language = &languages[0];
    const lv_font_t* authored_font = nullptr;
    ShiftMode shift = ShiftMode::Off;
    std::array<Slot, max_keys> slots{};
};

void end_variant_drag(KeyboardView& view)
{
    if (view.highlighted_variant) lv_obj_remove_state(view.highlighted_variant, LV_STATE_PRESSED);
    view.highlighted_variant = nullptr;
    if (view.drag_source) {
        lv_obj_set_press_lock(view.drag_source, false);
        lv_obj_set_scroll_chain_hor(view.drag_source, view.drag_scroll_chain_hor);
        lv_obj_set_scroll_chain_ver(view.drag_source, view.drag_scroll_chain_ver);
        view.drag_source = nullptr;
    }
}

void dismiss_variants(KeyboardView& view)
{
    end_variant_drag(view);
    view.held_button = nullptr;
    if (view.hold_timer) lv_timer_pause(view.hold_timer);
    auto* overlay = view.variant_overlay;
    view.variant_overlay = nullptr;
    view.variant_row = nullptr;
    view.variant_key = nullptr;
    if (overlay) lv_obj_delete(overlay);
}

const Layout& active_layout(const KeyboardView& view)
{
    const auto& definition = *view.language->definition;
    const Layout& base = definition.layouts[view.base_layout];
    return view.shift != ShiftMode::Off && base.shifted != no_layout
        ? definition.layouts[base.shifted] : base;
}

const Key* find_key(const Layout& layout, const char* id)
{
    for (std::size_t row = 0; row < layout.row_count; ++row) {
        const auto& definition = layout.rows[row];
        for (std::size_t key = 0; key < definition.key_count; ++key) {
            if (std::strcmp(definition.keys[key].id, id) == 0) {
                return &definition.keys[key];
            }
        }
    }
    return nullptr;
}

void position_key(Slot& slot, const Key& key, ShiftMode shift, std::int32_t left,
                  std::int32_t top, std::int32_t width, std::int32_t height)
{
    if (width <= 0 || height <= 0) {
        slot.key = nullptr;
        lv_obj_set_hidden(slot.button, true);
        return;
    }

    // Divide neighboring keys at a common edge, then inset to leave a gap.
    const auto horizontal_gap = std::min(key_gap, width - 1);
    const auto vertical_gap = std::min(key_gap, height - 1);
    lv_obj_set_pos(slot.button, left + horizontal_gap / 2, top + vertical_gap / 2);
    lv_obj_set_size(slot.button, width - horizontal_gap, height - vertical_gap);

    const char* label = shift == ShiftMode::Locked && key.tap.type == ActionType::Shift
        ? "\xe2\x87\xaa" : key.label;  // ⇪
    if (lv_label_get_text(slot.label) != label) {
        lv_label_set_text_static(slot.label, label);
    }
    slot.key = &key;
    lv_obj_set_hidden(slot.button, false);
}

void refresh(KeyboardView& view, std::int32_t width, std::int32_t height)
{
    dismiss_variants(view);
    const auto& layout = active_layout(view);
    const double horizontal_scale = static_cast<double>(width) / view.language->definition->width;
    const double vertical_scale = static_cast<double>(height) / layout.row_count;
    std::size_t slot = 0;

    for (std::size_t row = 0; row < layout.row_count; ++row) {
        const auto& definition = layout.rows[row];
        const auto top = static_cast<std::int32_t>(row * vertical_scale + 0.5);
        const auto bottom = static_cast<std::int32_t>((row + 1) * vertical_scale + 0.5);
        double position = definition.offset;
        for (std::size_t index = 0; index < definition.key_count; ++index) {
            const auto& key = definition.keys[index];
            const auto left = static_cast<std::int32_t>(position * horizontal_scale + 0.5);
            position += key.width;
            const auto right = static_cast<std::int32_t>(position * horizontal_scale + 0.5);
            position_key(view.slots[slot++], key, view.shift, left, top, right - left, bottom - top);
        }
    }
    while (slot < view.slots.size()) {
        auto& unused = view.slots[slot++];
        unused.key = nullptr;
        lv_obj_set_hidden(unused.button, true);
    }
}

void refresh(KeyboardView& view)
{
    refresh(view, lv_obj_get_content_width(view.container), lv_obj_get_content_height(view.container));
}

void apply(KeyboardView& view, const Action& action)
{
    const bool can_shift = view.language->definition->layouts[view.base_layout].shifted != no_layout;
    switch (action.type) {
    case ActionType::Insert:
        if (view.shift == ShiftMode::OneShot) {
            view.shift = ShiftMode::Off;
        }
        break;
    case ActionType::SwitchLayout:
        view.base_layout = action.layout;
        view.shift = ShiftMode::Off;
        break;
    case ActionType::Shift:
        if (can_shift) {
            view.shift = view.shift == ShiftMode::Off ? ShiftMode::OneShot : ShiftMode::Off;
        }
        break;
    case ActionType::ShiftLock:
        if (can_shift) {
            view.shift = view.shift == ShiftMode::Locked ? ShiftMode::Off : ShiftMode::Locked;
        }
        break;
    case ActionType::None:
    case ActionType::Backspace:
    case ActionType::Enter:
        break;
    }
}

void clear_pending(KeyboardView& view)
{
    view.pending_key = nullptr;
    lv_timer_pause(view.tap_timer);
}

const Action* flush_pending(KeyboardView& view)
{
    if (const Key* pending = view.pending_key) {
        clear_pending(view);
        apply(view, pending->tap);
        return &pending->tap;
    }
    return nullptr;
}

lv_result_t send_key_input(lv_obj_t* keyboard, const Action* action)
{
    if (action == nullptr) return LV_RESULT_OK;
    KeyInput input{};
    switch (action->type) {
    case ActionType::Insert:
        input = {KeyInputKind::Insert, action->text};
        break;
    case ActionType::Backspace:
        input = {KeyInputKind::Backspace, nullptr};
        break;
    case ActionType::Enter:
        input = {KeyInputKind::Enter, nullptr};
        break;
    default:
        return LV_RESULT_OK;
    }
    return lv_obj_send_event(keyboard, LV_EVENT_VALUE_CHANGED, &input);
}

void select_variant(KeyboardView& view, std::size_t index)
{
    if (!view.variant_key || index >= view.variant_key->variant_count) return;
    if (!lv_obj_is_visible(view.container)) { dismiss_variants(view); return; }
    // The strings live in the generated definition, beyond the popup's lifetime.
    const Action action{ActionType::Insert, view.variant_key->variants[index], no_layout};
    dismiss_variants(view);
    const auto old_shift = view.shift;
    apply(view, action);
    if (old_shift != view.shift) refresh(view);
    send_key_input(view.container, &action);
}

void on_variant(lv_event_t* event)
{
    auto& view = *static_cast<KeyboardView*>(lv_event_get_user_data(event));
    const auto index = lv_obj_get_index(lv_event_get_target_obj(event));
    if (index >= 0) select_variant(view, static_cast<std::size_t>(index));
}

lv_obj_t* variant_at_pointer(const KeyboardView& view, lv_indev_t* pointer)
{
    if (!view.variant_row || !pointer || lv_indev_get_type(pointer) != LV_INDEV_TYPE_POINTER) return nullptr;
    lv_point_t point;
    lv_indev_get_point(pointer, &point);
    lv_obj_update_layout(view.variant_row);
    lv_area_t bounds;
    lv_obj_get_coords(view.variant_row, &bounds);
    if (point.x < bounds.x1 || point.x > bounds.x2 || point.y < bounds.y1 || point.y > bounds.y2) return nullptr;
    for (std::uint32_t index = 0; index < lv_obj_get_child_count(view.variant_row); ++index) {
        auto* choice = lv_obj_get_child(view.variant_row, static_cast<std::int32_t>(index));
        if (lv_obj_is_visible(choice) && !lv_obj_has_state(choice, LV_STATE_DISABLED) && lv_obj_hit_test(choice, &point))
            return choice;
    }
    return nullptr;
}

void highlight_variant(KeyboardView& view, lv_obj_t* choice)
{
    if (view.highlighted_variant == choice) return;
    if (view.highlighted_variant) lv_obj_remove_state(view.highlighted_variant, LV_STATE_PRESSED);
    view.highlighted_variant = choice;
    if (choice) lv_obj_add_state(choice, LV_STATE_PRESSED);
}

void on_overlay(lv_event_t* event)
{
    auto& view = *static_cast<KeyboardView*>(lv_event_get_user_data(event));
    if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        end_variant_drag(view);
        view.variant_overlay = nullptr;
        view.variant_row = nullptr;
        view.variant_key = nullptr;
        view.held_button = nullptr;
        if (view.hold_timer) lv_timer_pause(view.hold_timer);
    } else if (lv_event_get_target_obj(event) == view.variant_overlay) {
        dismiss_variants(view);
    }
}

void show_variants(KeyboardView& view, const Slot& slot)
{
    auto* display = lv_obj_get_display(view.container);
    const auto screen_width = lv_display_get_horizontal_resolution(display);
    const auto screen_height = lv_display_get_vertical_resolution(display);
    if (screen_width < 1 || screen_height < 1) return;
    lv_area_t anchor;
    lv_obj_get_coords(slot.button, &anchor);
    const auto padding = std::min<std::int32_t>(4, (std::min(screen_width, screen_height) - 1) / 2);
    const auto button_width = std::min(screen_width - 2 * padding,
                                        std::max<std::int32_t>(36, lv_area_get_width(&anchor)));
    const auto button_height = std::min(screen_height - 2 * padding,
                                         std::max<std::int32_t>(36, lv_area_get_height(&anchor)));
    const bool open_right = anchor.x1 + anchor.x2 < screen_width;
    auto available_width = open_right ? screen_width - anchor.x2 - key_gap - 1 : anchor.x1 - key_gap;
    // Extremely small displays may need to overlap the anchor to remain usable.
    if (available_width < button_width + 2 * padding) available_width = screen_width;
    const auto columns = std::min<std::size_t>(slot.key->variant_count,
        std::max<std::int32_t>(1, (available_width - 2 * padding + key_gap) / (button_width + key_gap)));
    const auto rows = (slot.key->variant_count + columns - 1) / columns;
    const auto row_width = static_cast<std::int32_t>(columns) * (button_width + key_gap) - key_gap + 2 * padding;
    const auto desired_height = static_cast<std::int64_t>(rows) * (button_height + key_gap) - key_gap + 2 * padding;
    const auto row_height = static_cast<std::int32_t>(std::min<std::int64_t>(screen_height, desired_height));
    const auto preferred_left = open_right ? anchor.x2 + key_gap + 1 : anchor.x1 - key_gap - row_width;
    const auto left = std::clamp<std::int32_t>(preferred_left, 0, screen_width - row_width);
    const auto top = std::clamp<std::int32_t>((anchor.y1 + anchor.y2 + 1 - row_height) / 2,
                                0, screen_height - row_height);

    auto* overlay = lv_obj_create(lv_display_get_layer_top(display));
    if (!overlay) return;
    view.variant_overlay = overlay;
    view.variant_key = slot.key;
    lv_obj_remove_style_all(overlay);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, screen_width, screen_height);
    lv_obj_set_scrollable(overlay, false);
    lv_obj_set_click_focusable(overlay, false);
    lv_group_remove_obj(overlay);
    if (!lv_obj_add_event_cb(overlay, on_overlay, LV_EVENT_CLICKED, &view) ||
        !lv_obj_add_event_cb(overlay, on_overlay, LV_EVENT_DELETE, &view)) {
        dismiss_variants(view);
        return;
    }
    auto* row = lv_obj_create(overlay);
    if (!row) { dismiss_variants(view); return; }
    view.variant_row = row;
    lv_obj_remove_style_all(row);
    lv_obj_set_click_focusable(row, false);
    lv_group_remove_obj(row);
    lv_obj_set_pos(row, left, top);
    lv_obj_set_size(row, row_width, row_height);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x4a4a4a), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, view.key_style.radius, 0);
    lv_obj_set_style_pad_all(row, padding, 0);
    lv_obj_set_scroll_dir(row, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_AUTO);
    KeyStyle style = view.key_style;
    style.font = lv_obj_get_style_text_font(slot.button, LV_PART_MAIN);
    for (std::size_t index = 0; index < slot.key->variant_count; ++index) {
        auto* button = create_key(row, slot.key->variants[index], button_width,
                                  button_height, style);
        if (!button || !lv_obj_add_event_cb(button, on_variant, LV_EVENT_CLICKED, &view)) {
            dismiss_variants(view);
            return;
        }
        lv_obj_set_pos(button, static_cast<std::int32_t>(index % columns) * (button_width + key_gap),
                        static_cast<std::int32_t>(index / columns) * (button_height + key_gap));
    }
    view.suppress_click = slot.button;
    view.held_button = nullptr;
    view.drag_source = slot.button;
    view.drag_scroll_chain_hor = lv_obj_is_scroll_chain_hor(slot.button);
    view.drag_scroll_chain_ver = lv_obj_is_scroll_chain_ver(slot.button);
    // Keep the original touch until release, even while it crosses the overlay.
    // Prevent a slide over the options from scrolling an enclosing form.
    lv_obj_set_press_lock(slot.button, true);
    lv_obj_set_scroll_chain_hor(slot.button, false);
    lv_obj_set_scroll_chain_ver(slot.button, false);
    lv_obj_remove_state(slot.button, LV_STATE_PRESSED);
    // Watch ancestors too: the composite keyboard can be hidden by its owner.
    lv_timer_set_period(view.hold_timer, 50);
    lv_timer_reset(view.hold_timer);
    lv_timer_resume(view.hold_timer);
}

void on_hold(lv_timer_t* timer)
{
    auto& view = *static_cast<KeyboardView*>(lv_timer_get_user_data(timer));
    if (!lv_obj_is_visible(view.container)) { dismiss_variants(view); return; }
    if (view.variant_overlay) return;
    auto* target = view.held_button;
    lv_timer_pause(timer);
    const auto slot = std::find_if(view.slots.begin(), view.slots.end(),
        [target](const Slot& value) { return target && value.button == target; });
    if (slot != view.slots.end() && slot->key && slot->key->variant_count &&
        slot->key->tap.type == ActionType::Insert && lv_obj_is_visible(slot->button)) {
        show_variants(view, *slot);
    }
}

void on_press(lv_event_t* event)
{
    auto& view = *static_cast<KeyboardView*>(lv_event_get_user_data(event));
    auto* target = lv_event_get_target_obj(event);
    const auto code = lv_event_get_code(event);
    if (view.drag_source == target && view.variant_overlay) {
        if (code == LV_EVENT_PRESSING) {
            highlight_variant(view, variant_at_pointer(view, lv_event_get_indev(event)));
            return;
        }
        if (code == LV_EVENT_RELEASED) {
            auto* choice = variant_at_pointer(view, lv_event_get_indev(event));
            const auto index = choice ? lv_obj_get_index(choice) : -1;
            end_variant_drag(view);
            if (index >= 0) select_variant(view, static_cast<std::size_t>(index));
            return;
        }
        if (code == LV_EVENT_PRESS_LOST) { dismiss_variants(view); return; }
    }
    if (code == LV_EVENT_PRESSING) return;
    if (code != LV_EVENT_PRESSED) {
        view.held_button = nullptr;
        if (!view.variant_overlay) lv_timer_pause(view.hold_timer);
        return;
    }
    dismiss_variants(view);
    view.suppress_click = nullptr;
    const auto slot = std::find_if(view.slots.begin(), view.slots.end(),
        [target](const Slot& value) { return value.button == target; });
    if (slot == view.slots.end() || !slot->key || slot->key->tap.type != ActionType::Insert) return;
    // Resolve a just-tapped Shift before arming the hold for its uppercase key.
    const auto old_shift = view.shift;
    const auto old_base = view.base_layout;
    const auto* pending = flush_pending(view);
    if (view.shift != old_shift || view.base_layout != old_base) refresh(view);
    if (slot->key && slot->key->variant_count) {
        view.held_button = target;
        lv_timer_set_period(view.hold_timer, variant_hold_ms);
        lv_timer_reset(view.hold_timer);
        lv_timer_resume(view.hold_timer);
    }
    send_key_input(view.container, pending);
}

void on_timeout(lv_timer_t* timer)
{
    auto& view = *static_cast<KeyboardView*>(lv_timer_get_user_data(timer));
    const auto old_shift = view.shift;
    const auto old_base = view.base_layout;
    const auto* action = flush_pending(view);
    if (view.shift != old_shift || view.base_layout != old_base) {
        refresh(view);
    }
    send_key_input(view.container, action);
}

void on_click(lv_event_t* event)
{
    auto& view = *static_cast<KeyboardView*>(lv_event_get_user_data(event));
    auto* target = lv_event_get_target_obj(event);
    if (target == view.suppress_click) {
        view.suppress_click = nullptr;
        return;
    }
    dismiss_variants(view);
    const auto tapped = std::find_if(view.slots.begin(), view.slots.end(),
        [target](const Slot& slot) { return slot.button == target; });
    if (tapped == view.slots.end() || tapped->key == nullptr) {
        return;
    }

    const auto old_shift = view.shift;
    const auto old_base = view.base_layout;
    const char* id = tapped->key->id;
    const Key* pending = view.pending_key;
    const Action* previous_action = nullptr;
    const Action* action = nullptr;
    if (pending != nullptr && std::strcmp(pending->id, id) == 0 &&
        lv_tick_elaps(view.pending_since) < double_tap_ms) {
        clear_pending(view);
        action = &pending->double_tap;
    } else {
        // A quick tap after Shift must use that key's shifted action.
        previous_action = flush_pending(view);
        if (const auto* key = find_key(active_layout(view), id)) {
            if (key->double_tap.type == ActionType::None) {
                action = &key->tap;
            } else {
                view.pending_key = key;
                view.pending_since = lv_tick_get();
                lv_timer_reset(view.tap_timer);
                lv_timer_resume(view.tap_timer);
            }
        }
    }

    if (action != nullptr) apply(view, *action);
    if (view.shift != old_shift || view.base_layout != old_base) {
        refresh(view);
    }
    auto* keyboard = view.container;
    if (send_key_input(keyboard, previous_action) == LV_RESULT_OK) send_key_input(keyboard, action);
}

void on_commit(lv_event_t* event)
{
    auto& view = *static_cast<KeyboardView*>(lv_event_get_user_data(event));
    dismiss_variants(view);
    const auto old_shift = view.shift;
    const auto old_base = view.base_layout;
    const auto* pending = flush_pending(view);
    if (view.shift == ShiftMode::OneShot) view.shift = ShiftMode::Off;
    if (view.shift != old_shift || view.base_layout != old_base) refresh(view);
    send_key_input(view.container, pending);
}

void on_size(lv_event_t* event)
{
    auto& view = *static_cast<KeyboardView*>(lv_event_get_user_data(event));
    refresh(view);
}

void destroy_view(KeyboardView* view)
{
    dismiss_variants(*view);
    if (view->hold_timer) lv_timer_delete(view->hold_timer);
    if (view->tap_timer != nullptr) {
        lv_timer_delete(view->tap_timer);
    }
    view->~KeyboardView();
    lv_free(view);
}

void on_delete(lv_event_t* event)
{
    destroy_view(static_cast<KeyboardView*>(lv_event_get_user_data(event)));
}

KeyboardView* data(lv_obj_t* keyboard)
{
    if (!keyboard) return nullptr;
    for (uint32_t index = 0; index < lv_obj_get_event_count(keyboard); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(keyboard, index);
        if (lv_event_dsc_get_cb(descriptor) == on_delete)
            return static_cast<KeyboardView*>(lv_event_dsc_get_user_data(descriptor));
    }
    return nullptr;
}

}  // namespace

lv_obj_t* create_en_us_keyboard(lv_obj_t* parent, std::int32_t width, std::int32_t height,
                                const KeyStyle& style)
{
    if (parent == nullptr || width <= 0 || height <= 0 || width > LV_COORD_MAX ||
        height > LV_COORD_MAX || style.radius < 0) {
        return nullptr;
    }

    lv_obj_t* container = lv_obj_create(parent);
    if (container == nullptr) {
        return nullptr;
    }
    void* storage = lv_malloc(sizeof(KeyboardView));
    if (storage == nullptr) {
        lv_obj_delete(container);
        return nullptr;
    }
    auto* view = new (storage) KeyboardView{};
    view->container = container;
    view->authored_font = style.font;
    view->key_style = style;
    if (lv_obj_add_event_cb(container, on_delete, LV_EVENT_DELETE, view) == nullptr) {
        destroy_view(view);
        lv_obj_delete(container);
        return nullptr;
    }

    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, width, height);
    lv_obj_set_scrollable(container, false);
    lv_obj_set_clickable(container, false);

    view->tap_timer = lv_timer_create(on_timeout, double_tap_ms, view);
    if (view->tap_timer == nullptr) {
        lv_obj_delete(container);
        return nullptr;
    }
    lv_timer_pause(view->tap_timer);
    view->hold_timer = lv_timer_create(on_hold, variant_hold_ms, view);
    if (!view->hold_timer) { lv_obj_delete(container); return nullptr; }
    lv_timer_pause(view->hold_timer);

    KeyStyle keyboard_style = style;
    if (keyboard_style.font == nullptr) {
        keyboard_style.font = &espos_keyboard_font;
    }
    for (auto& slot : view->slots) {
        slot.button = create_key(container, "", 1, 1, keyboard_style);
        if (slot.button == nullptr ||
            !lv_obj_add_event_cb(slot.button, on_click, LV_EVENT_CLICKED, view) ||
            !lv_obj_add_event_cb(slot.button, on_press, LV_EVENT_PRESSED, view) ||
            !lv_obj_add_event_cb(slot.button, on_press, LV_EVENT_PRESSING, view) ||
            !lv_obj_add_event_cb(slot.button, on_press, LV_EVENT_RELEASED, view) ||
            !lv_obj_add_event_cb(slot.button, on_press, LV_EVENT_PRESS_LOST, view)) {
            lv_obj_delete(container);
            return nullptr;
        }
        slot.label = lv_obj_get_child(slot.button, 0);
    }
    if (lv_obj_add_event_cb(container, on_size, LV_EVENT_SIZE_CHANGED, view) == nullptr ||
        lv_obj_add_event_cb(container, on_commit, LV_EVENT_READY, view) == nullptr) {
        lv_obj_delete(container);
        return nullptr;
    }
    refresh(*view, width, height);
    return container;
}

void commit_en_us_keyboard_text(lv_obj_t* keyboard)
{
    if (keyboard != nullptr) lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr);
}

bool set_keyboard_language(lv_obj_t* keyboard, const char* id)
{
    auto* view = data(keyboard);
    if (!view || !id) return false;
    for (const auto& language : languages) {
        if (std::strcmp(language.id, id) != 0) continue;
        if (view->language == &language) return true;
        clear_pending(*view);
        view->language = &language;
        view->base_layout = language.definition->initial_layout;
        view->shift = ShiftMode::Off;
        if (!view->authored_font) for (auto& slot : view->slots) lv_obj_set_style_text_font(slot.button, keyboard_language_font(id), LV_PART_MAIN);
        refresh(*view);
        return true;
    }
    return false;
}

const char* keyboard_language(lv_obj_t* keyboard)
{
    const auto* view = data(keyboard);
    return view ? view->language->id : "";
}

std::size_t keyboard_language_count() { return sizeof(languages) / sizeof(languages[0]); }
const char* keyboard_language_id(std::size_t index)
{
    return index < keyboard_language_count() ? languages[index].id : nullptr;
}
const char* keyboard_language_name(std::size_t index)
{
    return index < keyboard_language_count() ? languages[index].name : nullptr;
}

const lv_font_t* keyboard_language_font(const char* id)
{
    if (!id) return &espos_keyboard_font;
#if LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
    if (!std::strcmp(id, "zh-CN")) { static const auto font = [] { auto value = lv_font_source_han_sans_sc_16_cjk; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
#if LV_FONT_DEJAVU_16_PERSIAN_HEBREW
    if (!std::strcmp(id, "ar-SA")) { static const auto font = [] { auto value = lv_font_dejavu_16_persian_hebrew; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
#if LV_ADVANCED_KEYBOARD_USE_HINDI_FONT
    if (!std::strcmp(id, "hi-IN")) { static const auto font = [] { auto value = espos_keyboard_hindi_font; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
#if LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT
    if (!std::strcmp(id, "bn-BD")) { static const auto font = [] { auto value = espos_keyboard_bengali_font; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
#if LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT
    if (!std::strcmp(id, "ru-RU")) { static const auto font = [] { auto value = espos_keyboard_cyrillic_font; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
#if LV_ADVANCED_KEYBOARD_USE_URDU_FONT
    if (!std::strcmp(id, "ur-PK")) { static const auto font = [] { auto value = espos_keyboard_urdu_font; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
#if LV_ADVANCED_KEYBOARD_USE_GREEK_FONT
    if (!std::strcmp(id, "el-GR")) { static const auto font = [] { auto value = espos_keyboard_greek_font; value.fallback = &espos_keyboard_font; return value; }(); return &font; }
#endif
    return &espos_keyboard_font;
}
bool keyboard_language_right_to_left(const char* id) { return id && (!std::strcmp(id, "ar-SA") || !std::strcmp(id, "ur-PK")); }

}  // namespace espos::keyboard
