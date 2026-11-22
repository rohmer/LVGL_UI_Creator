/* Replace only LVGL's CLIB allocator backend, leaving its public allocator and
 * allocation sizes unchanged. The tracking header is excluded from metrics. */
#include <lvgl.h>
#include "Allocator.h"
#include <stddef.h>
#include <stdlib.h>

#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6 || LV_USE_STDLIB_MALLOC != LV_STDLIB_CLIB
#error This benchmark requires the CLIB LVGL 9.6 test archive
#endif

typedef union {
#if defined(_MSC_VER)
    /* MSVC's C11 headers do not expose max_align_t. Its CRT malloc aligns to
     * 16 bytes on x64 (8 on x86); a 16-byte header preserves both alignments. */
    unsigned char alignment[16];
#else
    max_align_t alignment;
#endif
    struct { size_t size; } allocation;
} header_t;
static benchmark_memory_t memory;

static void update_peak(void) { if(memory.live > memory.peak) memory.peak = memory.live; }
benchmark_memory_t benchmark_memory(void) { return memory; }
void benchmark_reset_peak(void) { memory.peak = memory.live; memory.allocations = memory.reallocations = 0; }
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *mem, size_t size) { (void)mem; (void)size; return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) { (void)pool; }
void *lv_malloc_core(size_t size)
{
    header_t *header;
    if(size > SIZE_MAX - sizeof *header) return NULL;
    header = (header_t *)malloc(sizeof *header + size);
    if(!header) return NULL;
    header->allocation.size = size;
    memory.live += size; ++memory.allocations; update_peak();
    return header + 1;
}
void lv_free_core(void *pointer)
{
    header_t *header;
    if(!pointer) return;
    header = (header_t *)pointer - 1;
    memory.live -= header->allocation.size;
    free(header);
}
void *lv_realloc_core(void *pointer, size_t size)
{
    header_t *header, *resized;
    size_t previous;
    if(!pointer) return lv_malloc_core(size);
    if(!size) { lv_free_core(pointer); return NULL; }
    if(size > SIZE_MAX - sizeof *header) return NULL;
    header = (header_t *)pointer - 1;
    previous = header->allocation.size;
    resized = (header_t *)realloc(header, sizeof *header + size);
    if(!resized) return NULL;
    resized->allocation.size = size;
    memory.live = memory.live - previous + size; ++memory.reallocations; update_peak();
    return resized + 1;
}
void lv_mem_monitor_core(lv_mem_monitor_t *monitor) { (void)monitor; }
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
