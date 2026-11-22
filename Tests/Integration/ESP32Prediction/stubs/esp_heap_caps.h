#pragma once
#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_8BIT (1U << 2)
#define MALLOC_CAP_SPIRAM (1U << 10)
#define MALLOC_CAP_INTERNAL (1U << 11)

#ifdef __cplusplus
extern "C" {
#endif
void* heap_caps_malloc(size_t size, uint32_t caps);
void* heap_caps_realloc(void* memory, size_t size, uint32_t caps);
void heap_caps_free(void* memory);
size_t heap_caps_get_total_size(uint32_t caps);
size_t heap_caps_get_free_size(uint32_t caps);
#ifdef __cplusplus
}
#endif
