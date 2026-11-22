/* Intercept only generated-code allocations. LVGL itself keeps its real
 * allocator, so failures and byte counts below isolate variable ownership. */
#include "ui.h"
#include "ui_requirements.h"
char *ui_test_strdup(const char *text);
void *ui_test_realloc(void *memory, size_t size);
void ui_test_free(void *memory);
#undef lv_strdup
#undef lv_realloc
#undef lv_free
#define lv_strdup ui_test_strdup
#define lv_realloc ui_test_realloc
#define lv_free ui_test_free
#include "ui.c"
