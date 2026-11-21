#ifndef LV_ADVANCED_KEYBOARD_H
#define LV_ADVANCED_KEYBOARD_H
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#define LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION 1
#endif
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
#define LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Portable ESPOS mobile EN-US keyboard with three optional suggestions.
 * Textareas are external, borrowed objects; their content and styles remain
 * independent. The keyboard starts hidden and appears when a bound field is
 * focused or clicked. Keys retain the field focus. No display rotation or ESP driver is installed.
 * Calls run on the LVGL thread. The parent owns the returned object. */
lv_obj_t* lv_advanced_keyboard_create(lv_obj_t* parent);
bool lv_advanced_keyboard_is_instance(lv_obj_t* object);
lv_obj_t* lv_advanced_keyboard_get_textarea(lv_obj_t* object);
lv_obj_t* lv_advanced_keyboard_get_keyboard(lv_obj_t* object);
/* Languages select the key layout, separately from the prediction trie.
 * The default is en-US. Switching preserves target/text and resets Shift/pages.
 * Unsupported IDs fail without changing the keyboard. Metadata is static storage;
 * out-of-range metadata indices return NULL. Calls use the LVGL thread. */
bool lv_advanced_keyboard_set_language(lv_obj_t* object, const char* language);
const char* lv_advanced_keyboard_get_language(lv_obj_t* object);
size_t lv_advanced_keyboard_language_count(void);
const char* lv_advanced_keyboard_language_id(size_t index);
const char* lv_advanced_keyboard_language_name(size_t index);
/* Bind watches focus without selecting or modifying the field. Set selects an
 * external textarea immediately (NULL clears and hides). Standard LVGL
 * textareas and the native Entry of a Creator Text box are accepted. Multiple
 * bindings let one keyboard follow several fields. Both deletion orders are safe. */
bool lv_advanced_keyboard_bind_textarea(lv_obj_t* object, lv_obj_t* textarea);
bool lv_advanced_keyboard_set_textarea(lv_obj_t* object, lv_obj_t* textarea);
/* Enter commits autocorrect and emits READY on the external field and keyboard;
 * X emits CANCEL on the keyboard only. Both clear the active target and hide
 * before callbacks, keeping field watchers for the next click/focus.
 * Programmatic close performs the same dismissal without an event. */
void lv_advanced_keyboard_close(lv_obj_t* object);
bool lv_advanced_keyboard_is_dismissed(lv_obj_t* object);
void lv_advanced_keyboard_unbind_textarea(lv_obj_t* object, lv_obj_t* textarea);
void lv_advanced_keyboard_unbind_all(lv_obj_t* object);
size_t lv_advanced_keyboard_bound_textarea_count(lv_obj_t* object);
lv_obj_t* lv_advanced_keyboard_bound_textarea(lv_obj_t* object, size_t index);
void lv_advanced_keyboard_set_hide_on_defocus(lv_obj_t* object, bool enabled);
bool lv_advanced_keyboard_get_hide_on_defocus(lv_obj_t* object);
typedef struct lv_advanced_keyboard_predictor lv_advanced_keyboard_predictor_t;
/* A custom predictor is borrowed and must outlive this attachment. Passing
 * NULL disables prediction. A normal full build starts with shared built-in
 * English model bytes and a private inference workspace. */
void lv_advanced_keyboard_set_predictor(lv_obj_t* object, lv_advanced_keyboard_predictor_t* predictor);
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_get_predictor(lv_obj_t* object);
void lv_advanced_keyboard_set_autocorrect(lv_obj_t* object, bool enabled);
bool lv_advanced_keyboard_get_autocorrect(lv_obj_t* object);
void lv_advanced_keyboard_set_autocorrect_threshold(lv_obj_t* object, double threshold);
double lv_advanced_keyboard_get_autocorrect_threshold(lv_obj_t* object);
/* These convenience operations affect only the current external target. */
void lv_advanced_keyboard_set_text(lv_obj_t* object, const char* text);
const char* lv_advanced_keyboard_get_text(lv_obj_t* object);
void lv_advanced_keyboard_set_placeholder(lv_obj_t* object, const char* text);
const char* lv_advanced_keyboard_get_placeholder(lv_obj_t* object);
void lv_advanced_keyboard_set_password(lv_obj_t* object, bool enabled);
bool lv_advanced_keyboard_get_password(lv_obj_t* object);
/* Zero is unlimited. A shorter nonzero limit truncates existing text at a
 * UTF-8 character boundary and emits VALUE_CHANGED if the content changes. */
void lv_advanced_keyboard_set_max_length(lv_obj_t* object, uint32_t length);
uint32_t lv_advanced_keyboard_get_max_length(lv_obj_t* object);
void lv_advanced_keyboard_set_key_background(lv_obj_t* object, lv_color_t color);
lv_color_t lv_advanced_keyboard_get_key_background(lv_obj_t* object);
void lv_advanced_keyboard_set_key_foreground(lv_obj_t* object, lv_color_t color);
lv_color_t lv_advanced_keyboard_get_key_foreground(lv_obj_t* object);
void lv_advanced_keyboard_set_key_radius(lv_obj_t* object, int32_t radius);
int32_t lv_advanced_keyboard_get_key_radius(lv_obj_t* object);

enum { LV_ADVANCED_KEYBOARD_SUGGESTION_COUNT = 3, LV_ADVANCED_KEYBOARD_SUGGESTION_BYTES = 256 };
typedef struct {
    char text[LV_ADVANCED_KEYBOARD_SUGGESTION_BYTES];
    uint32_t mass;
    uint16_t length;
} lv_advanced_keyboard_suggestion_t;
/* The inputs are borrowed folded UTF-8 views (not NUL-terminated). Fill up to
 * capacity suggestions in rank order and return their count. The callback must
 * not throw, delete the widget, or edit LVGL objects. It is optional; no callback
 * leaves empty suggestion slots inactive. Context must outlive the widget.
 * Set each output length to its UTF-8 byte count (at most 256); a terminating
 * NUL is unnecessary. Invalid UTF-8 and embedded NUL bytes disable that slot. */
typedef size_t (*lv_advanced_keyboard_lookup_t)(void* context,
    const char* prefix, size_t prefix_length, const char* previous, size_t previous_length,
    const char* older, size_t older_length, lv_advanced_keyboard_suggestion_t* output, size_t capacity);
void lv_advanced_keyboard_set_suggestion_lookup(lv_obj_t* object, lv_advanced_keyboard_lookup_t lookup, void* context);

#ifdef __cplusplus
}
#endif
#endif
