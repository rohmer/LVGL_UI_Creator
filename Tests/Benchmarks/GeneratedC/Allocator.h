#pragma once
#include <stddef.h>

typedef struct {
    size_t live, peak, allocations, reallocations;
} benchmark_memory_t;
benchmark_memory_t benchmark_memory(void);
void benchmark_reset_peak(void);
