#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "ui.h"
#include "fixture_ids.h"
#include "Allocator.h"
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

static double seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
#endif
}
static double median(double *values, size_t count)
{
    size_t i, j;
    for(i = 0; i < count; ++i) for(j = i + 1; j < count; ++j)
        if(values[j] < values[i]) { double swap = values[j]; values[j] = values[i]; values[i] = swap; }
    return values[count / 2];
}
int main(void)
{
    enum { samples = 5, lookups = 500000, updates = 100000 };
    double lookup_time[samples], update_time[samples];
    benchmark_memory_t start, loaded, history;
    lv_display_t *display;
    volatile uintptr_t sink = 0;
    size_t run, iteration;
    lv_init(); display = lv_display_create(800, 480);
    if(!display) return 2;
    start = benchmark_memory(); benchmark_reset_peak();
    if(!ui_init()) return 3;
    lv_obj_update_layout(lv_screen_active());
    for(iteration = 0; iteration < sizeof fixture_ids / sizeof fixture_ids[0]; ++iteration) {
        const size_t per_screen = sizeof fixture_ids / sizeof fixture_ids[0] / 2;
        if(ui_get_object(fixture_ids[iteration].screen, fixture_ids[iteration].id) !=
           lv_obj_get_child(ui_get_screen(fixture_ids[iteration].screen), (int32_t)(iteration % per_screen))) return 6;
    }
    if(ui_get_object(0, -1) || ui_get_object(2, 3) || ui_get_object(0, 4)) return 7;
    loaded = benchmark_memory();
    for(run = 0; run < samples; ++run) {
        double begin = seconds();
        for(iteration = 0; iteration < lookups; ++iteration) {
            const size_t index = (iteration * 131U) % (sizeof fixture_ids / sizeof fixture_ids[0]);
            sink ^= (uintptr_t)ui_get_object(fixture_ids[index].screen, fixture_ids[index].id);
        }
        lookup_time[run] = seconds() - begin;
        begin = seconds();
        for(iteration = 0; iteration < updates; ++iteration)
            if(!ui_set_v_sample((int64_t)(iteration & 255U))) return 4;
        update_time[run] = seconds() - begin;
    }
    history = benchmark_memory();
    printf("{\"ui_live_bytes\":%zu,\"ui_peak_bytes\":%zu,\"ui_allocations\":%zu,"
           "\"ui_reallocations\":%zu,\"history_live_bytes\":%zu,"
           "\"lookup_ns_median\":%.3f,\"history_update_ns_median\":%.3f,\"sink\":%llu}\n",
           loaded.live - start.live, loaded.peak - start.live, loaded.allocations, loaded.reallocations,
           history.live - loaded.live, median(lookup_time, samples) * 1e9 / lookups,
           median(update_time, samples) * 1e9 / updates, (unsigned long long)sink);
    {
        lv_obj_t *deleted = ui_get_object(1, 3);
        lv_obj_delete(deleted);
        if(ui_get_object(1, 3)) return 8;
    }
    ui_deinit();
    if(ui_get_object(0, 3) || ui_get_object(1, 10000)) return 9;
    lv_display_delete(display); lv_deinit();
    if(benchmark_memory().live) return 5;
    return 0;
}
