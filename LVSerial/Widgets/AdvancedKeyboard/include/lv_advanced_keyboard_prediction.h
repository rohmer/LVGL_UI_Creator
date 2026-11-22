#ifndef LV_ADVANCED_KEYBOARD_PREDICTION_H
#define LV_ADVANCED_KEYBOARD_PREDICTION_H
#include "lv_advanced_keyboard.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_advanced_keyboard_predictor lv_advanced_keyboard_predictor_t;

/* Models are immutable compact NGTR v3 with NGIX v1 indices. Input byte arrays
 * are borrowed without alignment restrictions and must outlive the predictor.
 * create() builds and owns an index; create_borrowed() also borrows the index,
 * allowing both arrays to remain in memory-mapped flash. ESP32 (including
 * Arduino ESP32) prefers a shared PSRAM copy of both arrays when available;
 * unavailable or exhausted PSRAM falls back to the borrowed arrays. Keep the
 * input lifetime contract on every target. Invalid data and
 * allocation failures return NULL. No inference call allocates heap storage.
 * One instance is required per concurrent caller. */
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_predictor_create(const void* trie, size_t trie_size);
/* Small bundled English model; shared PSRAM bytes on ESP32 when available. */
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_predictor_create_builtin(void);
/* Borrows the same static default-model arrays, or returns false when the
 * optional builtin model was omitted from this firmware profile. */
bool lv_advanced_keyboard_get_builtin_trie(const void** trie, size_t* trie_size, const void** index, size_t* index_size);
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_predictor_create_borrowed(
    const void* trie, size_t trie_size, const void* index, size_t index_size);
void lv_advanced_keyboard_predictor_destroy(lv_advanced_keyboard_predictor_t* predictor);
/* Full format/structure validation with no allocations or retained pointers. */
bool lv_advanced_keyboard_validate_trie(const void* trie, size_t trie_size, const void* index, size_t index_size);
size_t lv_advanced_keyboard_predictor_workspace_bytes(void);
uint32_t lv_advanced_keyboard_predictor_node_count(const lv_advanced_keyboard_predictor_t* predictor);

/* Produces an owned NGIX index. On failure outputs are reset to NULL/zero.
 * Temporary index construction uses four bytes per trie node and prefers PSRAM
 * on ESP32, with the normal heap as fallback. Free a returned
 * index with free_trie_index(), after every borrowing predictor is destroyed.
 * Firmware exporters can generate the same bytes using layout/build_ngtr_index.py. */
bool lv_advanced_keyboard_build_trie_index(const void* trie, size_t trie_size, void** index, size_t* index_size);
void lv_advanced_keyboard_free_trie_index(void* index);

/* Token views use explicit byte counts and need not be NUL terminated. Prefix
 * and context must already be Unicode folded. Results retain trie vocabulary
 * case; the typing view applies the target's capitalization when displaying. */
size_t lv_advanced_keyboard_predictor_suggest_folded(lv_advanced_keyboard_predictor_t* predictor,
    const char* prefix, size_t prefix_size, const char* previous, size_t previous_size,
    const char* older, size_t older_size, lv_advanced_keyboard_suggestion_t* output, size_t capacity);

/* Folds UTF-8 tokens, scores keyboard-weighted edits and context, and returns a
 * changed word only when its probability meets threshold (0..1). Numeric and
 * protected calendar/number words are kept. Output and probability are reset
 * when no correction is accepted. */
bool lv_advanced_keyboard_predictor_autocorrect_utf8(lv_advanced_keyboard_predictor_t* predictor,
    const char* typed, size_t typed_size, const char* previous, size_t previous_size,
    const char* older, size_t older_size, double threshold,
    lv_advanced_keyboard_suggestion_t* output, double* probability);

#ifdef __cplusplus
}
#endif
#endif
