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

extern "C" {
LV_FONT_DECLARE(espos_keyboard_font)
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
};
constexpr std::size_t max_keys = (std::max)({detail::en_us_qwerty.max_keys, detail::es_es_qwerty.max_keys,
    detail::fr_fr_azerty.max_keys, detail::de_de_qwertz.max_keys,
    detail::it_it_qwerty.max_keys, detail::pt_pt_qwerty.max_keys});

constexpr std::uint32_t double_tap_ms = 300;
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
    const Key* pending_key = nullptr;
    std::uint32_t pending_since = 0;
    std::size_t base_layout = en_us_qwerty.initial_layout;
    const Language* language = &languages[0];
    ShiftMode shift = ShiftMode::Off;
    std::array<Slot, max_keys> slots{};
};

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

    KeyStyle keyboard_style = style;
    if (keyboard_style.font == nullptr) {
        keyboard_style.font = &espos_keyboard_font;
    }
    for (auto& slot : view->slots) {
        slot.button = create_key(container, "", 1, 1, keyboard_style);
        if (slot.button == nullptr || lv_obj_add_event_cb(slot.button, on_click, LV_EVENT_CLICKED, view) == nullptr) {
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

}  // namespace espos::keyboard
